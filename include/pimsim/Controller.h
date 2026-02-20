#ifndef PIMSIM_CONTROLLER_H
#define PIMSIM_CONTROLLER_H

#include "common.h"
#include "pimsim/Context.h"
#include "pimsim/Memory.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <variant>
#include <vector>

namespace pimsim {

class Controller : public CommandExecutor {
public:
  Controller(Context *ctx)
      : CommandExecutor(CommandExecutor::Type::Controller), context(ctx) {
    bufferedAddresses.resize(16);
  }
  virtual ~Controller() = default;

  int command(llvm::ArrayRef<llvm::StringRef> args) override;

  virtual void pushMemory(Memory *mem) { memories.push_back(mem); }
  CommandExecutor *getExecutor(size_t idx) const override {
    if (idx < memories.size()) {
      return memories[idx];
    }
    return nullptr;
  }
  Memory *getMemory(size_t idx) const {
    if (idx < memories.size()) {
      return memories[idx];
    }
    return nullptr;
  }

  std::string getExecutorName() const override { return "Controller"; }

  std::pair<Memory *, dramsim3::Address> decodeAddress(size_t address) const;
  size_t encodeAddress(int memoryIndex, int channel, int rank, int bankgroup,
                       int bank, int row, int column) const;

  void encodeToBuffer(size_t address, size_t bufIdx) {
    bufferIn(address, bufIdx);
  }
  void encodeToBuffer(size_t memIdx, dramsim3::Address dramAddr,
                      size_t bufIdx) {
    bufferIn(memIdx, dramAddr, bufIdx);
  }

  static bool classof(const CommandExecutor *executor) {
    return executor->isController();
  }

  Context *getContext() const { return context; }

protected:
  void bufferIn(size_t address, size_t idx) {
    if (idx < bufferedAddresses.size()) {
      bufferedAddresses[idx] = address;
    } else {
      getContext()->getERR() << "Buffer index out of range: " << idx << "\n";
    }
  }

  void bufferIn(size_t memIdx, dramsim3::Address dramAddr, size_t idx) {
    if (idx < bufferedAddresses.size()) {
      bufferedAddresses[idx] = std::make_pair(memIdx, dramAddr);
    } else {
      getContext()->getERR() << "Buffer index out of range: " << idx << "\n";
    }
  }

  void bufferClear() {
    for (auto &entry : bufferedAddresses) {
      entry = std::monostate{};
    }
  }

  void printHelp() const;
  void parseAndShow(llvm::StringRef arg, llvm::raw_ostream &os) const;
  std::pair<size_t, dramsim3::Address>
  parseBufferAddress(llvm::ArrayRef<llvm::StringRef> args) const;

  std::pair<Memory *, dramsim3::Address> getAddress(llvm::StringRef arg) const;

  std::pair<size_t, dramsim3::Address>
  decodeAddressWithMemoryIndex(size_t address) const;

  std::pair<Memory *, dramsim3::Address> bufferOut(size_t idx) const;
  void bufferShow(llvm::raw_ostream &os, size_t idx) const;

  Context *context;
  std::vector<Memory *> memories;
  std::vector<std::variant<std::monostate, std::pair<size_t, dramsim3::Address>,
                           size_t>>
      bufferedAddresses;

private:
  void printMemories() const;
  int bufferCommand(llvm::ArrayRef<llvm::StringRef> args);
};

} // namespace pimsim

#endif // PIMSIM_CONTROLLER_H
