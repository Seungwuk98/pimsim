#include "pimsim/Newton.h"
#include "pimsim/Neupims.h"
#include "llvm/Support/Casting.h"

namespace pimsim {

int Newton::command(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.empty()) {
    getContext()->getERR() << "No command provided.\n";
    return -1;
  }

  assert(!args.empty() && "No command provided.");
  llvm::StringRef cmd = args[0];
  if (cmd == "help") {
    return printHelp();
  } else if (cmd == "comp") {
    return comp(args.slice(1));
  } else if (cmd == "read_res") {
    return readRes(args.slice(1));
  } else if (cmd == "gact") {
    return gact(args.slice(1));
  } else if (cmd == "gwrite") {
    return gwrite(args.slice(1));
  } else if (cmd == "show_gbuf") {
    return showGBuffer(args.slice(1));
  } else {
    return DefaultDRAM::command(args);
  }
}

int Newton::printHelp() const {
  llvm::raw_ostream &os = getContext()->getOS();
  os << "Available Commands:\n";
  DefaultDRAM::printHelp();
  os << "Newton Commands:\n";
  os << "  help                           - Show this help message\n";
  os << "  comp                           - Perform computation on all banks\n";
  os << "  read_res <address>             - Read computation result back to "
        "memory at <address>\n";
  os << "  gact <address>                 - Activate all banks with data from "
        "global buffer\n";
  os << "  gwrite <address>               - Write data from memory at "
        "<address> to global buffer\n";
  os << "  show_gbuf <channel_address>    - Show the contents of the global "
        "buffer of the channel corresponding to <channel_address>\n";
  return 0;
}

int Newton::comp(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 1) {
    getContext()->getERR() << "Usage: comp <address>\n";
    return -1;
  }
  size_t address = parseAddress(args[0], getContext());
  dramsim3::Address dramAddr = getDRAMAddress(getConfig(), address);
  Channel *channel = getChannel(dramAddr.channel);
  NewtonChannel *newtonChannel = llvm::cast<NewtonChannel>(channel);
  newtonChannel->comp();
  return 0;
}

int Newton::readRes(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 2) {
    getContext()->getERR()
        << "Usage: read_res <channel_address> <result_address>\n";
    return -1;
  }
  size_t channelAddrV = parseAddress(args[0], getContext());
  dramsim3::Address channelAddr = getDRAMAddress(getConfig(), channelAddrV);
  Channel *channel = getChannel(channelAddr.channel);
  NewtonChannel *newtonChannel = llvm::cast<NewtonChannel>(channel);

  size_t resultAddrV = parseAddress(args[1], getContext());
  dramsim3::Address resultAddr = getDRAMAddress(getConfig(), resultAddrV);
  newtonChannel->readRes(resultAddr);
  return 0;
}

int Newton::gact(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 2) {
    getContext()->getERR() << "Usage: gact <bg_and_row_address>\n";
    return -1;
  }

  size_t address = parseAddress(args[0], getContext());
  dramsim3::Address dramAddr = getDRAMAddress(getConfig(), address);
  Channel *channel = getChannel(dramAddr.channel);
  NewtonChannel *newtonChannel = llvm::cast<NewtonChannel>(channel);
  BankGroup *bankGroup =
      newtonChannel->getRank(dramAddr.rank)->getBankGroup(dramAddr.bankgroup);
  NewtonBankGroup *newtonBankGroup = llvm::cast<NewtonBankGroup>(bankGroup);
  newtonBankGroup->gact(dramAddr.row);
  return 0;
}

int Newton::gwrite(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 2) {
    getContext()->getERR() << "Usage: gwrite <channel_address> <src_address>\n";
    return -1;
  }
  size_t channelAddrV = parseAddress(args[0], getContext());
  dramsim3::Address channelAddr = getDRAMAddress(getConfig(), channelAddrV);
  Channel *channel = getChannel(channelAddr.channel);
  NewtonChannel *newtonChannel = llvm::cast<NewtonChannel>(channel);

  size_t srcAddrV = parseAddress(args[1], getContext());
  dramsim3::Address srcAddr = getDRAMAddress(getConfig(), srcAddrV);
  newtonChannel->gwrite(srcAddr);
  return 0;
}

