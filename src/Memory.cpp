#include "pimsim/Memory.h"
#include "common.h"
#include "configuration.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MemoryBuffer.h"
#include <cassert>
#include <memory>

namespace pimsim {

Memory::~Memory() = default;

void Memory::printMemoryConfiguration() const {
  const auto &cfg = getConfig();
  llvm::raw_ostream &os = getContext()->getOS();
  os << "Memory Configuration:\n";
  os << "  Channels: " << cfg.channels << "\n";
  os << "  Ranks per Channel: " << cfg.ranks << "\n";
  os << "  Bank Groups per Rank: " << cfg.bankgroups << "\n";
  os << "  Banks per Bank Group: " << cfg.banks << "\n";
  os << "  Rows per Bank: " << cfg.rows << "\n";
  os << "  Columns per Row: " << cfg.columns << "\n";
  os << "  Shift Bits: " << cfg.shift_bits << "\n";
}

size_t Memory::totalMemorySize() const {
  size_t totalSize = getConfig().channels * getConfig().ranks *
                     getConfig().bankgroups * getConfig().banks *
                     getConfig().rows * getConfig().columns * sizeof(Byte);
  return totalSize;
}

bool Memory::writeTo(const std::filesystem::path &path) const {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  if (ec) {
    llvm::errs() << "Failed to create directory: " << ec.message() << "\n";
    return true;
  }
  for (size_t c = 0; c < channels.size(); ++c) {
    auto channelPath = path / std::to_string(c);
    if (channels[c]->writeTo(channelPath)) {
      return true;
    }
  }
  return false;
}

size_t Memory::write(llvm::ArrayRef<Byte> data, size_t channel, size_t rank,
                     size_t bankgroup, size_t bank, size_t row, size_t column) {
  assert(channel < channels.size() && "Invalid channel ID");
  size_t totalWritten = 0;
  for (size_t ch = channel; ch < channels.size(); ++ch) {
    Channel *chn = channels[ch].get();
    auto written = chn->write(data, rank, bankgroup, bank, row, column);
    totalWritten += written;
    if (written < data.size()) {
      data = data.slice(written);
      rank = 0;
      bankgroup = 0;
      bank = 0;
      row = 0;
      column = 0;
    } else {
      assert(written == data.size() && "Wrote more than requested size");
      break;
    }
  }
  return totalWritten;
}

size_t Memory::read(llvm::MutableArrayRef<Byte> data, size_t channel,
                    size_t rank, size_t bankgroup, size_t bank, size_t row,
                    size_t column) {
  assert(channel < channels.size() && "Invalid channel ID");
  size_t totalRead = 0;
  for (size_t ch = channel; ch < channels.size(); ++ch) {
    Channel *chn = channels[ch].get();
    auto read = chn->read(data, rank, bankgroup, bank, row, column);
    totalRead += read;
    if (read < data.size()) {
      data = data.slice(read);
      rank = 0;
      bankgroup = 0;
      bank = 0;
      row = 0;
      column = 0;
    } else {
      assert(read == data.size() && "Read more than requested size");
      break;
    }
  }
  return totalRead;
}

bool Memory::readFrom(const std::filesystem::path &path) {
  if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
    llvm::errs() << "Path does not exist or is not a directory: "
                 << path.string() << "\n";
    return true;
  }
  for (size_t c = 0; c < channels.size(); ++c) {
    auto channelPath = path / std::to_string(c);
    if (!std::filesystem::exists(channelPath) ||
        !std::filesystem::is_directory(channelPath)) {
      llvm::errs()
          << "Warning: Channel path does not exist or is not a directory: "
          << channelPath.string() << "\n";
      continue;
    }
    if (channels[c]->readFrom(channelPath))
      return true;
  }
  return false;
}

void Memory::connectHierarchy() {
  for (auto &channel : channels) {
    channel->parentMemory = this;
    for (auto &rank : channel->ranks) {
      rank->parentChannel = channel.get();
      for (auto &bankGroup : rank->bankGroups) {
        bankGroup->parentRank = rank.get();
        for (auto &bank : bankGroup->banks) {
          bank->parentBankGroup = bankGroup.get();
        }
      }
    }
  }
}

Channel::Channel(Context *ctx, std::vector<std::unique_ptr<Rank>> &&rks)
    : CastingBase(TypeID<Channel>), context(ctx), ranks(std::move(rks)) {}
