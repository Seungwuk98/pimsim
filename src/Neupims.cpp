#include "pimsim/Neupims.h"
#include "llvm/ADT/ArrayRef.h"
#include <bit>

namespace pimsim {

NeupimsChannel::NeupimsChannel(Context *ctx,
                               std::vector<std::unique_ptr<Rank>> &&ranks)
    : PIMChannel(TypeID<NeupimsChannel>, ctx, std::move(ranks)) {}

llvm::SmallVector<f16> NeupimsChannel::readResult() const {
  llvm::SmallVector<f16> result;
  size_t numElements = headerLength();
  result.reserve(numElements);

  auto header = pimHeaderBits;
  const auto &config = getParentMemory()->getConfig();
  for (size_t r = 0; r < config.ranks; ++r) {
    auto rank = ranks[r].get();
    for (size_t bg = 0; bg < config.bankgroups; ++bg) {
      auto bankGroup = rank->getBankGroup(bg);
      for (size_t b = 0; b < config.banks; ++b) {
        if (header & 1) {
          NewtonBank *bank = llvm::cast<NewtonBank>(bankGroup->getBank(b));
          result.emplace_back(bank->getAddResult());
        }
        header >>= 1;
      }
    }
  }
  return result;
}

void NeupimsChannel::comp() {
  assert(headerLength() > 0 &&
         "PIM header must have at least one bit set for computation");
  const auto &config = getParentMemory()->getConfig();
  auto header = pimHeaderBits;
  for (size_t r = 0; r < config.ranks; ++r) {
    auto rank = ranks[r].get();
    for (size_t bg = 0; bg < config.bankgroups; ++bg) {
      auto bankGroup = rank->getBankGroup(bg);
      for (size_t b = 0; b < config.banks; ++b) {
        if (header & 1) {
          NeupimsDualRBBank *bank =
              llvm::cast<NeupimsDualRBBank>(bankGroup->getBank(b));
          bank->comp();
        }
        header >>= 1;
      }
    }
  }
}

bool NeupimsChannel::classof(const Channel *channel) {
  return TypeID<NeupimsChannel> == channel->getTypeID();
}

NeupimsDualRBBank::NeupimsDualRBBank(Context *ctx, size_t numRows,
                                     size_t columnSize)
    : NewtonBank(TypeID<NeupimsDualRBBank>, ctx, numRows, columnSize) {
  pimBuffer.buffer.resize(columnSize);
  pimBuffer.isOpen = false;
  pimBuffer.row = -1;
}

void NeupimsDualRBBank::activate(size_t row) {
  Bank::activate(row);
  assert(row < getNumRows() && "Row address out of range");
  // Activation logic here
  assert(pimBuffer.isOpen == false &&
         "Row buffer must be closed before activation");
  Byte *rowPtr = dataArray.getRow(row);
  assert(pimBuffer.buffer.size() == getColumnSize() &&
         "Row buffer size mismatch");

  std::uninitialized_copy(rowPtr, rowPtr + getColumnSize(),
                          pimBuffer.buffer.begin());
  pimBuffer.row = row;
  pimBuffer.isOpen = true;
}

void NeupimsDualRBBank::write(size_t column, const Byte *src, size_t size) {
  Bank::write(column, src, size);
  assert(pimBuffer.isOpen && "Row buffer must be open for write");
  memcpy(pimBuffer.buffer.data() + column, src, size);
}

void NeupimsDualRBBank::comp() {
  assert(pimBuffer.isOpen && "Row buffer must be open to perform computation");

  addResult = doCompf16(
      pimBuffer.buffer,
      llvm::cast<NeupimsChannel>(getParentChannel())->getGlobalBuffer());
}

void NeupimsDualRBBank::doActivateHook(size_t row) {
  auto &rowBuffer = getRowBuffer();
  auto &pimBuffer = getPimBuffer();
  if (rowBuffer.isOpen) {
    if (rowBuffer.row != row) {
      precharge();
    } else {
      return; // Row is already active
    }
  }
  if (pimBuffer.isOpen) {
    pimPrecharge();
  }
  activate(row);
}

bool NeupimsDualRBBank::classof(const Bank *bank) {
  return TypeID<NeupimsDualRBBank> == bank->getTypeID();
}

int Neupims::command(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.empty()) {
    getContext()->getERR() << "No command provided. Available commands: "
                              "pim_header\n";
    return -1;
  }