int Newton::showGBuffer(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 1) {
    getContext()->getERR() << "Usage: show_gbuf <channel_address>\n";
    return -1;
  }
  size_t channelAddrV = parseAddress(args[0], getContext());
  dramsim3::Address channelAddr = getDRAMAddress(getConfig(), channelAddrV);
  Channel *channel = getChannel(channelAddr.channel);
  NewtonChannel *newtonChannel = llvm::cast<NewtonChannel>(channel);
  newtonChannel->inspectGlobalBuffer(getContext()->getOS());
  return 0;
}

NewtonChannel::NewtonChannel(Context *ctx,
                             std::vector<std::unique_ptr<Rank>> &&rks)
    : Channel(TypeID<NewtonChannel>, ctx, std::move(rks)) {}

NewtonChannel::NewtonChannel(size_t typeID, Context *ctx,
                             std::vector<std::unique_ptr<Rank>> &&rks)
    : Channel(typeID, ctx, std::move(rks)) {}

void NewtonChannel::initializeGlobalBuffer() {
  auto columnSize = getParentMemory()->getConfig().columns;
  globalBuffer.buffer.resize(columnSize, 0);
}

void NewtonChannel::comp() {
  if (globalBuffer.buffer.size() == 0) {
    getContext()->getERR()
        << "Global buffer is not initialized. Use gwrite command to "
           "initialize it before computation.\n";
    return;
  }

  for (auto &rank : ranks) {
    for (size_t bgi = 0; bgi < getParentMemory()->getConfig().bankgroups;
         ++bgi) {
      auto bankGroup = rank->getBankGroup(bgi);
      for (size_t bi = 0; bi < getParentMemory()->getConfig().banks; ++bi) {
        auto bank = bankGroup->getBank(bi);
        NewtonBank *newtonBank = llvm::cast<NewtonBank>(bank);
        newtonBank->comp();
      }
    }
  }
}

llvm::SmallVector<f16> NewtonChannel::readResult() const {
  llvm::SmallVector<f16> result;
  auto &config = getParentMemory()->getConfig();
  result.reserve(config.ranks * config.bankgroups * config.banks / 2);

  for (size_t r = 0; r < config.ranks; ++r) {
    auto rank = ranks[r].get();
    for (size_t bg = 0; bg < config.bankgroups; ++bg) {
      auto bankGroup = rank->getBankGroup(bg);
      for (size_t b = 0; b < config.banks; ++b) {
        NewtonBank *bank = llvm::cast<NewtonBank>(bankGroup->getBank(b));
        result.emplace_back(bank->addResult);
      }
    }
  }

  return result;
}

void NewtonChannel::readRes(const dramsim3::Address &addr) {
  llvm::SmallVector<Byte> tempBuffer;
  llvm::SmallVector<f16> result = readResult();
  size_t dataSize = result.size() * sizeof(f16);
  tempBuffer.resize(dataSize);
  std::memcpy(tempBuffer.data(), result.data(), dataSize);

  auto written = getParentMemory()->write(tempBuffer, addr.channel, addr.rank,
                                          addr.bankgroup, addr.bank, addr.row,
                                          addr.column);
  assert(written == dataSize && "Did not write all result data back to memory");
}

void NewtonChannel::gwrite(const dramsim3::Address &addr) {
  if (globalBuffer.buffer.size() == 0) {
    initializeGlobalBuffer();
  }

  size_t dataSize =
      getParentMemory()->getConfig().columns; // assuming full row activation
  auto readData =
      getParentMemory()->read(globalBuffer.buffer, addr.channel, addr.rank,
                              addr.bankgroup, addr.bank, addr.row, addr.column);
  assert(readData == dataSize && "Did not read all data for global buffer");
}

bool NewtonChannel::classof(const Channel *channel) {
  return ClassOf::classof(channel) || llvm::isa<NeupimsChannel>(channel);
}

void NewtonBankGroup::gact(size_t row) {
  for (auto &bank : banks) {
    NewtonBank *newtonBank = llvm::cast<NewtonBank>(bank.get());
    newtonBank->activate(row);
  }
}

NewtonBank::NewtonBank(Context *ctx, size_t numRows, size_t columnSize)
    : Bank(TypeID<NewtonBank>, ctx, numRows, columnSize) {}

NewtonBank::NewtonBank(size_t typeID, Context *ctx, size_t numRows,
                       size_t columnSize)
    : Bank(typeID, ctx, numRows, columnSize) {}

