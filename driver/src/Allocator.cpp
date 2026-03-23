#include "Allocator.h"
#include "DeviceImpl.h"
#include "llvm/ADT/ArrayRef.h"
#include <cassert>
#include <limits>
#include <unordered_set>

namespace pimsim::driver {

bool Allocator::allocateRowGroups(size_t groupCnt, pimsim_row_group_t *groups,
                                  pimsim_module_allocate_policy_t mdPolicy,
                                  pimsim_ch_allocate_policy_t chPolicy) {
  auto moduleSize = modules.size();

  llvm::SmallVector<AllocatedRowGroup> allocated;
  auto leftCnt = groupCnt;

  switch (mdPolicy) {
  case PIMSIM_PREFER_ON_ONE_MODULE: {
    std::unordered_set<Module *> checked;

    while (leftCnt > 0) {
      Module *moduleCh = findMinAllocatedModule(checked);
      if (!moduleCh)
        break;
      checked.insert(moduleCh);

      auto allocSuccessSize =
          moduleCh->allocateRowGroups(leftCnt, allocated, chPolicy);

      leftCnt -= allocSuccessSize;
    }
    break;
  }
  case PIMSIM_PREFER_BALANCED_MODULES: {
    size_t extraSize = 0;
    llvm::SmallVector<llvm::SmallVector<AllocatedRowGroup>>
        allocatedRowsForShuffle;
    allocatedRowsForShuffle.resize(moduleSize);

    for (auto &module : modules) {
      size_t moduleAllocSize =
          groupCnt / moduleSize +
          (groupCnt % moduleSize < module->getModuleIdx()) + extraSize;

      auto allocSuccessSize = module->allocateRowGroups(
          moduleAllocSize, allocatedRowsForShuffle[module->getModuleIdx()],
          chPolicy);
      assert(allocSuccessSize <= moduleAllocSize);
      extraSize = moduleAllocSize - allocSuccessSize;
      leftCnt -= allocSuccessSize;
    }

    if (leftCnt > 0)
      break;

    // shuffle allocated row groups

    llvm::SmallVector<size_t> cursors;
    cursors.resize(moduleSize, 0);
    std::set<std::pair<size_t, llvm::SmallVector<AllocatedRowGroup> *>>
        availableModule;
    for (size_t idx = 0; idx < moduleSize; ++idx) {
      availableModule.insert({idx, &allocatedRowsForShuffle[idx]});
    }

    while (!availableModule.empty()) {
      std::vector<decltype(availableModule)::const_iterator> emptyModule;

      for (auto i = availableModule.begin(), e = availableModule.end(); i != e;
           ++i) {
        auto [moduleIdx, moduleResult] = *i;
        auto &moduleCursor = cursors[moduleIdx];
        if (moduleCursor >= moduleResult->size()) {
          emptyModule.push_back(i);
          continue;
        }

        allocated.emplace_back((*moduleResult)[moduleCursor++]);
      }

      for (auto it : emptyModule) {
        availableModule.erase(it);
      }
    }
  }
  default:
    break;
  }

  if (leftCnt > 0)
    return true;

  assert(allocated.size() == groupCnt);
  for (size_t idx = 0; idx < groupCnt; ++idx) {
    createRowGroupImpl(allocated[idx], groups[idx]);
  }
  return false;
}

void Allocator::createRowGroupImpl(AllocatedRowGroup &rg,
                                   pimsim_row_group_t &result) {
  auto moduleIdx = rg.module->getModuleIdx();
  auto chIdx = rg.channel->getChIdx();
  auto *config = rg.module->getConfig();
  auto banks = config->banks;

  result.row_cnt = banks;
  result.channel_addr = device->controller->encodeAddress(moduleIdx, chIdx, -1,
                                                          -1, -1, rg.row, -1);

  result.row_addrs = new size_t[banks];

  for (auto ba = 0; ba < config->banks; ++ba) {
    auto thisBank = ba % config->banks_per_group;
    auto bg = ba / config->banks_per_group;
    auto rk = bg / config->bankgroups;
    bg = bg % config->bankgroups;
    assert(rk < config->ranks && bg < config->bankgroups &&
           thisBank < config->banks_per_group &&
           "Calculated rank, bank group, or bank index is out of bounds");

    auto rowAddr = device->controller->encodeAddress(moduleIdx, chIdx, rk, bg,
                                                     thisBank, rg.row, 0);
    result.row_addrs[ba] = rowAddr;
  }
}

void Allocator::freeRowGroup(pimsim_row_group_t *groups) {
  auto [memIdx, addr] =
      device->controller->decodeAddressWithMemoryIndex(groups->channel_addr);

  assert(memIdx < modules.size() &&
         "Invalid module index in row group address");

  auto *module = modules[memIdx].get();
  module->freeRowGroup(addr.channel, addr.row);
  delete[] groups->row_addrs;
}

Module *
Allocator::findMinAllocatedModule(const std::unordered_set<Module *> &checked) {
  size_t allocatedSize = std::numeric_limits<size_t>::max();
  Module *minModule = nullptr;
  for (const auto &module : modules) {
    if (!checked.contains(module.get()) &&
        allocatedSize > module->allocatedSize) {
      minModule = module.get();
    }
  }
  return minModule;
}

Module::Module(size_t moduleIdx, Allocator *allocator,
               const dramsim3::Config *config)
    : moduleIdx(moduleIdx), allocator(allocator), config(config) {
  for (size_t ch = 0; ch < config->channels; ++ch) {
    channels.emplace_back(std::make_unique<Channel>(ch, this));
  }
}

size_t
Module::allocateRowGroups(size_t groupCnt,
                          llvm::SmallVectorImpl<AllocatedRowGroup> &allocated,
                          pimsim_ch_allocate_policy_t policy) {

  size_t leftCnt = groupCnt;
  switch (policy) {
  case PIMSIM_PREFER_ON_ONE_CHANNEL: {

    std::unordered_set<Channel *> checked;
    while (leftCnt > 0) {
      Channel *minCh = findMinAllocatedCh(checked);
      if (!minCh)
        break;
      checked.insert(minCh);

      size_t rowHint = -1;
      while (leftCnt > 0 && minCh->allocSize() < config->rows) {
        size_t allocatedRow;
        if (minCh->allocateRowGroup(&allocatedRow, rowHint))
          break;

        leftCnt--;
        rowHint = allocatedRow + 1;
        allocated.emplace_back(this, minCh, allocatedRow);
      }
    }
    break;
  }
  case PIMSIM_PREFER_BALANCED_CHANNELS: {
    std::set<std::pair<size_t, Channel *>> availableCH;
    for (auto &ch : channels) {
      availableCH.insert({ch->getChIdx(), ch.get()});
    }

    while (!availableCH.empty() && leftCnt > 0) {
      std::vector<decltype(availableCH)::const_iterator> failed;

      for (auto i = availableCH.begin(), e = availableCH.end(); i != e; ++i) {
        auto [_, ch] = *i;
        size_t allocatedRow;
        if (ch->allocateRowGroup(&allocatedRow)) {
          failed.push_back(i);
          continue;
        }

        leftCnt--;
        allocated.emplace_back(this, ch, allocatedRow);
        if (leftCnt == 0)
          break;
      }

      for (auto it : failed) {
        availableCH.erase(it);
      }
    }
    break;
  }
  default:
    break;
  }

  size_t allocSize = groupCnt - leftCnt;
  allocatedSize += allocSize;
  return allocSize;
}

void Module::freeRowGroup(size_t chIdx, size_t row) {
  assert(chIdx < channels.size() && "Invalid channel index in freeRowGroup");
  channels[chIdx]->freeRowGroup(row);
  allocatedSize--;
}

Channel *
Module::findMinAllocatedCh(const std::unordered_set<Channel *> &checked) {
  size_t allocatedSize = std::numeric_limits<size_t>::max();
  Channel *minCh = nullptr;
  for (const auto &ch : channels) {
    if (!checked.contains(ch.get()) && allocatedSize > ch->allocSize()) {
      minCh = ch.get();
    }
  }
  return minCh;
}

bool Channel::allocateRowGroup(size_t *allocatedRow, size_t rowHint) {
  if (allocatedRowGroup.size() == parentModule->getConfig()->rows)
    return true; // full

  if (rowHint == -1) {
    rowHint = allocatedRowGroup.empty() ? 0 : *allocatedRowGroup.rbegin();
  }

  while (rowHint < parentModule->getConfig()->rows) {
    if (auto [it, inserted] = allocatedRowGroup.insert(rowHint); inserted) {
      *allocatedRow = rowHint;
      return false;
    }

    rowHint++;
  }
  if (allocatedRowGroup.size() < parentModule->getConfig()->rows) {
    return allocateRowGroup(allocatedRow, 0);
  }
  return true;
}

void Channel::freeRowGroup(size_t row) {
  if (auto it = allocatedRowGroup.find(row); it != allocatedRowGroup.end()) {
    allocatedRowGroup.erase(it);
  } else {
    assert(false && "double free detected");
  }
}

} // namespace pimsim::driver