  llvm::StringRef cmd = args[0];
  auto func =
      llvm::StringSwitch<std::function<int(llvm::ArrayRef<llvm::StringRef>)>>(
          cmd)
          .Case("help",
                [this](auto args) {
                  printHelp();
                  return 0;
                })
          .Case("pim_header", [this](auto args) { return pimHeader(args); })
          .Case("pim_precharge",
                [this](auto args) { return pimPrecharge(args); })
          .Default(nullptr);

  if (func) {
    return func(args.slice(1));
  } else {
    return Newton::command(args);
  }
}

int Neupims::pimHeader(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 2) {
    getContext()->getERR() << "Usage: pim_header <channel_address> <bits>\n";
    return -1;
  }

  llvm::StringRef channelAddrStr = args[0];
  llvm::StringRef bitsStr = args[1];

  // Parse channel Address
  size_t channelAddrV = parseAddress(channelAddrStr, getContext());
  dramsim3::Address dramAddr = getDRAMAddress(getConfig(), channelAddrV);
  Channel *channel = getChannel(dramAddr.channel);
  auto *neupimsChannel = llvm::dyn_cast<NeupimsChannel>(channel);
  if (!neupimsChannel) {
    getContext()->getERR() << "Channel at address " << channelAddrStr
                           << " is not a NeupimsChannel\n";
    return -1;
  }

  size_t bitsValue;
  if (bitsStr.getAsInteger(0, bitsValue)) {
    getContext()->getERR() << "Invalid bits value: " << bitsStr << "\n";
    return -1;
  }

  const auto &config = getConfig();
  size_t maxBit = config.ranks * config.bankgroups * config.banks;
  size_t mask = (1ULL << maxBit) - 1;
  if (bitsValue > mask) {
    getContext()->getERR() << "Bits value exceeds maximum allowed for "
                           << "the memory configuration (max " << maxBit - 1
                           << " bits)\n";
    return -1;
  }

  neupimsChannel->pimHeader(bitsValue);
  return 0;
}

int Neupims::pimPrecharge(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 1) {
    getContext()->getERR() << "Usage: pim_precharge <channel_address>\n";
    return -1;
  }

  llvm::StringRef channelAddrStr = args[0];
  size_t addrV = parseAddress(channelAddrStr, getContext());
  dramsim3::Address dramAddr = getDRAMAddress(getConfig(), addrV);
  Bank *bank = getChannel(dramAddr.channel)
                   ->getRank(dramAddr.rank)
                   ->getBankGroup(dramAddr.bankgroup)
                   ->getBank(dramAddr.bank);

  NeupimsDualRBBank *neupimsBank = llvm::cast<NeupimsDualRBBank>(bank);
  neupimsBank->pimPrecharge();
  return 0;
}

void Neupims::printHelp() const {
  llvm::raw_ostream &os = getContext()->getOS();
  Newton::printHelp();
  os << "Neupims Commands:\n";
  os << "  help                           - Show this help message\n";
  os << "  pim_header <channel_address> <bits> - Set the PIM header bits for "
        "the "
        "channel specified by <channel_address>\n";
  os << "  pim_precharge <row_address> - Precharge the bank specified by "
        "<row_address> for "
        "PIM operation\n";
}

int NeupimsController::command(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.empty()) {
    getContext()->getERR() << "No command provided. Available commands: "
                              "pim_header\n";
    return -1;
  }

  llvm::StringRef cmd = args[0];
  auto func =
      llvm::StringSwitch<std::function<int(llvm::ArrayRef<llvm::StringRef>)>>(
          cmd)
          .Case("pim_header", [this](auto args) { return pimHeader(args); })
          .Case("comp", [this](auto args) { return comp(args); }) // overrided
          .Default(nullptr);

  if (func) {
    return func(args.slice(1));
  } else {
    return NewtonController::command(args);
  }
}

