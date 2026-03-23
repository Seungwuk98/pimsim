#include "pimsim/DefaultDRAM.h"
#include "common.h"
#include "pimsim/TypeSupport.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/FunctionExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

namespace pimsim {

int DefaultDRAM::command(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.empty()) {
    getContext()->getERR() << "No command provided.\n";
    return -1;
  }

  llvm::StringRef cmd = args[0];

  auto func = llvm::StringSwitch<
                  llvm::function_ref<int(llvm::ArrayRef<llvm::StringRef>)>>(cmd)
                  .Case("help",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          return printHelp();
                        })
                  .Case("show_row",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          return showRow(args);
                        })
                  .Case("show_row_buffer",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          return showRowBuffer(args);
                        })
                  .Case("write",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          return write(args);
                        })
                  .Case("read",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          return read(args);
                        })
                  .Case("activate",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          return activate(args);
                        })
                  .Case("precharge",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          return precharge(args);
                        })
                  // Add more command cases here
                  .Default(nullptr);

  if (func) {
    return func(args.slice(1));
  } else {
    return -2;
  }
}

int DefaultDRAM::printHelp() const {
  llvm::raw_ostream &os = getContext()->getOS();
  os << "DefaultDRAM Commands:\n";
  os << "  help                           - Show this help message\n";
  os << "  show_row <address> <type>      - Show the contents of the row at "
        "<address> broken down by <type>\n";
  os << "  show_row_buffer <address> <type> - Show the contents of the row "
        "buffer "
        "of the bank corresponding to <address> broken down by <type>\n";
  os << "  write <address> <value> <type> - Write <value> to the memory at "
        "<address>\n";
  os << "  read <address> <type>          - Read type of value from memory at "
        "<address>\n";
  os << "  activate <address>              - Activate the row at <address>\n";
  os << "  precharge <address>             - Precharge the row at <address>\n";
  // Add more command descriptions here
  return 0;
}

int DefaultDRAM::showRow(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 2) {
    getContext()->getERR() << "Usage: show_row <address> <type>\n";
    return -1;
  }

  size_t address = parseAddress(args[0], getContext());
  if (address == static_cast<size_t>(-1)) {
    return -1;
  }

  dramsim3::Address dramAddr = getDRAMAddress(getConfig(), address);
  Channel *channel = getChannel(dramAddr.channel);
  Rank *rank = channel->getRank(dramAddr.rank);
  BankGroup *bankGroup = rank->getBankGroup(dramAddr.bankgroup);
  Bank *bank = bankGroup->getBank(dramAddr.bank);

  auto typeStr = args[1];
  if (typeStr == "i8") {
    bank->inspectRow<int8_t>(getContext()->getOS(), address - dramAddr.column,
                             dramAddr.row);
  } else if (typeStr == "u8") {
    bank->inspectRow<uint8_t>(getContext()->getOS(), address - dramAddr.column,
                              dramAddr.row);
  } else if (typeStr == "i16") {
    bank->inspectRow<int16_t>(getContext()->getOS(), address - dramAddr.column,
                              dramAddr.row);
  } else if (typeStr == "u16") {
    bank->inspectRow<uint16_t>(getContext()->getOS(), address - dramAddr.column,
                               dramAddr.row);
  } else if (typeStr == "i32") {
    bank->inspectRow<int32_t>(getContext()->getOS(), address - dramAddr.column,
                              dramAddr.row);
  } else if (typeStr == "u32") {
    bank->inspectRow<uint32_t>(getContext()->getOS(), address - dramAddr.column,
                               dramAddr.row);
  } else if (typeStr == "i64") {
    llvm::errs() << "DEBUG: Showing row as i64\n";
    bank->inspectRow<int64_t>(getContext()->getOS(), address - dramAddr.column,
                              dramAddr.row);
  } else if (typeStr == "u64") {
    bank->inspectRow<uint64_t>(getContext()->getOS(), address - dramAddr.column,
                               dramAddr.row);
  } else if (typeStr == "f16") {
    bank->inspectRow<f16>(getContext()->getOS(), address - dramAddr.column,
                          dramAddr.row);
  } else if (typeStr == "bf16") {
    bank->inspectRow<bf16>(getContext()->getOS(), address - dramAddr.column,
                           dramAddr.row);
  } else if (typeStr == "f32") {
    bank->inspectRow<float>(getContext()->getOS(), address - dramAddr.column,
                            dramAddr.row);
  } else if (typeStr == "f64") {
    bank->inspectRow<double>(getContext()->getOS(), address - dramAddr.column,
                             dramAddr.row);
  } else {
    getContext()->getERR() << "Unsupported type: " << typeStr << "\n";
    return -1;
  }
  return 0;
}