void NewtonBank::comp() {
  assert(rowBuffer.isOpen && "Row must be open to perform computation");
  // For simplicity, we perform addition of all f16 elements in the row buffer
  addResult = doCompf16(
      rowBuffer.buffer,
      llvm::cast<NewtonChannel>(getParentChannel())->getGlobalBuffer());
}

bool NewtonBank::classof(const Bank *bank) {
  return ClassOf::classof(bank) || llvm::isa<NeupimsDualRBBank>(bank);
}

f16 NewtonBank::doCompf16(llvm::ArrayRef<Byte> rowBufferData,
                          llvm::ArrayRef<Byte> globalBufferData) {
  size_t numElements = rowBufferData.size() / sizeof(f16);
  llvm::APFloat sum(llvm::APFloat::IEEEhalf(), 0);

  for (size_t i = 0; i < numElements; ++i) {
    f16 rowBufElem;
    std::memcpy(&rowBufElem, rowBufferData.data() + i * sizeof(f16),
                sizeof(f16));
    llvm::APFloat rowBufAP(llvm::APFloat::IEEEhalf(),
                           llvm::APInt(16, rowBufElem.data));

    f16 globalBufElem;
    std::memcpy(&globalBufElem, globalBufferData.data() + i * sizeof(f16),
                sizeof(f16));
    llvm::APFloat globalBufAP(llvm::APFloat::IEEEhalf(),
                              llvm::APInt(16, globalBufElem.data));

    auto product = rowBufAP * globalBufAP;
    sum.add(product, llvm::APFloat::rmNearestTiesToEven);
  }

  f16 result;
  result.data = static_cast<uint16_t>(sum.bitcastToAPInt().getZExtValue());
  return result;
}

NewtonFastBank::NewtonFastBank(Context *ctx, size_t numRows, size_t columnSize)
    : Bank(TypeID<NewtonFastBank>, ctx, numRows, columnSize) {}

void NewtonFastBank::comp() {
  assert(rowBuffer.isOpen && "Row must be open to perform computation");
  // For simplicity, we perform addition of all f16 elements in the row buffer
  size_t numElements = rowBuffer.buffer.size() / sizeof(f16);
  f32 sum = 0.0f;

  llvm::ArrayRef<Byte> bufferRef(
      llvm::cast<NewtonChannel>(getParentChannel())->globalBuffer.buffer);
  assert(bufferRef.size() == rowBuffer.buffer.size() &&
         "initialize global buffer before computation. use gact command.");

  for (size_t i = 0; i < numElements; ++i) {
    f16 element;
    std::memcpy(&element, rowBuffer.buffer.data() + i * sizeof(f16),
                sizeof(f16));
    // Convert f16 to f32
    llvm::APFloat rowBufElem(
        llvm::APFloat::IEEEhalf(),
        llvm::APInt(16, static_cast<uint16_t>(element.data)));

    std::memcpy(&element, bufferRef.data() + i * sizeof(f16), sizeof(f16));
    llvm::APFloat globalBufElem(
        llvm::APFloat::IEEEhalf(),
        llvm::APInt(16, static_cast<uint16_t>(element.data)));

    sum += rowBufElem.convertToFloat() * globalBufElem.convertToFloat();
  }
  // Convert sum back to f16
  llvm::APFloat sumAP(sum);
  bool loseInfo;
  sumAP.convert(llvm::APFloat::IEEEhalf(), llvm::APFloat::rmNearestTiesToEven,
                &loseInfo);
  addResult.data = static_cast<uint16_t>(sumAP.bitcastToAPInt().getZExtValue());
}

int NewtonController::command(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.empty()) {
    context->getERR() << "No command provided.\n";
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
          .Case("comp", [this](auto args) { return comp(args); })
          .Case("read_res", [this](auto args) { return readRes(args); })
          .Case("gwrite", [this](auto args) { return gwrite(args); })
          .Case("show_res",
                [this](auto args) {
                  showRes(args);
                  return 0;
                })
          .Default(nullptr);

  if (func) {
    return func(args.slice(1));
  } else {
    return DefaultDRAMController::command(args);
  }
}