void NeupimsController::printHelp() const {
  getContext()->getOS() << "Available commands:\n"
                        << "  pim_header <channel_address> <bits> - Set the "
                           "PIM header bits for the "
                           "channel\n";
}

int NeupimsController::pimHeader(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 2) {
    getContext()->getERR() << "Usage: pim_header <channel_address> <bits>\n";
    return -1;
  }

  llvm::StringRef channelAddrStr = args[0];
  llvm::StringRef bitsStr = args[1];

  // Parse channel address
  auto [memory, addr] = getAddress(channelAddrStr);
  if (!memory)
    return -1;

  auto *channel = memory->getChannel(addr.channel);
  auto *neupimsChannel = llvm::dyn_cast<NeupimsChannel>(channel);
  if (!neupimsChannel) {
    getContext()->getERR() << "Channel at address " << channelAddrStr
                           << " is not a NeupimsChannel\n";
    return -1;
  }

  size_t bitsValue;
  if (bitsStr.getAsInteger(0, bitsValue)) {
    getContext()->getERR() << "Invalid bits value: " << bitsStr << "\n";
    return -1;
  }

  const auto &config = memory->getConfig();
  size_t maxBit = config.ranks * config.bankgroups * config.banks;
  size_t mask = (1ULL << maxBit) - 1;
  if (bitsValue > mask) {
    getContext()->getERR() << "Bits value exceeds maximum allowed for "
                           << "the memory configuration (max " << maxBit - 1
                           << " bits)\n";
    return -1;
  }

  neupimsChannel->pimHeader(bitsValue);
  return 0;
}

int NeupimsController::comp(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 1) {
    getContext()->getERR() << "Usage: comp <channel_address>\n";
    return -1;
  }

  llvm::StringRef channelAddrStr = args[0];
  auto [memory, addr] = getAddress(channelAddrStr);
  if (!memory)
    return -1;

  auto *channel = memory->getChannel(addr.channel);
  auto *neupimsChannel = llvm::dyn_cast<NeupimsChannel>(channel);
  if (!neupimsChannel) {
    getContext()->getERR() << "Channel at address " << channelAddrStr
                           << " is not a NeupimsChannel\n";
    return -1;
  }

  const auto &config = memory->getConfig();

  auto header = neupimsChannel->getPimHeaderBits();
  for (size_t r = 0; r < config.ranks; ++r) {
    auto rank = neupimsChannel->getRank(r);
    for (size_t bg = 0; bg < config.bankgroups; ++bg) {
      auto bankGroup = rank->getBankGroup(bg);
      for (size_t b = 0; b < config.banks; ++b) {
        auto bank = bankGroup->getBank(b);
        bool isActive = header & 1;
        header >>= 1;
        if (!isActive) {
          continue; // Skip banks that are not in the PIM header
        }
        if (auto *neupimsBank = llvm::dyn_cast<NeupimsDualRBBank>(bank)) {
          auto &pimBuffer = neupimsBank->getPimBuffer();
          auto &rowBuffer = neupimsBank->getRowBuffer();
          if (!pimBuffer.isOpen) {
            if (rowBuffer.isOpen) {
              neupimsBank->precharge();
            }
            neupimsBank->activate(addr.row);
          } else if (pimBuffer.row != addr.row) {
            neupimsBank->pimPrecharge();
            if (rowBuffer.isOpen) {
              neupimsBank->precharge();
            }
            neupimsBank->activate(addr.row);
          }
        } else {
          getContext()->getERR()
              << "Bank at rank " << r << ", bank group " << bg << ", bank " << b
              << " is not a NeupimsBank\n";
          return -1;
        }
      }
    }
  }
  neupimsChannel->comp();
  return 0;
}

} // namespace pimsim
