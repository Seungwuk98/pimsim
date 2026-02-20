#include "pimsim/Controller.h"
#include "pimsim/TypeSupport.h"
#include "llvm/ADT/DenseMap.h"

namespace pimsim {

int Controller::command(llvm::ArrayRef<llvm::StringRef> args) {
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
                  .Case("buffer",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          return bufferCommand(args);
                        })
                  .Case("memories",
                        [&](llvm::ArrayRef<llvm::StringRef> args) {
                          printMemories();
                          return 0;
                        })
                  // Add more command cases here
                  .Default(nullptr);

  if (func) {
    return func(args.slice(1));
  } else {
    return -2;
  }
}

void Controller::printHelp() const {
  llvm::raw_ostream &os = getContext()->getOS();
  os << "Controller Commands:\n";
  os << "  help                           - Show this help message\n";
  os << "  buffer <subcommand> [args...]   - Buffer related commands\n";
  os << "         show  <buf_idx>          - Show the contents of buffer index "
        "<buf_idx>\n";
  os << "         store <buf_idx>\n";
  os << "                          <address> - Store the decoded address of "
        "<address> in buffer index "
     << "<buf_idx>\n";
  os << "                          encode <mem_idx> ch=<channel> ra=<rank> "
        "bg=<bankgroup> ba=<bank> "
        "row=<row> col=<column> <buf_idx> - Encode the given address "
        "components and store in buffer index <buf_idx>\n";
  os << "         clear <buf_idx>?         - Clear the buffer index <buf_idx> "
        "(optional, default is all)\n";
  os << "  memories                       - List all connected memories\n";
}

void Controller::printMemories() const {
  llvm::raw_ostream &os = getContext()->getOS();
  os << "Connected Memories:\n";
  for (size_t i = 0; i < memories.size(); ++i) {
    os << "  [" << i << "] " << memories[i]->getExecutorName() << "\n";
  }
}

int Controller::bufferCommand(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.size() < 2) {
    getContext()->getERR() << "Usage: buffer <subcommand> [args...]\n";
    return -1;
  }

  llvm::StringRef subCmd = args[0];
  if (subCmd == "show") {
    if (args.size() != 2) {
      getContext()->getERR() << "Usage: buffer show <buf_idx>\n";
      return -1;
    }
    size_t bufIdx = 0;
    llvm::StringRef bufIdxStr = args[1];
    if (bufIdxStr[0] == '$') {
      bufIdxStr = bufIdxStr.drop_front();
    }

    if (bufIdxStr.getAsInteger(0, bufIdx)) {
      getContext()->getERR() << "Invalid buffer index: " << args[1] << "\n";
      return -1;
    }
    bufferShow(getContext()->getOS(), bufIdx);
    return 0;
  } else if (subCmd == "store") {
    if (args.size() < 3) {
      getContext()->getERR() << "Usage: buffer store <buf_idx> <address>\n";
      getContext()->getERR()
          << "   or: buffer store <buf_idx> encode <mem_idx> ch=<channel> "
             "ra=<rank> bg=<bankgroup> ba=<bank> row=<row> col=<column>\n";
      return -1;
    }
    size_t bufIdx = 0;
    llvm::StringRef bufIdxStr = args[1];
    if (bufIdxStr[0] == '$') {
      bufIdxStr = bufIdxStr.drop_front();
    }

    if (bufIdxStr.getAsInteger(0, bufIdx)) {
      getContext()->getERR() << "Invalid buffer index: " << args[1] << "\n";
      return -1;
    }
    if (args[2] == "encode") {
      if (args.size() < 4) {
        getContext()->getERR()
            << "Usage: buffer store <buf_idx> encode <mem_idx> "
               "ch=<channel> ra=<rank> bg=<bankgroup> "
               "ba=<bank> row=<row> col=<column>\n";
        return -1;
      }
      auto address = parseBufferAddress(args.slice(3));
      if (address.first == static_cast<size_t>(-1)) {
        return -1;
      }
      bufferIn(address.first, address.second, bufIdx);
      return 0;
    } else {
      if (args.size() != 3) {
        getContext()->getERR() << "Usage: buffer store <buf_idx> <address>\n";
        return -1;
      }
      size_t address = parseAddress(args[2], getContext());
      if (address == static_cast<size_t>(-1)) {
        return -1;
      }
      bufferIn(address, bufIdx);
      return 0;
    }
  } else if (subCmd == "clear") {
    if (args.size() == 1) {
      bufferClear();
      return 0;
    } else if (args.size() == 2) {
      size_t bufIdx = 0;
      if (args[1].getAsInteger(0, bufIdx)) {
        getContext()->getERR() << "Invalid buffer index: " << args[1] << "\n";
        return -1;
      }
      if (bufIdx < bufferedAddresses.size()) {
        bufferedAddresses[bufIdx] = std::monostate{};
        return 0;
      } else {
        getContext()->getERR()
            << "Buffer index out of range: " << bufIdx << "\n";
        return -1;
      }
    } else {
      getContext()->getERR() << "Usage: buffer clear <buf_idx>?\n";
      return -1;
    }
  } else {
    getContext()->getERR() << "Unknown buffer subcommand: " << subCmd << "\n";
    return -1;
  }
}