void NewtonController::printHelp() const {
  llvm::raw_ostream &os = getContext()->getOS();
  DefaultDRAMController::printHelp();
  os << "NewtonController Commands:\n";
  os << "  comp <address>                 - Trigger computation on the channel "
        "specified "
        "by <address>\n";
  os << "  read_res <channel_address> <result_address> - Read computation "
        "result from the channel specified by <channel_address> back to memory "
        "at <result_address>\n";
  os << "  gwrite <channel_address> <src_address> - Write data from memory at "
        "<src_address> to the global buffer of the channel specified by "
        "<channel_address>\n";
  os << "  show_res <channel_address> - Show the computation result of the "
        "channel specified by <channel_address>\n";
}

int NewtonController::comp(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 1) {
    getContext()->getERR() << "Usage: comp <channel_address>\n";
    return -1;
  }

  auto [memory, dramAddr] = getAddress(args[0]);
  if (!memory)
    return -1;

  Channel *channel = memory->getChannel(dramAddr.channel);
  NewtonChannel *newtonChannel = llvm::cast<NewtonChannel>(channel);
  if (newtonChannel->getGlobalBuffer().empty()) {
    getContext()->getERR()
        << "Global buffer is empty. Please initialize it with gwrite command "
           "before performing computation.\n";
    return -1;
  }

  size_t row = dramAddr.row;
  const auto &config = memory->getConfig();
  if (row >= config.rows) {
    getContext()->getERR() << "Invalid row address: " << row << "\n";
    return -1;
  }

  for (size_t r = 0; r < config.ranks; ++r) {
    auto rank = newtonChannel->getRank(r);
    for (size_t bg = 0; bg < config.bankgroups; ++bg) {
      auto bankGroup = rank->getBankGroup(bg);
      for (size_t b = 0; b < config.banks; ++b) {
        NewtonBank *bank = llvm::cast<NewtonBank>(bankGroup->getBank(b));
        if (!bank->getRowBuffer().isOpen) {
          bank->activate(row);
        } else if (bank->getRowBuffer().row != row) {
          bank->precharge();
          bank->activate(row);
        }
      }
    }
  }

  newtonChannel->comp();
  return 0;
}

int NewtonController::readRes(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 2) {
    getContext()->getERR()
        << "Usage: read_res <channel_address> <result_address>\n";
    return -1;
  }

  auto [memory, dramAddr] = getAddress(args[0]);
  if (!memory) {
    getContext()->getERR() << "Invalid channel address: " << args[0] << "\n";
    return -1;
  }

  auto [resultMemory, resultDramAddr] = getAddress(args[1]);
  if (!resultMemory) {
    getContext()->getERR() << "Invalid result address: " << args[1] << "\n";
    return -1;
  }

  Channel *channel = memory->getChannel(dramAddr.channel);
  NewtonChannel *newtonChannel = llvm::cast<NewtonChannel>(channel);

  newtonChannel->readRes(resultDramAddr);
  return 0;
}

int NewtonController::gwrite(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 2) {
    getContext()->getERR() << "Usage: gwrite <channel_address> <src_address>\n";
    return -1;
  }

  auto [memory, dramAddr] = getAddress(args[0]);
  if (!memory) {
    getContext()->getERR() << "Invalid channel address: " << args[0] << "\n";
    return -1;
  }

  Channel *channel = memory->getChannel(dramAddr.channel);
  NewtonChannel *newtonChannel = llvm::cast<NewtonChannel>(channel);

  auto [srcMemory, srcDramAddr] = getAddress(args[1]);
  if (!srcMemory) {
    getContext()->getERR() << "Invalid source address: " << args[1] << "\n";
    return -1;
  }

  newtonChannel->gwrite(srcDramAddr);
  return 0;
}

int NewtonController::showRes(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 1) {
    getContext()->getERR() << "Usage: show_res <channel_address>\n";
    return -1;
  }

  auto [memory, dramAddr] = getAddress(args[0]);
  if (!memory) {
    getContext()->getERR() << "Invalid channel address: " << args[0] << "\n";
    return -1;
  }

  Channel *channel = memory->getChannel(dramAddr.channel);
  NewtonChannel *newtonChannel = llvm::cast<NewtonChannel>(channel);
  llvm::SmallVector<f16> result = newtonChannel->readResult();
  llvm::raw_ostream &os = getContext()->getOS();
  os << "Computation Result:\n";
  for (size_t i = 0; i < result.size(); ++i) {
    os << "Element " << i << ": " << result[i] << "\n";
  }
  return 0;
}

} // namespace pimsim