int DefaultDRAM::showRowBuffer(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 2) {
    getContext()->getERR() << "Usage: show_row_buffer <address> <type>\n";
    return -1;
  }

  size_t address = parseAddress(args[0], getContext());
  if (address == static_cast<size_t>(-1)) {
    return -1;
  }

  dramsim3::Address dramAddr = getDRAMAddress(getConfig(), address);
  Channel *channel = getChannel(dramAddr.channel);
  Rank *rank = channel->getRank(dramAddr.rank);
  BankGroup *bankGroup = rank->getBankGroup(dramAddr.bankgroup);
  Bank *bank = bankGroup->getBank(dramAddr.bank);

  auto typeStr = args[1];
  if (typeStr == "i8") {
    bank->inspectRowBuffer<int8_t>(getContext()->getOS());
  } else if (typeStr == "u8") {
    bank->inspectRowBuffer<uint8_t>(getContext()->getOS());
  } else if (typeStr == "i16") {
    bank->inspectRowBuffer<int16_t>(getContext()->getOS());
  } else if (typeStr == "u16") {
    bank->inspectRowBuffer<uint16_t>(getContext()->getOS());
  } else if (typeStr == "i32") {
    bank->inspectRowBuffer<int32_t>(getContext()->getOS());
  } else if (typeStr == "u32") {
    bank->inspectRowBuffer<uint32_t>(getContext()->getOS());
  } else if (typeStr == "i64") {
    bank->inspectRowBuffer<int64_t>(getContext()->getOS());
  } else if (typeStr == "u64") {
    bank->inspectRowBuffer<uint64_t>(getContext()->getOS());
  } else if (typeStr == "f16") {
    bank->inspectRowBuffer<f16>(getContext()->getOS());
  } else if (typeStr == "bf16") {
    bank->inspectRowBuffer<bf16>(getContext()->getOS());
  } else if (typeStr == "f32") {
    bank->inspectRowBuffer<float>(getContext()->getOS());
  } else if (typeStr == "f64") {
    bank->inspectRowBuffer<double>(getContext()->getOS());
  } else {
    getContext()->getERR() << "Unsupported type: " << typeStr << "\n";
    return -1;
  }

  return 0;
}

int DefaultDRAM::write(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 3) {
    getContext()->getERR() << "Invalid number of arguments: " << args.size()
                           << "\n";
    getContext()->getERR() << "Usage: write <address> <value> <type>\n";
    return -1;
  }

  size_t address = parseAddress(args[0], getContext());
  if (address == static_cast<size_t>(-1)) {
    return -1;
  }

  llvm::StringRef valueStr = args[1];
  llvm::StringRef typeStr = args[2];

  dramsim3::Address dramAddr = getDRAMAddress(getConfig(), address);
  Channel *channel = getChannel(dramAddr.channel);
  Rank *rank = channel->getRank(dramAddr.rank);
  BankGroup *bankGroup = rank->getBankGroup(dramAddr.bankgroup);
  Bank *bank = bankGroup->getBank(dramAddr.bank);

  auto [typeSize, printer, verifier, isInt] = parseType(typeStr, getContext());
  if (typeSize == 0) {
    getContext()->getERR() << "Unsupported type: " << typeStr << "\n";
    return -1;
  }

  std::vector<Byte> bytes(typeSize);
  if (isInt) {
    uint64_t value = 0;
    if (valueStr.getAsInteger(0, value)) {
      getContext()->getERR() << "Invalid integer value: " << valueStr << "\n";
      return -1;
    }
    memcpy(bytes.data(), &value, typeSize);
  } else {
    // For floating point types, we can use llvm::APFloat to parse the value
    llvm::APFloat apValue(parseFloatSemantics(typeStr));
    auto parseResult =
        apValue.convertFromString(valueStr, llvm::APFloat::rmNearestTiesToEven);
    if (auto ec = parseResult.takeError()) {
      getContext()->getERR()
          << "Invalid floating point value: " << valueStr << "\n";
      return -1;
    }
    uint64_t intValue = apValue.bitcastToAPInt().getZExtValue();
    memcpy(bytes.data(), &intValue, typeSize);
  }
  bank->write(dramAddr.column, bytes.data(), typeSize);
  return 0;
}

