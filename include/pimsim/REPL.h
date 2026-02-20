#ifndef PIMSIM_REPL_H
#define PIMSIM_REPL_H

#include "pimsim/Context.h"
#include "pimsim/Memory.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include <string>
#include <vector>
namespace pimsim {

class REPL : public CommandExecutor {
public:
  REPL(Context *ctx)
      : CommandExecutor(CommandExecutor::Type::REPL), context(ctx) {
    scopeStack.emplace_back(this);
  }

  int acceptInput(llvm::StringRef input);
  bool run();

  int command(llvm::ArrayRef<llvm::StringRef> args) override;
  CommandExecutor *getExecutor(size_t idx) const override {
    if (idx < executors.size()) {
      return executors[idx].get();
    }
    return nullptr;
  }
  std::string getExecutorName() const override { return "REPL"; }

  static bool classof(const CommandExecutor *executor) {
    return executor->isREPL();
  }

  bool runScript(llvm::StringRef scriptPath);

private:
  std::unique_ptr<dramsim3::Config> loadMemoryConfig(llvm::StringRef arg) const;

  void printHelp() const;
  void printGlobalHelp() const;
  void printHistory() const;
  void printExecutors() const;
  bool printConfiguration(llvm::StringRef arg) const;
  bool createMemory(llvm::ArrayRef<llvm::StringRef> args);
  bool runScope(llvm::ArrayRef<llvm::StringRef> args);
  bool runScopes() const;

  template <MemoryType T, typename... Args>
  void loadAndCreateMemory(llvm::StringRef configPath, Args &&...args) {
    auto config = loadMemoryConfig(configPath);
    if (!config) {
      context->getERR() << "Failed to load memory configuration from file: "
                        << configPath << "\n";
      return;
    }
    auto memory = context->createMemory<T>(std::move(config),
                                           std::forward<Args>(args)...);
    auto executor = llvm::cast<CommandExecutor>(std::move(memory));
    executors.emplace_back(std::move(executor));
  }

  template <ControllerType T, typename... Args>
  void createController(llvm::ArrayRef<llvm::StringRef> mems, Args &&...args) {
    std::vector<Memory *> memExecutors;
    for (const auto &memArg : mems) {
      int memIdx = -1;
      if (memArg.getAsInteger(10, memIdx) || memIdx < 0 ||
          memIdx >= static_cast<int>(executors.size())) {
        context->getERR() << "Invalid memory index: " << memArg << "\n";
        return;
      }
      auto executor = executors[memIdx].get();
      if (!executor->isMemory()) {
        context->getERR() << "Executor at index " << memIdx
                          << " is not a memory executor.\n";
        return;
      }
      memExecutors.push_back(llvm::cast<Memory>(executor));
    }
    auto controller = context->createController<T>(std::forward<Args>(args)...);
    for (Memory *mem : memExecutors) {
      controller->pushMemory(mem);
    }
    auto executor = llvm::cast<CommandExecutor>(std::move(controller));
    executors.emplace_back(std::move(executor));
  }

  struct Scope {
    Scope(CommandExecutor *executor) : executor(executor) {}
    CommandExecutor *executor;
  };

  Context *context;
  std::vector<std::string> history;
  std::vector<std::unique_ptr<CommandExecutor>> executors;
  std::vector<Scope> scopeStack;
};

} // namespace pimsim

#endif // PIMSIM_REPL_H
