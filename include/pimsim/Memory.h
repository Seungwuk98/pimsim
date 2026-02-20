#ifndef PIMSIM_MEMORY_H
#define PIMSIM_MEMORY_H

#include "common.h"
#include "configuration.h"
#include "pimsim/CastingBase.h"
#include "pimsim/Context.h"
#include "pimsim/Serde.h"
#include "pimsim/TypeSupport.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include <cstddef>
#include <filesystem>
#include <memory>
#include <type_traits>

namespace pimsim {

using Byte = std::uint8_t;
class Channel;
class Rank;
class BankGroup;
class Bank;

class Memory : public CommandExecutor {
public:
  Memory(Context *ctx, std::unique_ptr<dramsim3::Config> cfg,
         std::vector<std::unique_ptr<Channel>> &&chs)
      : CommandExecutor(CommandExecutor::Type::Memory), config(std::move(cfg)),
        context(ctx), channels(std::move(chs)) {
    connectHierarchy();
  }
  virtual ~Memory();

  Channel *getChannel(size_t channel_id);

  void printHelp() const;
  void printMemoryConfiguration() const;
  int globalCommand(llvm::ArrayRef<llvm::StringRef> args);

  virtual bool writeTo(const std::filesystem::path &path) const;
  virtual bool readFrom(const std::filesystem::path &path);
  std::string getExecutorName() const override { return "Memory"; }
  size_t write(llvm::ArrayRef<Byte> data, size_t channel, size_t rank,
               size_t bankgroup, size_t bank, size_t row, size_t column);

  size_t read(llvm::MutableArrayRef<Byte> data, size_t channel, size_t rank,
              size_t bankgroup, size_t bank, size_t row, size_t column);

  size_t totalMemorySize() const;

  Context *getContext() const { return context; }
  const dramsim3::Config &getConfig() const { return *config; }

  static bool classof(const CommandExecutor *executor) {
    return executor->isMemory();
  }

private:
  void connectHierarchy();

  Context *context;
  std::unique_ptr<dramsim3::Config> config;
  std::vector<std::unique_ptr<Channel>> channels;
};

class Channel : public CastingBase<Channel> {
public:
  Channel(Context *ctx, std::vector<std::unique_ptr<Rank>> &&rks);
  virtual ~Channel();

  Rank *getRank(size_t rank_id);

  virtual bool writeTo(const std::filesystem::path &path) const;
  virtual bool readFrom(const std::filesystem::path &path);
  Memory *getParentMemory() const { return parentMemory; }

  size_t write(llvm::ArrayRef<Byte> data, size_t rank, size_t bankgroup,
               size_t bank, size_t row, size_t column);
  size_t read(llvm::MutableArrayRef<Byte> data, size_t rank, size_t bankgroup,
              size_t bank, size_t row, size_t column);

  Context *getContext() const { return context; }

protected:
  Channel(size_t typeID, Context *ctx,
          std::vector<std::unique_ptr<Rank>> &&rks);

  Context *context;
  std::vector<std::unique_ptr<Rank>> ranks;

private:
  friend class Memory;
  Memory *parentMemory = nullptr;
};

class Rank {
public:
  Rank(Context *ctx, std::vector<std::unique_ptr<BankGroup>> &&bgs)
      : context(ctx), bankGroups(std::move(bgs)) {}
  virtual ~Rank();
  BankGroup *getBankGroup(size_t bg_id);

  virtual bool writeTo(const std::filesystem::path &path) const;
  virtual bool readFrom(const std::filesystem::path &path);

  Channel *getParentChannel() const { return parentChannel; }

  size_t write(llvm::ArrayRef<Byte> data, size_t bankgroup, size_t bank,
               size_t row, size_t column);
  size_t read(llvm::MutableArrayRef<Byte> data, size_t bankgroup, size_t bank,
              size_t row, size_t column);

private:
  friend class Memory;
  Context *context;
  std::vector<std::unique_ptr<BankGroup>> bankGroups;

  Channel *parentChannel = nullptr;
};

class BankGroup : public CastingBase<BankGroup> {
public:
  BankGroup(Context *ctx, std::vector<std::unique_ptr<Bank>> &&bks);
  virtual ~BankGroup();
  Bank *getBank(size_t bank_id);

  virtual bool writeTo(const std::filesystem::path &path) const;
  virtual bool readFrom(const std::filesystem::path &path);

  Rank *getParentRank() const { return parentRank; }
  Channel *getParentChannel() const { return parentRank->getParentChannel(); }

  size_t write(llvm::ArrayRef<Byte> data, size_t bank, size_t row,
               size_t column);
  size_t read(llvm::MutableArrayRef<Byte> data, size_t bank, size_t row,
              size_t column);

protected:
  BankGroup(size_t typeID, Context *ctx,
            std::vector<std::unique_ptr<Bank>> &&bks);

  Context *context;
  std::vector<std::unique_ptr<Bank>> banks;

  Rank *parentRank = nullptr;

private:
  friend class Memory;
};

template <typename T, typename Enable = void> struct RowPrinter;

class Bank : public CastingBase<Bank> {
public:
  Bank(Context *ctx, size_t numRows, size_t columnSize);
  Bank(Context *ctx);
  virtual ~Bank();

  virtual void activate(size_t row);
  virtual void precharge();
  virtual void read(size_t column, Byte *dst, size_t size);
  virtual void write(size_t column, const Byte *src, size_t size);
  virtual void refresh() {};
  virtual void writeTo(llvm::raw_ostream &os) const;
  virtual llvm::ArrayRef<Byte> readFrom(llvm::ArrayRef<Byte>);