std::pair<Memory *, dramsim3::Address>
Controller::decodeAddress(size_t address) const {
  auto [memIdx, dramAddr] = decodeAddressWithMemoryIndex(address);
  if (memIdx != static_cast<size_t>(-1) && memIdx < memories.size()) {
    return {memories[memIdx], dramAddr};
  }
  context->getERR() << "Address " << address
                    << " does not correspond to any memory.\n";
  return {nullptr, dramsim3::Address()};
}

std::pair<size_t, dramsim3::Address>
Controller::decodeAddressWithMemoryIndex(size_t address) const {
  for (auto [idx, mem] : llvm::enumerate(memories)) {
    if (address < mem->totalMemorySize()) {
      address <<=
          mem->getConfig().shift_bits; // Shift to get the actual DRAM address
      dramsim3::Address dramAddr = getDRAMAddress(mem->getConfig(), address);
      return {idx, dramAddr};
    } else {
      address -= mem->totalMemorySize();
    }
  }
  return {static_cast<size_t>(-1), dramsim3::Address()};
}

size_t Controller::encodeAddress(int memoryIndex, int channel, int rank,
                                 int bankgroup, int bank, int row,
                                 int column) const {
  if (memoryIndex < 0 || memoryIndex >= static_cast<int>(memories.size())) {
    getContext()->getERR() << "Invalid memory index: " << memoryIndex << "\n";
    return static_cast<size_t>(-1);
  }

  Memory *mem = memories[memoryIndex];

  // -1 indicates that the component is not specified and it is not used in dram
  // behavior
  if (channel < -1 || channel >= static_cast<int>(mem->getConfig().channels)) {
    getContext()->getERR() << "Invalid channel: " << channel << "\n";
    return static_cast<size_t>(-1);
  }

  if (rank < -1 || rank >= static_cast<int>(mem->getConfig().ranks)) {
    getContext()->getERR() << "Invalid rank: " << rank << "\n";
    return static_cast<size_t>(-1);
  }

  if (bankgroup < -1 ||
      bankgroup >= static_cast<int>(mem->getConfig().bankgroups)) {
    getContext()->getERR() << "Invalid bank group: " << bankgroup << "\n";
    return static_cast<size_t>(-1);
  }

  if (bank < -1 || bank >= static_cast<int>(mem->getConfig().banks)) {
    getContext()->getERR() << "Invalid bank: " << bank << "\n";
    return static_cast<size_t>(-1);
  }

  if (row < -1 || row >= static_cast<int>(mem->getConfig().rows)) {
    getContext()->getERR() << "Invalid row: " << row << "\n";
    return static_cast<size_t>(-1);
  }

  if (column < -1 || column >= static_cast<int>(mem->getConfig().columns)) {
    getContext()->getERR() << "Invalid column: " << column << "\n";
    return static_cast<size_t>(-1);
  }

  size_t address = 0;
  const auto &config = mem->getConfig();
  address |= static_cast<size_t>(channel & config.ch_mask) << config.ch_pos;
  address |= static_cast<size_t>(rank & config.ra_mask) << config.ra_pos;
  address |= static_cast<size_t>(bankgroup & config.bg_mask) << config.bg_pos;
  address |= static_cast<size_t>(bank & config.bg_mask) << config.ba_pos;
  address |= static_cast<size_t>(row & config.ro_mask) << config.ro_pos;
  address |= static_cast<size_t>(column & config.co_mask) << config.co_pos;

  // Add the offset of the memory in the controller's address space
  for (int i = 0; i < memoryIndex; ++i) {
    address += memories[i]->totalMemorySize();
  }

  return address;
}