Channel::Channel(size_t typeID, Context *ctx,
                 std::vector<std::unique_ptr<Rank>> &&rks)
    : CastingBase(typeID), context(ctx), ranks(std::move(rks)) {}
Channel::~Channel() = default;
Channel *Memory::getChannel(size_t channel_id) {
  assert(channel_id < channels.size() && "Invalid channel ID");
  return channels[channel_id].get();
}

size_t Channel::write(llvm::ArrayRef<Byte> data, size_t rank, size_t bankgroup,
                      size_t bank, size_t row, size_t column) {
  assert(rank < ranks.size() && "Invalid rank ID");

  size_t totalWritten = 0;
  for (size_t rankId = rank; rankId < ranks.size(); ++rankId) {
    Rank *rnk = ranks[rankId].get();
    auto written = rnk->write(data, bankgroup, bank, row, column);
    totalWritten += written;
    if (written < data.size()) {
      data = data.slice(written);
      bankgroup = 0;
      bank = 0;
      row = 0;
      column = 0;
    } else {
      assert(written == data.size() && "Wrote more than requested size");
      break;
    }
  }

  return totalWritten;
}

size_t Channel::read(llvm::MutableArrayRef<Byte> data, size_t rank,
                     size_t bankgroup, size_t bank, size_t row, size_t column) {
  assert(rank < ranks.size() && "Invalid rank ID");

  size_t totalRead = 0;
  for (size_t rankId = rank; rankId < ranks.size(); ++rankId) {
    Rank *rnk = ranks[rankId].get();
    auto read = rnk->read(data, bankgroup, bank, row, column);
    totalRead += read;
    if (read < data.size()) {
      data = data.slice(read);
      bankgroup = 0;
      bank = 0;
      row = 0;
      column = 0;
    } else {
      assert(read == data.size() && "Read more than requested size");
      break;
    }
  }

  return totalRead;
}

bool Channel::writeTo(const std::filesystem::path &path) const {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  if (ec) {
    llvm::errs() << "Failed to create directory: " << ec.message() << "\n";
    return true;
  }
  for (size_t r = 0; r < ranks.size(); ++r) {
    auto rankPath = path / std::to_string(r);
    if (ranks[r]->writeTo(rankPath)) {
      return true;
    }
  }
  return false;
}

bool Channel::readFrom(const std::filesystem::path &path) {
  if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
    llvm::errs() << "Path does not exist or is not a directory: "
                 << path.string() << "\n";
    return true;
  }
  for (size_t r = 0; r < ranks.size(); ++r) {
    auto rankPath = path / std::to_string(r);
    if (!std::filesystem::exists(rankPath) ||
        !std::filesystem::is_directory(rankPath)) {
      llvm::errs()
          << "Warning: Rank path does not exist or is not a directory: "
          << rankPath.string() << "\n";
      continue;
    }
    if (ranks[r]->readFrom(rankPath))
      return true;
  }
  return false;
}

Rank::~Rank() = default;
Rank *Channel::getRank(size_t rank_id) {
  assert(rank_id < ranks.size() && "Invalid rank ID");
  return ranks[rank_id].get();
}

size_t Rank::write(llvm::ArrayRef<Byte> data, size_t bankgroup, size_t bank,
                   size_t row, size_t column) {
  assert(bankgroup < bankGroups.size() && "Invalid bank group ID");

  size_t totalWritten = 0;
  for (size_t bg = bankgroup; bg < bankGroups.size(); ++bg) {
    BankGroup *bgp = bankGroups[bg].get();
    auto written = bgp->write(data, bank, row, column);
    totalWritten += written;
    if (written < data.size()) {
      data = data.slice(written);
      bank = 0;
      row = 0;
      column = 0;
    } else {
      assert(written == data.size() && "Wrote more than requested size");
      break;
    }
  }

  return totalWritten;
}

size_t Rank::read(llvm::MutableArrayRef<Byte> data, size_t bankgroup,
                  size_t bank, size_t row, size_t column) {
  assert(bankgroup < bankGroups.size() && "Invalid bank group ID");

  size_t totalRead = 0;
  for (size_t bg = bankgroup; bg < bankGroups.size(); ++bg) {
    BankGroup *bgp = bankGroups[bg].get();
    auto read = bgp->read(data, bank, row, column);
    totalRead += read;
    if (read < data.size()) {
      data = data.slice(read);
      bank = 0;
      row = 0;
      column = 0;
    } else {
      assert(read == data.size() && "Read more than requested size");
      break;
    }
  }

  return totalRead;
}