  // Higher-level read/write that handles multi-bank access
  size_t write(llvm::ArrayRef<Byte> data, size_t row, size_t column);
  size_t read(llvm::MutableArrayRef<Byte> data, size_t row, size_t column);

  // Hook for subclasses to implement additional behavior during activation for
  // higher-level read/write
  virtual void doActivateHook(size_t row);

  struct RowBuffer {
    size_t row;
    bool isOpen;
    llvm::SmallVector<Byte> buffer;
  };

  struct DataArray {
    DataArray() = default;
    ~DataArray();

    Byte *getRow(size_t row) { return data + row * columnSize; }
    const Byte *getRow(size_t row) const { return data + row * columnSize; }

    size_t columnSize;
    size_t numRows;
    Byte *data = nullptr;
  };

  size_t getColumnSize() const { return dataArray.columnSize; }
  size_t getNumRows() const { return dataArray.numRows; }
  RowBuffer &getRowBuffer() { return rowBuffer; }
  DataArray &getData() { return dataArray; }

  template <typename T>
  void inspectRow(llvm::raw_ostream &os, size_t base, size_t row) const {
    std::string header;
    llvm::raw_string_ostream headerStream(header);
    headerStream << "Row(" << llvm::format_hex_no_prefix(row, 4, true) << ")";
    RowPrinter<T>::print(os, base, dataArray.getRow(row), dataArray.columnSize,
                         header);
  }
  template <typename T> void inspectRowBuffer(llvm::raw_ostream &os) const {
    if (!rowBuffer.isOpen) {
      os << "Row buffer is not open.\n";
      return;
    }
    std::string header;
    llvm::raw_string_ostream headerStream(header);
    headerStream << "Row Buffer("
                 << llvm::format_hex_no_prefix(rowBuffer.row, 4, true) << ")";
    RowPrinter<T>::print(os, 0, rowBuffer.buffer.data(), dataArray.columnSize,
                         header);
  }

  BankGroup *getParentBankGroup() const { return parentBankGroup; }
  Rank *getParentRank() const { return getParentBankGroup()->getParentRank(); }
  Channel *getParentChannel() const {
    return getParentRank()->getParentChannel();
  }

protected:
  Bank(size_t typeID, Context *ctx);
  Bank(size_t typeID, Context *ctx, size_t numRows, size_t columnSize);

  Context *context;
  DataArray dataArray;
  RowBuffer rowBuffer;

  BankGroup *parentBankGroup = nullptr;

private:
  friend class Memory;
  friend class DefaultBankEncoder;
  friend class Serde<Bank>;
};

template <typename T>
struct RowPrinter<T,
                  std::enable_if_t<std::conjunction_v<std::is_integral<T>>>> {
  static void print(llvm::raw_ostream &os, size_t base, const Byte *data,
                    size_t size, llvm::StringRef header) {
    assert(size % sizeof(T) == 0 &&
           "Row size must be multiple of the data type size");
    size_t numElements = size / sizeof(T);
    constexpr size_t width = TypeSupport<T>::dataWidth();
    os << header;

    std::vector<T> elements(numElements);
    std::memcpy(elements.data(), data, size);

    for (size_t i = 0; i < numElements; ++i) {
      os << ' ';
      std::string data = std::to_string(elements[i]);
      assert(data.length() <= width &&
             "Data length exceeds defined width for type");
      os << std::string(width - data.length(), '0') << data;
    }

    os << '\n';
  }
};

template <typename T>
struct RowPrinter<T, std::enable_if_t<IsFloat<T>::value>> {
  static void print(llvm::raw_ostream &os, size_t base, const Byte *data,
                    size_t size, llvm::StringRef header) {
    assert(size % sizeof(T) == 0 &&
           "Row size must be multiple of the data type size");
    size_t numElements = size / sizeof(T);
    constexpr size_t width = TypeSupport<T>::dataWidth();
    size_t bitwidth = sizeof(T) * 8;
    os << header;

    for (size_t i = 0; i < numElements; ++i) {
      typename IsFloat<T>::bitRep bits = 0;
      std::memcpy(&bits, data + i * sizeof(T), sizeof(T));
      os << ' ';
      TypeSupport<T>::print(
          os, llvm::ArrayRef<Byte>(data + i * sizeof(T), sizeof(T)));
    }
    os << '\n';
  }

  static const auto &getSemantics() {
    if constexpr (std::is_same_v<T, f16>) {
      return llvm::APFloat::IEEEhalf();
    } else if constexpr (std::is_same_v<T, bf16>) {
      return llvm::APFloat::BFloat();
    } else {
      static_assert(std::is_floating_point_v<T>,
                    "T must be a floating point type");
      if constexpr (std::is_same_v<T, float>) {
        return llvm::APFloat::IEEEsingle();
      } else if constexpr (std::is_same_v<T, double>) {
        return llvm::APFloat::IEEEdouble();
      } else {
        static_assert(sizeof(T) == 0, "Unsupported floating point type");
      }
    }
  }
};

template <typename T> using IsFloatV = typename IsFloat<T>::value;

size_t parseAddress(llvm::StringRef addrStr, Context *ctx);
dramsim3::Address getDRAMAddress(const dramsim3::Config &config,
                                 size_t address);

} // namespace pimsim

#endif // PIMSIM_MEMORY_H
