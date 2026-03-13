#ifndef PIMSIM_NEUPIMS_H
#define PIMSIM_NEUPIMS_H

#include "pimsim/Context.h"
#include "pimsim/Newton.h"
namespace pimsim {

class Neupims : public Newton {
public:
  using Newton::Newton;
  int command(llvm::ArrayRef<llvm::StringRef> args) override;
  std::string getExecutorName() const override {
    return std::format("{} - Neupims", Newton::getExecutorName());
  }

private:
  void printHelp() const;
  int pimHeader(llvm::ArrayRef<llvm::StringRef> args);
  int pimPrecharge(llvm::ArrayRef<llvm::StringRef> args);
};

class NeupimsChannel
    : public PIMChannel<NeupimsChannel, NewtonChannel, HeaderedChannelTrait> {
public:
  NeupimsChannel(Context *ctx, std::vector<std::unique_ptr<Rank>> &&ranks);

  llvm::SmallVector<f16> readResult() const override;
  void comp(size_t col) override;

  static bool classof(const Channel *channel);
};

class NeupimsDualRBBank : public NewtonBank {
public:
  NeupimsDualRBBank(Context *ctx, size_t numRows, size_t columnSize);

  void activate(size_t row) override;
  virtual void pimPrecharge() {
    assert(pimBuffer.isOpen && "Row buffer must be open for precharge");
    // Precharge logic here
    pimBuffer.isOpen = false;
  }
  void write(size_t column, const Byte *src, size_t size) override;
  void comp(size_t col) override;

  void doActivateHook(size_t row) override;

  static bool classof(const Bank *bank);
  RowBuffer &getPimBuffer() { return pimBuffer; }

private:
  RowBuffer pimBuffer;
};

class NeupimsController : public NewtonController {
public:
  using NewtonController::NewtonController;
  int command(llvm::ArrayRef<llvm::StringRef> args) override;

private:
  void printHelp() const;
  int pimHeader(llvm::ArrayRef<llvm::StringRef> args);
  int comp(llvm::ArrayRef<llvm::StringRef> args) override;
};

template <> struct MemoryConstructImpl<MemoryType::Neupims> {
  static std::unique_ptr<Neupims>
  construct(Context *ctx, std::unique_ptr<dramsim3::Config> config) {
    return MemoryConstructor<Neupims, NeupimsChannel, Rank, NewtonBankGroup,
                             NeupimsDualRBBank>::construct(ctx,
                                                           std::move(config));
  }
};

template <> struct ControllerConstructImpl<ControllerType::Neupims> {
  static std::unique_ptr<NeupimsController> construct(Context *ctx) {
    return std::make_unique<NeupimsController>(ctx);
  }
};

} // namespace pimsim

#endif // PIMSIM_NEUPIMS_H