bool Rank::writeTo(const std::filesystem::path &path) const {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  if (ec) {
    llvm::errs() << "Failed to create directory: " << ec.message() << "\n";
    return true;
  }
  for (size_t bg = 0; bg < bankGroups.size(); ++bg) {
    auto bgPath = path / std::to_string(bg);
    if (bankGroups[bg]->writeTo(bgPath)) {
      return true;
    }
  }
  return false;
}

bool Rank::readFrom(const std::filesystem::path &path) {
  if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
    llvm::errs() << "Path does not exist or is not a directory: "
                 << path.string() << "\n";
    return true;
  }
  for (size_t bg = 0; bg < bankGroups.size(); ++bg) {
    auto bgPath = path / std::to_string(bg);
    if (!std::filesystem::exists(bgPath) ||
        !std::filesystem::is_directory(bgPath)) {
      llvm::errs()
          << "Warning: Bank group path does not exist or is not a directory: "
          << bgPath.string() << "\n";
      continue;
    }
    if (bankGroups[bg]->readFrom(bgPath))
      return true;
  }
  return false;
}

BankGroup::BankGroup(Context *ctx, std::vector<std::unique_ptr<Bank>> &&bks)
    : CastingBase(TypeID<BankGroup>), context(ctx), banks(std::move(bks)) {}
BankGroup::BankGroup(size_t typeID, Context *ctx,
                     std::vector<std::unique_ptr<Bank>> &&bks)
    : CastingBase(typeID), context(ctx), banks(std::move(bks)) {}
BankGroup::~BankGroup() = default;
BankGroup *Rank::getBankGroup(size_t bg_id) {
  assert(bg_id < bankGroups.size() && "Invalid bank group ID");
  return bankGroups[bg_id].get();
}

size_t BankGroup::write(llvm::ArrayRef<Byte> data, size_t bank, size_t row,
                        size_t column) {
  assert(bank < banks.size() && "Invalid bank ID");

  size_t totalWritten = 0;
  for (size_t b = bank; b < banks.size(); ++b) {
    Bank *bnk = banks[b].get();
    auto written = bnk->write(data, row, column);
    totalWritten += written;
    if (written < data.size()) {
      data = data.slice(written);
      row = 0;
      column = 0;
    } else {
      assert(written == data.size() && "Wrote more than requested size");
      break;
    }
  }

  return totalWritten;
}
size_t BankGroup::read(llvm::MutableArrayRef<Byte> data, size_t bank,
                       size_t row, size_t column) {
  assert(bank < banks.size() && "Invalid bank ID");

  size_t totalRead = 0;
  for (size_t b = bank; b < banks.size(); ++b) {
    Bank *bnk = banks[b].get();
    auto read = bnk->read(data, row, column);
    totalRead += read;
    if (read < data.size()) {
      data = data.slice(read);
      row = 0;
      column = 0;
    } else {
      assert(read == data.size() && "Read more than requested size");
      break;
    }
  }

  return totalRead;
}

bool BankGroup::writeTo(const std::filesystem::path &path) const {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  if (ec) {
    llvm::errs() << "Failed to create directory: " << ec.message() << "\n";
    return true;
  }
  for (size_t b = 0; b < banks.size(); ++b) {
    auto bankPath = path / std::to_string(b);
    std::error_code ec;
    llvm::raw_fd_ostream os(bankPath.string(), ec);
    if (ec) {
      llvm::errs() << "Failed to open file: " << ec.message() << "\n";
      return true;
    }
    banks[b]->writeTo(os);
  }
  return false;
}
bool BankGroup::readFrom(const std::filesystem::path &path) {
  for (size_t b = 0; b < banks.size(); ++b) {
    auto bankPath = path / std::to_string(b);
    if (!std::filesystem::exists(bankPath) ||
        !std::filesystem::is_regular_file(bankPath)) {
      llvm::errs()
          << "Warning: Bank file does not exist or is not a regular file: "
          << bankPath.string() << "\n";
      continue;
    }
    auto memoryBuffer = llvm::MemoryBuffer::getFile(bankPath.string());
    if (std::error_code ec = memoryBuffer.getError()) {
      llvm::errs() << "Failed to read file: " << ec.message() << "\n";
      return true;
    }
    auto buffer = memoryBuffer->get()->getBuffer();
    auto leftByte = banks[b]->readFrom(llvm::ArrayRef<Byte>(
        reinterpret_cast<const Byte *>(buffer.data()), buffer.size()));
    assert(leftByte.empty() && "Did not consume all bytes during read");
  }
  return false;
}
Bank *BankGroup::getBank(size_t bank_id) {
  assert(bank_id < banks.size() && "Invalid bank ID");
  return banks[bank_id].get();
}