int DefaultDRAM::read(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 2) {
    getContext()->getERR() << "Usage: read <address> <type>\n";
    return -1;
  }

  size_t address = parseAddress(args[0], getContext());
  if (address == static_cast<size_t>(-1)) {
    return -1;
  }

  llvm::StringRef typeStr = args[1];

  dramsim3::Address dramAddr = getDRAMAddress(getConfig(), address);
  Channel *channel = getChannel(dramAddr.channel);
  Rank *rank = channel->getRank(dramAddr.rank);
  BankGroup *bankGroup = rank->getBankGroup(dramAddr.bankgroup);
  Bank *bank = bankGroup->getBank(dramAddr.bank);

  auto [typeSize, printer, verifier, isInt] = parseType(typeStr, getContext());
  if (typeSize == 0) {
    getContext()->getERR() << "Unsupported type: " << typeStr << "\n";
    return -1;
  }

  std::vector<Byte> data(typeSize);
  bank->read(dramAddr.column, data.data(), typeSize);
  printer(getContext()->getOS(), data);
  return 0;
}

int DefaultDRAM::activate(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 1) {
    getContext()->getERR() << "Usage: activate <address>\n";
    return -1;
  }

  size_t address = parseAddress(args[0], getContext());
  if (address == static_cast<size_t>(-1)) {
    return -1;
  }

  dramsim3::Address dramAddr = getDRAMAddress(getConfig(), address);
  Channel *channel = getChannel(dramAddr.channel);
  Rank *rank = channel->getRank(dramAddr.rank);
  BankGroup *bankGroup = rank->getBankGroup(dramAddr.bankgroup);
  Bank *bank = bankGroup->getBank(dramAddr.bank);

  bank->activate(dramAddr.row);
  return 0;
}

int DefaultDRAM::precharge(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 1) {
    getContext()->getERR() << "Usage: precharge <address>\n";
    return -1;
  }

  size_t address = parseAddress(args[0], getContext());
  if (address == static_cast<size_t>(-1)) {
    return -1;
  }

  dramsim3::Address dramAddr = getDRAMAddress(getConfig(), address);
  Channel *channel = getChannel(dramAddr.channel);
  Rank *rank = channel->getRank(dramAddr.rank);
  BankGroup *bankGroup = rank->getBankGroup(dramAddr.bankgroup);
  Bank *bank = bankGroup->getBank(dramAddr.bank);

  bank->precharge();
  return 0;
}

int DefaultDRAMController::command(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.empty()) {
    getContext()->getERR() << "No command provided.\n";
    return -1;
  }

  llvm::StringRef cmd = args[0];

  auto func = llvm::StringSwitch<
                  llvm::function_ref<int(llvm::ArrayRef<llvm::StringRef>)>>(cmd)
                  .Case("help",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          printHelp();
                          return 0;
                        })
                  .Case("read",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          return read(args);
                        })
                  .Case("write",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          return write(args);
                        })
                  .Case("verify",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          return verify(args);
                        })
                  .Case("memories",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          printMemories();
                          return 0;
                        })
                  .Case("encode",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          return encode(args);
                        })
                  .Case("decode",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          return decode(args);
                        })
                  // Add more command cases here
                  .Default(nullptr);

  if (func) {
    return func(args.slice(1));
  } else {
    return Controller::command(args); // Try base controller commands
  }
}

