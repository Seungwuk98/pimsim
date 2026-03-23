#ifndef PIMSIM_DRIVER_ALLOCATOR_H
#define PIMSIM_DRIVER_ALLOCATOR_H

#include <bitset>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "API.h"
#include "configuration.h"
#include "llvm/ADT/SmallVector.h"

namespace pimsim::driver {

class Channel;
class Module;

struct AllocatedRowGroup {
  Module *module;
  Channel *channel;
  size_t row;
};

class Allocator {
public:
  Allocator(pimsim_memory_device_t *device) : device(device) {}

  bool allocateRowGroups(size_t groupCnt, pimsim_row_group_t *groups,
                         pimsim_module_allocate_policy_t mdPolicy,
                         pimsim_ch_allocate_policy_t chPolicy);

  void freeRowGroup(pimsim_row_group_t *group);

  void pushModule(std::unique_ptr<Module> module) {
    modules.push_back(std::move(module));
  }

private:
  Module *findMinAllocatedModule(const std::unordered_set<Module *> &checked);

  void createRowGroupImpl(AllocatedRowGroup &rg, pimsim_row_group_t &result);

  friend class Channel;
  friend class Module;
  pimsim_memory_device_t *device;
  std::vector<std::unique_ptr<Module>> modules;
};

class Module {
public:
  Module(size_t moduleIdx, Allocator *allocator,
         const dramsim3::Config *config);

  const dramsim3::Config *getConfig() { return config; }
  size_t allocateRowGroups(size_t groupCnt,
                           llvm::SmallVectorImpl<AllocatedRowGroup> &allocated,
                           pimsim_ch_allocate_policy_t chPolicy);

  void freeRowGroup(size_t chIdx, size_t row);

  size_t getModuleIdx() const { return moduleIdx; }

private:
  friend class Channel;
  friend class Allocator;
  Channel *findMinAllocatedCh(const std::unordered_set<Channel *> &checked);

  size_t moduleIdx;
  Allocator *allocator;
  const dramsim3::Config *config;
  std::vector<std::unique_ptr<Channel>> channels;
  size_t allocatedSize = 0;
};

class Channel {
public:
  Channel(size_t chIdx, Module *parentModule)
      : chIdx(chIdx), parentModule(parentModule) {}

  bool allocateRowGroup(size_t *allocatedRow, size_t rowHint = -1);
  void freeRowGroup(size_t row);

  size_t allocSize() const { return allocatedRowGroup.size(); }

  size_t getChIdx() const { return chIdx; }

private:
  size_t chIdx;
  Module *parentModule;
  std::set<size_t> allocatedRowGroup;
};

} // namespace pimsim::driver

#endif // PIMSIM_DRIVER_ALLOCATOR_H