Bank::~Bank() {}
Bank::Bank(Context *ctx, size_t numRows, size_t columnSize)
    : CastingBase(TypeID<Bank>), context(ctx) {
  dataArray.numRows = numRows;
  dataArray.columnSize = columnSize;
  size_t dataSize = numRows * columnSize;
  dataArray.data = new Byte[dataSize];
  rowBuffer.buffer.resize(columnSize);
  rowBuffer.isOpen = false;
}

Bank::Bank(size_t typeID, Context *ctx) : CastingBase(typeID), context(ctx) {}
Bank::Bank(size_t typeID, Context *ctx, size_t numRows, size_t columnSize)
    : CastingBase(typeID), context(ctx) {
  dataArray.numRows = numRows;
  dataArray.columnSize = columnSize;
  size_t dataSize = numRows * columnSize;
  dataArray.data = new Byte[dataSize];
  rowBuffer.buffer.resize(columnSize);
  rowBuffer.isOpen = false;
}

size_t Bank::write(llvm::ArrayRef<Byte> data, size_t row, size_t column) {
  if (data.empty())
    return 0;

  size_t totalWritten = 0;
  while (!data.empty() && row < getNumRows()) {
    auto idx = row * getColumnSize() + column;
    size_t writableSize = std::min(data.size(), getColumnSize() - column);
    doActivateHook(row);
    write(column, data.data(), writableSize);
    totalWritten += writableSize;
    data = data.slice(writableSize);
    column = 0;
    ++row;
  }

  return totalWritten;
}

size_t Bank::read(llvm::MutableArrayRef<Byte> data, size_t row, size_t column) {
  if (data.empty())
    return 0;

  size_t totalRead = 0;
  while (!data.empty() && row < getNumRows()) {
    auto idx = row * getColumnSize() + column;
    size_t readableSize = std::min(data.size(), getColumnSize() - column);
    doActivateHook(row);
    read(column, data.data(), readableSize);
    totalRead += readableSize;
    data = data.slice(readableSize);
    column = 0;
    ++row;
  }
  return totalRead;
}

void Bank::doActivateHook(size_t row) {
  if (rowBuffer.isOpen) {
    if (rowBuffer.row != row) {
      precharge();
    } else {
      return; // Row is already active
    }
  }
  activate(row);
}

void Bank::activate(size_t row) {
  assert(row < getNumRows() && "Row address out of range");
  // Activation logic here
  assert(rowBuffer.isOpen == false &&
         "Row buffer must be closed before activation");
  Byte *rowPtr = dataArray.getRow(row);
  assert(rowBuffer.buffer.size() == getColumnSize() &&
         "Row buffer size mismatch");

  std::uninitialized_copy(rowPtr, rowPtr + getColumnSize(),
                          rowBuffer.buffer.begin());
  rowBuffer.row = row;
  rowBuffer.isOpen = true;
}

void Bank::precharge() {
  // Precharge logic here
  rowBuffer.isOpen = false;
}

void Bank::read(size_t column, Byte *dst, size_t size) {
  assert(column + size <= getColumnSize() && "Column address out of range");
  assert(rowBuffer.isOpen && "Row buffer is not open");
  memcpy(dst, rowBuffer.buffer.data() + column, size);
}

void Bank::write(size_t column, const Byte *src, size_t size) {
  assert(column + size <= getColumnSize() && "Column address out of range");
  assert(rowBuffer.isOpen && "Row buffer is not open");
  memcpy(rowBuffer.buffer.data() + column, src, size);
  memcpy(dataArray.getRow(rowBuffer.row) + column, src, size);
}

Bank::DataArray::~DataArray() {
  if (data)
    delete[] data;
}