void DefaultDRAMController::printHelp() const {
  Controller::printHelp(); // Print base controller help first
  llvm::raw_ostream &os = getContext()->getOS();
  os << "DefaultDRAMController Commands:\n";
  os << "  help                           - Show this help message\n";
  os << "  read <address> <type>          - Read type of value from memory at "
        "<address>\n";
  os << "  write <address> <value> <type> - Write <value> to the memory at "
        "<address>\n";
  os << "  verify <address> <expected_value> <type> <e>? - Verify that the "
        "value at <address> matches <expected_value>\n";
  os << "  memories                       - List all connected memories\n";
  os << "  encode <memory_idx> ch=<channel> ra=<rank> "
        "bg=<bankgroup> "
        "ba=<bank> "
        "row=<row> "
        "col=<column> - Encode the given "
        "address components into a physical address\n";
  os << "  decode <address> - Decode the given physical address into its "
        "components\n";
  // Add more command descriptions here
}

int DefaultDRAMController::read(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 2) {
    getContext()->getERR() << "Usage: read <address> <type>\n";
    return -1;
  }

  auto [memory, memAddress] = getAddress(args[0]);
  if (!memory)
    return -1;

  llvm::StringRef typeStr = args[1];
  auto [typeSize, printer, verifier, isInt] = parseType(typeStr, getContext());
  if (typeSize == 0) {
    getContext()->getERR() << "Unsupported type: " << typeStr << "\n";
    return -1;
  }

  std::vector<Byte> data(typeSize);

  if (int result = read(memory, memAddress, data)) {
    return result;
  }
  printer(getContext()->getOS(), data);
  return 0;
}

int DefaultDRAMController::read(Memory *memory, dramsim3::Address address,
                                llvm::MutableArrayRef<Byte> out) {
  if (!memory) {
    getContext()->getERR() << "Memory is null\n";
    return -1;
  }

  auto defaultDRAM = llvm::cast<DefaultDRAM>(memory);
  auto bank = defaultDRAM->getChannel(address.channel)
                  ->getRank(address.rank)
                  ->getBankGroup(address.bankgroup)
                  ->getBank(address.bank);
  assert(bank && "Bank should not be null");

  memory->read(out, address.channel, address.rank, address.bankgroup,
               address.bank, address.row, address.column);
  return 0;
}

int DefaultDRAMController::write(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() != 3) {
    getContext()->getERR() << "Invalid number of arguments: " << args.size()
                           << "\n";
    getContext()->getERR() << "Usage: write <address> <value> <type>\n";
    return -1;
  }

  auto [memory, memAddress] = getAddress(args[0]);
  if (!memory)
    return -1;

  llvm::StringRef valueStr = args[1];
  llvm::StringRef typeStr = args[2];

  auto [typeSize, printer, verifier, isInt] = parseType(typeStr, getContext());
  if (typeSize == 0) {
    getContext()->getERR() << "Unsupported type: " << typeStr << "\n";
    return -1;
  }

  std::vector<Byte> bytes(typeSize);
  if (isInt) {
    uint64_t value = 0;
    if (valueStr.getAsInteger(0, value)) {
      getContext()->getERR() << "Invalid integer value: " << valueStr << "\n";
      return -1;
    }
    memcpy(bytes.data(), &value, typeSize);
  } else {
    // For floating point types, we can use llvm::APFloat to parse the value
    llvm::APFloat apValue(parseFloatSemantics(typeStr));
    auto parseResult =
        apValue.convertFromString(valueStr, llvm::APFloat::rmNearestTiesToEven);
    if (auto ec = parseResult.takeError()) {
      getContext()->getERR()
          << "Invalid floating point value: " << valueStr << "\n";
      return -1;
    }
    uint64_t intValue = apValue.bitcastToAPInt().getZExtValue();
    memcpy(bytes.data(), &intValue, typeSize);
  }

  if (int result = write(memory, memAddress, bytes)) {
    return result;
  }
  return 0;
}

int DefaultDRAMController::write(Memory *memory, dramsim3::Address address,
                                 llvm::ArrayRef<Byte> data) {
  if (!memory) {
    getContext()->getERR() << "Memory is null\n";
    return -1;
  }

  auto defaultDRAM = llvm::cast<DefaultDRAM>(memory);
  auto bank = defaultDRAM->getChannel(address.channel)
                  ->getRank(address.rank)
                  ->getBankGroup(address.bankgroup)
                  ->getBank(address.bank);
  assert(bank && "Bank should not be null");

  memory->write(data, address.channel, address.rank, address.bankgroup,
                address.bank, address.row, address.column);
  return 0;
}

