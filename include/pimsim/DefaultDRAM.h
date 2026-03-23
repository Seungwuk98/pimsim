#ifndef PIMSIM_DEFAULT_DRAM_H
#define PIMSIM_DEFAULT_DRAM_H

#include "common.h"
#include "configuration.h"
#include "pimsim/Controller.h"
#include "pimsim/Memory.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Casting.h"
#include <format>

namespace pimsim {

class DefaultDRAM : public Memory {
public:
  DefaultDRAM(Context *ctx, std::unique_ptr<dramsim3::Config> cfg,
              std::vector<std::unique_ptr<Channel>> &&chs)
      : Memory(ctx, std::move(cfg), std::move(chs)) {}

  int command(llvm::ArrayRef<llvm::StringRef> args) override;

  int printHelp() const;

  int showRow(llvm::ArrayRef<llvm::StringRef> args);

  int showRowBuffer(llvm::ArrayRef<llvm::StringRef> args);

  int write(llvm::ArrayRef<llvm::StringRef> args);

  int read(llvm::ArrayRef<llvm::StringRef> args);

  int activate(llvm::ArrayRef<llvm::StringRef> args);

  int precharge(llvm::ArrayRef<llvm::StringRef> args);
};

class DefaultDRAMController : public Controller {
public:
  DefaultDRAMController(Context *ctx) : Controller(ctx) {}

  int command(llvm::ArrayRef<llvm::StringRef> args) override;
  std::string getExecutorName() const override {
    return std::format("{} - DefaultDRAMController",
                       Controller::getExecutorName());
  }

  virtual int read(Memory *memory, dramsim3::Address address,
                   llvm::MutableArrayRef<Byte> out);
  virtual int write(Memory *memory, dramsim3::Address address,
                    llvm::ArrayRef<Byte> data);

protected:
  void printHelp() const;

private:
  void printMemories() const;

  int read(llvm::ArrayRef<llvm::StringRef> args);
  int write(llvm::ArrayRef<llvm::StringRef> args);
  int verify(llvm::ArrayRef<llvm::StringRef> args);
  int encode(llvm::ArrayRef<llvm::StringRef> args) const;
  int decode(llvm::ArrayRef<llvm::StringRef> args) const;
};

template <> struct MemoryConstructImpl<MemoryType::DRAM> {
  static std::unique_ptr<DefaultDRAM>
  construct(Context *ctx, std::unique_ptr<dramsim3::Config> config) {
    return MemoryConstructor<DefaultDRAM, Channel, Rank, BankGroup,
                             Bank>::construct(ctx, std::move(config));
  }
};

template <> struct ControllerConstructImpl<ControllerType::DefaultDRAM> {
  static std::unique_ptr<DefaultDRAMController> construct(Context *ctx) {
    return std::make_unique<DefaultDRAMController>(ctx);
  }
};

} // namespace pimsim

#endif // PIMSIM_DEFAULT_DRAM_H
