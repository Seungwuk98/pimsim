#ifndef PIMSIM_PIM_H
#define PIMSIM_PIM_H

#include "common.h"
#include "pimsim/DefaultDRAM.h"
#include "pimsim/Memory.h"
#include "pimsim/PIMSupport.h"
#include <format>

namespace pimsim {

class NewtonBank;
class Newton : public DefaultDRAM {
public:
  using DefaultDRAM::DefaultDRAM;

  int command(llvm::ArrayRef<llvm::StringRef> args) override;
  std::string getExecutorName() const override {
    return std::format("{} - Newton", DefaultDRAM::getExecutorName());
  }

protected:
  int printHelp() const;

private:
  friend class Serde<Newton>;

  int comp(llvm::ArrayRef<llvm::StringRef> args);
  int readRes(llvm::ArrayRef<llvm::StringRef> args);
  int gact(llvm::ArrayRef<llvm::StringRef> args);
  int gwrite(llvm::ArrayRef<llvm::StringRef> args);
  int showGBuffer(llvm::ArrayRef<llvm::StringRef> args);
};

class NewtonChannel
    : public PIMChannel<NewtonChannel, Channel, BufferedChannelTrait> {
public:
  NewtonChannel(Context *ctx, std::vector<std::unique_ptr<Rank>> &&ranks);

  llvm::SmallVector<f16> readResult() const override;
  void comp() override;

  void inspectGlobalBuffer(llvm::raw_ostream &os) {
    if (globalBuffer.buffer.size() == 0) {
      initializeGlobalBuffer();
    }
    RowPrinter<f16>::print(os, 0, globalBuffer.buffer.data(),
                           globalBuffer.buffer.size(), "Global Buffer:");
  }

  static bool classof(const Channel *channel);

protected:
  NewtonChannel(size_t typeID, Context *ctx,
                std::vector<std::unique_ptr<Rank>> &&ranks);

  friend class NewtonBank;
  friend class NewtonFastBank;

  friend class Serde<NewtonChannel>;
};

class NewtonBankGroup : public BankGroup,
                        public ClassOf<NewtonBankGroup, BankGroup> {
public:
  using BankGroup::BankGroup;

  void gact(size_t row);
};

class NewtonBank : public Bank, public ClassOf<NewtonBank, Bank> {
public:
  NewtonBank(Context *ctx, size_t numRows, size_t columnSize);

  virtual void comp();

  f16 getAddResult() const { return addResult; }

  static bool classof(const Bank *bank);

protected:
  NewtonBank(size_t typeID, Context *ctx, size_t numRows, size_t columnSize);
  f16 doCompf16(llvm::ArrayRef<Byte> rowBufferData,
                llvm::ArrayRef<Byte> globalBufferData);

  f16 addResult;

private:
  friend class NewtonChannel;
  friend class Serde<NewtonBank>;
};

class NewtonFastBank : public Bank {
public:
  NewtonFastBank(Context *ctx, size_t numRows, size_t columnSize);

  void comp();

private:
  friend class NewtonChannel;
  friend class Serde<NewtonFastBank>;
  f16 addResult;
};

template <> struct MemoryConstructImpl<MemoryType::Newton> {
  static std::unique_ptr<Newton>
  construct(Context *ctx, std::unique_ptr<dramsim3::Config> config,
            bool fastMode = false) {
    if (fastMode) {
      return MemoryConstructor<Newton, NewtonChannel, Rank, NewtonBankGroup,
                               NewtonFastBank>::construct(ctx,
                                                          std::move(config));
    } else {
      return MemoryConstructor<Newton, NewtonChannel, Rank, NewtonBankGroup,
                               NewtonBank>::construct(ctx, std::move(config));
    }
  }
};

class NewtonController : public DefaultDRAMController {
public:
  using DefaultDRAMController::DefaultDRAMController;

  int command(llvm::ArrayRef<llvm::StringRef> args) override;
  std::string getExecutorName() const override {
    return std::format("{} - NewtonController",
                       DefaultDRAMController::getExecutorName());
  }

private:
  void printHelp() const;
  virtual int comp(llvm::ArrayRef<llvm::StringRef> args);
  virtual int readRes(llvm::ArrayRef<llvm::StringRef> args);
  int gwrite(llvm::ArrayRef<llvm::StringRef> args);
  int showRes(llvm::ArrayRef<llvm::StringRef> args);
};

template <> struct ControllerConstructImpl<ControllerType::Newton> {
  static std::unique_ptr<NewtonController> construct(Context *ctx) {
    return std::make_unique<NewtonController>(ctx);
  }
};

} // namespace pimsim

#endif // PIMSIM_PIM_H