int DefaultDRAMController::verify(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() < 3) {
    getContext()->getERR()
        << "Usage: verify <address> <expected_value> <type> <e>?\n";
    return -1;
  }

  auto [memory, memAddress] = getAddress(args[0]);
  if (!memory)
    return -1;

  llvm::StringRef expectedValueStr = args[1];
  llvm::StringRef typeStr = args[2];

  auto [typeSize, printer, verifier, isInt] = parseType(typeStr, getContext());
  if (typeSize == 0) {
    getContext()->getERR() << "Unsupported type: " << typeStr << "\n";
    return -1;
  }

  auto bank = memory->getChannel(memAddress.channel)
                  ->getRank(memAddress.rank)
                  ->getBankGroup(memAddress.bankgroup)
                  ->getBank(memAddress.bank);
  assert(bank && "Bank should not be null");

  std::vector<Byte> memoryData(typeSize);
  auto &rowBuffer = bank->getRowBuffer();
  if (!rowBuffer.isOpen) {
    bank->activate(memAddress.row);
  }
  if (rowBuffer.row != memAddress.row) {
    bank->precharge();
    bank->activate(memAddress.row);
  }
  bank->read(memAddress.column, memoryData.data(), typeSize);
  if (!verifier(memoryData, expectedValueStr, context->getERR())) {
    getContext()->getERR() << "Verification failed at address ";
    parseAndShow(args[0], getContext()->getERR());
    getContext()->getERR() << "(";
    printer(getContext()->getERR(), memoryData);
    getContext()->getERR() << " != " << expectedValueStr << ")\n";
    return -1;
  }
  getContext()->getOS() << "Verification succeeded at address ";
  parseAndShow(args[0], getContext()->getOS());
  getContext()->getOS() << " (";
  printer(getContext()->getOS(), memoryData);
  getContext()->getOS() << " == " << expectedValueStr << ")\n";
  return 0;
}

void DefaultDRAMController::printMemories() const {
  llvm::raw_ostream &os = getContext()->getOS();
  os << "Connected Memories:\n";
  for (size_t i = 0; i < memories.size(); ++i) {
    os << "  [" << i << "] " << memories[i]->getExecutorName() << "\n";
  }
}

int DefaultDRAMController::encode(llvm::ArrayRef<llvm::StringRef> args) const {
  if (args.empty()) {
    getContext()->getERR()
        << "Usage: encode <memory_idx> ch=<channel> ra=<rank> "
        << "bg=<bankgroup> ba=<bank> row=<row> col=<column>\n";
    return -1;
  }

  auto [memIdx, memAddress] = parseBufferAddress(args);
  if (memIdx == static_cast<size_t>(-1)) {
    return -1;
  }

  size_t address = encodeAddress(memIdx, memAddress.channel, memAddress.rank,
                                 memAddress.bankgroup, memAddress.bank,
                                 memAddress.row, memAddress.column);
  if (address == static_cast<size_t>(-1)) {
    return -1;
  }

  context->getOS() << "Encoded address: " << llvm::format_hex(address, 8, true)
                   << "\n";
  return 0;
}

int DefaultDRAMController::decode(llvm::ArrayRef<llvm::StringRef> args) const {
  if (args.size() != 1) {
    getContext()->getERR() << "Usage: decode <address>\n";
    return -1;
  }

  auto [memory, memAddress] = getAddress(args[0]);
  if (!memory)
    return -1;

  context->getOS() << "Decoded address components:\n";
  context->getOS() << "  Memory: " << memory->getExecutorName() << "\n";
  context->getOS() << "  Channel: " << memAddress.channel << "\n";
  context->getOS() << "  Rank: " << memAddress.rank << "\n";
  context->getOS() << "  Bank Group: " << memAddress.bankgroup << "\n";
  context->getOS() << "  Bank: " << memAddress.bank << "\n";
  context->getOS() << "  Row: " << memAddress.row << "\n";
  context->getOS() << "  Column: " << memAddress.column << "\n";
  return 0;
}

} // namespace pimsim