std::pair<Memory *, dramsim3::Address> Controller::bufferOut(size_t idx) const {
  if (idx < bufferedAddresses.size()) {
    const auto &entry = bufferedAddresses[idx];
    return std::visit(
        [&](const auto &value) -> std::pair<Memory *, dramsim3::Address> {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, std::monostate>) {
            getContext()->getERR() << "Buffer index " << idx << " is empty.\n";
            return std::make_pair(nullptr, dramsim3::Address());
          } else if constexpr (std::is_same_v<
                                   T, std::pair<size_t, dramsim3::Address>>) {
            size_t memIdx = value.first;
            dramsim3::Address dramAddr = value.second;
            if (memIdx < memories.size()) {
              return std::make_pair(memories[memIdx], dramAddr);
            } else {
              getContext()->getERR()
                  << "Memory index out of range: " << memIdx << "\n";
              return std::make_pair(nullptr, dramsim3::Address());
            }
          } else if constexpr (std::is_same_v<T, size_t>) {
            size_t address = value;
            return decodeAddress(address);
          }
        },
        entry);
  } else {
    getContext()->getERR() << "Buffer index out of range: " << idx << "\n";
    return {nullptr, dramsim3::Address()};
  }
}

void Controller::bufferShow(llvm::raw_ostream &os, size_t idx) const {
  if (idx < bufferedAddresses.size()) {
    const auto &entry = bufferedAddresses[idx];
    std::visit(
        [&](const auto &value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, std::monostate>) {
            os << "<empty>";
          } else if constexpr (std::is_same_v<
                                   T, std::pair<size_t, dramsim3::Address>>) {
            size_t memIdx = value.first;
            dramsim3::Address dramAddr = value.second;
            os << "mem = " << memIdx << ", {ch=" << dramAddr.channel
               << ", ra=" << dramAddr.rank << ", bg=" << dramAddr.bankgroup
               << ", ba=" << dramAddr.bank << ", row=" << dramAddr.row
               << ", col=" << dramAddr.column << "}";
          } else if constexpr (std::is_same_v<T, size_t>) {
            size_t address = value;
            os << llvm::format_hex(address, 8, true);
          }
        },
        entry);
  } else {
    os << "Buffer index out of range: " << idx << "\n";
  }
}

std::pair<Memory *, dramsim3::Address>
Controller::getAddress(llvm::StringRef arg) const {
  if (arg.starts_with("$")) {
    arg = arg.drop_front();
    size_t bufIdx = 0;
    if (arg.getAsInteger(0, bufIdx)) {
      getContext()->getERR() << "Invalid buffer index: " << arg << "\n";
      return {nullptr, dramsim3::Address()};
    }

    if (bufIdx < bufferedAddresses.size()) {
      return bufferOut(bufIdx);
    } else {
      getContext()->getERR() << "Buffer index out of range: " << bufIdx;
      getContext()->getERR()
          << ", Total buffers: " << bufferedAddresses.size() << "\n";
      return {nullptr, dramsim3::Address()};
    }
  }

  size_t address = parseAddress(arg, getContext());
  if (address == static_cast<size_t>(-1)) {
    return {nullptr, dramsim3::Address()};
  }
  return decodeAddress(address);
}

