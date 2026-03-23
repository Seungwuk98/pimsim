#ifndef PIMSIM_CONTEXT_H
#define PIMSIM_CONTEXT_H

#include "configuration.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

namespace pimsim {

class Memory;
enum class MemoryType { DRAM, Newton, Neupims, Quokka };
enum class ControllerType { DefaultDRAM, Newton, Neupims, Quokka };

template <MemoryType MT> struct MemoryConstructImpl;
template <ControllerType CT> struct ControllerConstructImpl;

class Context {
public:
  Context(llvm::StringRef logDir, llvm::raw_ostream &out = llvm::outs(),
          llvm::raw_ostream &err = llvm::errs())
      : logDirectory(logDir), out(out), err(err) {}

  enum PrintOption {
    Binary,
    Hexadecimal,
  };

  template <MemoryType MT, typename... Args> auto createMemory(Args &&...args) {
    return MemoryConstructImpl<MT>::construct(this,
                                              std::forward<Args>(args)...);
  }

  template <ControllerType CT, typename... Args>
  auto createController(Args &&...args) {
    return ControllerConstructImpl<CT>::construct(this,
                                                  std::forward<Args>(args)...);
  }

  llvm::raw_ostream &getOS() { return out; }
  llvm::raw_ostream &getERR() { return err; }
  llvm::StringRef getLogDirectory() const { return logDirectory; }

  std::unique_ptr<dramsim3::Config> createConfig(llvm::StringRef configFile);

private:
  llvm::StringRef logDirectory;
  llvm::raw_ostream &out;
  llvm::raw_ostream &err;
};

class Channel;
class Rank;
class BankGroup;
class Bank;

template <typename ConcreteMemory, typename ChannelT, typename RankT,
          typename BankGroupT, typename BankT>
struct MemoryConstructor {
  static std::unique_ptr<ConcreteMemory>
  construct(Context *ctx, std::unique_ptr<dramsim3::Config> config) {
    auto banks = config->banks_per_group;
    auto bankGroups = config->bankgroups;
    auto ranks = config->ranks;
    auto channels = config->channels;

    std::vector<std::unique_ptr<Channel>> channelVec;
    channelVec.reserve(channels);
    for (size_t ch = 0; ch < channels; ++ch) {
      std::vector<std::unique_ptr<Rank>> rankVec;
      rankVec.reserve(ranks);
      for (size_t ra = 0; ra < ranks; ++ra) {
        std::vector<std::unique_ptr<BankGroup>> bgVec;
        bgVec.reserve(bankGroups);
        for (size_t bg = 0; bg < bankGroups; ++bg) {
          std::vector<std::unique_ptr<Bank>> bankVec;
          bankVec.reserve(banks);
          for (size_t b = 0; b < banks; ++b)
            bankVec.emplace_back(
                std::make_unique<BankT>(ctx, config->rows, config->columns));
          bgVec.emplace_back(
              std::make_unique<BankGroupT>(ctx, std::move(bankVec)));
        }
        rankVec.emplace_back(std::make_unique<RankT>(ctx, std::move(bgVec)));
      }
      channelVec.emplace_back(
          std::make_unique<ChannelT>(ctx, std::move(rankVec)));
    }
    return std::make_unique<ConcreteMemory>(ctx, std::move(config),
                                            std::move(channelVec));
  }
};

class CommandExecutor {
public:
  enum class Type { REPL, Controller, Memory };
  virtual ~CommandExecutor() = default;

  virtual int command(llvm::ArrayRef<llvm::StringRef> args) = 0;
  virtual std::string getExecutorName() const = 0;
  virtual CommandExecutor *getExecutor(size_t idx) const {
    llvm_unreachable("not implemented intoScope for this executor type");
  }

  bool isREPL() const { return type == Type::REPL; }
  bool isController() const { return type == Type::Controller; }
  bool isMemory() const { return type == Type::Memory; }

protected:
  CommandExecutor(Type type) : type(type) {}
  Type type;
};

} // namespace pimsim

#endif // PIMSIM_CONTEXT_H