template <> struct Serde<Bank::DataArray> {
  static llvm::ArrayRef<Byte> deserializeImpl(Bank::DataArray &obj,
                                              llvm::ArrayRef<Byte> data) {
    assert(obj.data == nullptr &&
           "DataArray must be empty before deserialization");
    data = deserialize(obj.columnSize, data);
    data = deserialize(obj.numRows, data);
    size_t dataSize = obj.columnSize * obj.numRows;

    auto left = deserialize(obj.data, data);
    auto length = data.size() - left.size();
    data = left;
    assert(length == dataSize && "Data size mismatch during deserialization");
    return data;
  }

  static void serializeImpl(const Bank::DataArray &obj,
                            llvm::SmallVectorImpl<Byte> &out) {
    serialize(obj.columnSize, out);
    serialize(obj.numRows, out);
    serialize(obj.data, out, obj.columnSize * obj.numRows);
  }
};

template <> struct Serde<Bank::RowBuffer> {
  static llvm::ArrayRef<Byte> deserializeImpl(Bank::RowBuffer &obj,
                                              llvm::ArrayRef<Byte> data) {
    data = deserialize(obj.row, data);
    data = deserialize(obj.isOpen, data);
    if (obj.isOpen) {
      data = deserialize(obj.buffer, data);
    }
    return data;
  }

  static void serializeImpl(const Bank::RowBuffer &obj,
                            llvm::SmallVectorImpl<Byte> &out) {
    serialize(obj.row, out);
    serialize(obj.isOpen, out);
    if (obj.isOpen) {
      serialize(obj.buffer, out);
    }
  }
};

template <> struct Serde<Bank> {
  static llvm::ArrayRef<Byte> deserializeImpl(Bank &obj,
                                              llvm::ArrayRef<Byte> data) {
    data = deserialize(obj.dataArray, data);
    data = deserialize(obj.rowBuffer, data);
    return data;
  }

  static void serializeImpl(const Bank &obj, llvm::SmallVectorImpl<Byte> &out) {
    serialize(obj.dataArray, out);
    serialize(obj.rowBuffer, out);
  }
};

llvm::ArrayRef<Byte> Bank::readFrom(llvm::ArrayRef<Byte> data) {
  return deserialize(*this, data);
}

void Bank::writeTo(llvm::raw_ostream &os) const {
  llvm::SmallVector<Byte, 1024> buffer;
  serialize(*this, buffer);
  os << llvm::StringRef(reinterpret_cast<const char *>(buffer.data()),
                        buffer.size());
}

void printRow(llvm::raw_ostream &os, llvm::ArrayRef<Byte> rowData, size_t row,
              size_t widthHelp) {
  os << "Row(" << llvm::format_hex_no_prefix(row, 4, true) << ")";

  for (size_t col = 0; col < rowData.size(); col += 8) {
    os << std::string(widthHelp - 2, ' '); // 3 for "Row"
    for (unsigned group = col; group < col + 8; group += 2) {
      uint16_t value = 0;
      assert(group + 1 < rowData.size() &&
             "Row data size is not aligned to groupByte");
      value |= static_cast<uint16_t>(rowData[group]) << 8;
      value |= static_cast<uint16_t>(rowData[group + 1]);
      os << llvm::format_hex_no_prefix(value, 2, true);
    }
  }
  os << '\n';
}

unsigned printHeader(llvm::raw_ostream &os, llvm::ArrayRef<Byte> rowData,
                     size_t base) {
  auto baseFormat = llvm::format_hex_no_prefix(base, 4, true);
  std::string baseStr;
  llvm::raw_string_ostream ss(baseStr);
  ss << baseFormat;
  auto baseWidth = baseStr.size();

  os << "Address: ";
  for (size_t col = 0; col < rowData.size(); col += 8) {
    os << ' ';
    os << llvm::format_hex_no_prefix(base + col, baseWidth, true);
  }
  os << '\n';
  os << "Column:  ";
  for (size_t col = 0; col < rowData.size(); col += 8) {
    os << ' ';
    os << llvm::format_hex_no_prefix(col, baseWidth, true);
  }
  os << '\n';
  return baseWidth;
}

size_t parseAddress(llvm::StringRef addrStr, Context *ctx) {
  size_t address = 0;
  if (addrStr.getAsInteger(0, address)) {
    ctx->getERR() << "Invalid address format: " << addrStr << "\n";
    return static_cast<size_t>(-1);
  }
  return address;
}

dramsim3::Address getDRAMAddress(const dramsim3::Config &config,
                                 size_t address) {
  return config.AddressMapping(address);
}

} // namespace pimsim