void Controller::parseAndShow(llvm::StringRef arg,
                              llvm::raw_ostream &os) const {
  if (arg.starts_with("$")) {
    arg = arg.drop_front();
    size_t bufIdx = 0;
    if (arg.getAsInteger(0, bufIdx)) {
      getContext()->getERR() << "Invalid buffer index: " << arg << "\n";
      return;
    }
    os << arg << "(";
    bufferShow(os, bufIdx);
    os << ")";
  } else {
    size_t address = parseAddress(arg, getContext());
    if (address == static_cast<size_t>(-1)) {
      return;
    }
    os << llvm::format_hex(address, 8, true);
  }
}

static int getOrDefault(llvm::DenseMap<llvm::StringRef, int> &map,
                        llvm::StringRef key, int defaultValue) {
  auto it = map.find(key);
  if (it != map.end()) {
    return it->second;
  }
  return defaultValue;
}

std::pair<size_t, dramsim3::Address>
Controller::parseBufferAddress(llvm::ArrayRef<llvm::StringRef> args) const {
  if (args.size() < 1) {
    getContext()->getERR() << "Expected memory index and address components.\n";
    return {static_cast<size_t>(-1), dramsim3::Address()};
  }

  llvm::DenseMap<llvm::StringRef, int> components;

  int memoryIndex = -1;
  if (args[0].getAsInteger(0, memoryIndex) || memoryIndex < 0 ||
      memoryIndex >= static_cast<int>(memories.size())) {
    getContext()->getERR() << "Invalid memory index: " << args[0] << "\n";
    return {static_cast<size_t>(-1), dramsim3::Address()};
  }

  for (size_t i = 1; i < args.size(); ++i) {
    auto kv = llvm::StringRef(args[i]).split('=');
    if (kv.second.empty()) {
      getContext()->getERR() << "Invalid component format: " << args[i] << "\n";
      return {static_cast<size_t>(-1), dramsim3::Address()};
    }
    if (kv.first != "ch" && kv.first != "ra" && kv.first != "bg" &&
        kv.first != "ba" && kv.first != "row" && kv.first != "col") {
      getContext()->getERR() << "Unknown component: " << kv.first << "\n";
      return {static_cast<size_t>(-1), dramsim3::Address()};
    }
    int value = 0;
    if (kv.second.getAsInteger(0, value)) {
      getContext()->getERR()
          << "Invalid value for " << kv.first << ": " << kv.second << "\n";
      return {static_cast<size_t>(-1), dramsim3::Address()};
    }
    components[kv.first] = value;
  }

  const auto &config = memories[memoryIndex]->getConfig();
  int channel = getOrDefault(components, "ch", -1);
  int rank = getOrDefault(components, "ra", -1);
  int bankgroup = getOrDefault(components, "bg", -1);
  int bank = getOrDefault(components, "ba", -1);
  int row = getOrDefault(components, "row", -1);
  int column = getOrDefault(components, "col", -1);

  if (channel < -1 || channel >= static_cast<int>(config.channels)) {
    getContext()->getERR() << "Invalid channel: " << channel << "\n";
    return {static_cast<size_t>(-1), dramsim3::Address()};
  }
  if (rank < -1 || rank >= static_cast<int>(config.ranks)) {
    getContext()->getERR() << "Invalid rank: " << rank << "\n";
    return {static_cast<size_t>(-1), dramsim3::Address()};
  }
  if (bankgroup < -1 || bankgroup >= static_cast<int>(config.bankgroups)) {
    getContext()->getERR() << "Invalid bank group: " << bankgroup << "\n";
    return {static_cast<size_t>(-1), dramsim3::Address()};
  }
  if (bank < -1 || bank >= static_cast<int>(config.banks)) {
    getContext()->getERR() << "Invalid bank: " << bank << "\n";
    return {static_cast<size_t>(-1), dramsim3::Address()};
  }
  if (row < -1 || row >= static_cast<int>(config.rows)) {
    getContext()->getERR() << "Invalid row: " << row << "\n";
    return {static_cast<size_t>(-1), dramsim3::Address()};
  }
  if (column < -1 || column >= static_cast<int>(config.columns)) {
    getContext()->getERR() << "Invalid column: " << column << "\n";
    return {static_cast<size_t>(-1), dramsim3::Address()};
  }
  return {static_cast<size_t>(memoryIndex),
          dramsim3::Address{channel, rank, bankgroup, bank, row, column}};
}

} // namespace pimsim
