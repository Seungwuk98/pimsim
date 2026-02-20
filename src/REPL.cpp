#include "pimsim/REPL.h"
#include "pimsim/Configs.h"
#include "pimsim/Context.h"
#include "pimsim/DefaultDRAM.h"
#include "pimsim/Neupims.h"
#include "pimsim/Newton.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/MemoryBuffer.h"

namespace pimsim {

bool REPL::run() {
  context->getOS()
      << "Welcome to PIMSim REPL! Type 'help' for a list of commands.\n";
  std::string input;
  while (true) {
    context->getOS() << "> ";
    if (!std::getline(std::cin, input)) {
      context->getOS() << "Exiting REPL.\n";
      break;
    }
    if (input.empty())
      continue;
    llvm::StringRef trimmedInput = llvm::StringRef(input).trim();
    if (trimmedInput == "exit" || trimmedInput == "quit") {
      context->getOS() << "Exiting REPL.\n";
      break;
    }
    if (auto outCode = acceptInput(trimmedInput)) {
      context->getERR() << "Error executing command: " << input << "\n";
      context->getERR() << "Return code: " << outCode << "\n";
    }
  }
  return false;
}

void REPL::printHelp() const {
  context->getOS() << "Available commands:\n";
  context->getOS() << "  executors - List all created command executors\n";
  context->getOS()
      << "  create <executor type> <specific type> <memory_config_file>? "
         "- Create a new "
         "memory or controller of the "
         "specified type. For memory, the configuration file is required.\n";
  context->getOS() << "  config <executor index> - Show configuration of the "
                      "specified executor\n";
}

void REPL::printGlobalHelp() const {
  context->getOS() << "Global commands:\n";
  context->getOS() << "  exit, quit - Exit the REPL\n";
  context->getOS()
      << "  help - Show global help message and current scope help message\n";
  context->getOS() << "  history - Show command history\n";
  context->getOS() << "  scope <index> - Enter the scope of the specified "
                      "command executor\n";
  context->getOS() << "        exit - Exit the current scope\n";
  context->getOS() << "        Show current scope stack\n";
  context->getOS() << "  scopes - Show all scopes\n";
  context->getOS() << "  run <script_path> - Run commands from a script file "
                      "in current scope\n";
}

int REPL::command(llvm::ArrayRef<llvm::StringRef> args) {
  if (args[0] == "history") {
    if (args.size() != 1) {
      context->getOS() << "Warning: 'history' command does not take any "
                          "arguments. Ignoring extra "
                          "arguments.\n";
    }
    printHistory();
    return false;
  } else if (args[0] == "executors") {
    if (args.size() != 1) {
      context->getOS() << "Warning: 'memories' command does not take any "
                          "arguments. Ignoring extra "
                          "arguments.\n";
    }
    printExecutors();
    return false;
  } else if (args[0] == "create") {
    return createMemory(args.slice(1));
  } else if (args[0] == "help") {
    printHelp();
    return false;
  } else if (args[0] == "config") {
    if (args.size() != 2) {
      context->getERR() << "Usage: config <executor index>\n";
      return true;
    }
    return printConfiguration(args[1]);
  }
  return true;
}

void REPL::printHistory() const {
  context->getOS() << "Command History:\n";
  for (size_t i = 0; i < history.size(); ++i)
    context->getOS() << "  " << i << ": " << history[i] << "\n";
}

void REPL::printExecutors() const {
  if (executors.empty()) {
    context->getOS() << "No executors created yet.\n";
    return;
  }
  context->getOS() << "Executors:\n";
  for (size_t i = 0; i < executors.size(); ++i)
    context->getOS() << "  " << i << ": " << executors[i]->getExecutorName()
                     << "\n";
}

bool REPL::runScript(llvm::StringRef scriptPath) {
  // TODO: Implement script execution logic
  auto memBuffer = llvm::MemoryBuffer::getFile(scriptPath);
  if (std::error_code ec = memBuffer.getError()) {
    context->getERR() << "Failed to read script file: " << ec.message() << "\n";
    return true;
  }
  auto buffer = memBuffer->get()->getBuffer();
  llvm::SmallVector<llvm::StringRef> lines;
  buffer.split(lines, '\n', -1, false);

  for (const auto &[idx, line] : llvm::enumerate(lines)) {
    llvm::StringRef trimmedLine = line.trim();
    auto [exeLine, comment] = trimmedLine.split('#');
    if (exeLine.empty())
      continue;
    context->getOS() << "Executing line " << idx << ": " << trimmedLine << "\n";
    if (auto err = acceptInput(exeLine)) {
      context->getERR() << "Error executing line " << idx << ": " << trimmedLine
                        << "\n";
      context->getERR() << "Return code: " << err << "\n";
      return true;
    }
  }
  return false;
}

bool REPL::runScope(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.empty()) {
    // show current scope
    context->getOS() << "Current Scope: "
                     << scopeStack.back().executor->getExecutorName();
    return false;
  }

  llvm::StringRef scopeArg = args[0];
  if (scopeArg == "exit") {
    if (scopeStack.size() <= 1) {
      context->getERR() << "Cannot exit the global scope.\n";
      return true;
    }
    scopeStack.pop_back();
    context->getOS() << "Exited to scope: "
                     << scopeStack.back().executor->getExecutorName() << "\n";
    return false;
  }

  int scopeIdx = -1;
  if (scopeArg.getAsInteger(10, scopeIdx) || scopeIdx < 0 ||
      scopeIdx >= static_cast<int>(executors.size())) {
    context->getERR() << "Invalid scope index: " << scopeArg << "\n";
    return true;
  }

  auto executor = scopeStack.back().executor->getExecutor(scopeIdx);
  scopeStack.emplace_back(executor);
  context->getOS() << "Entered scope: "
                   << scopeStack.back().executor->getExecutorName() << "\n";
  return false;
}

bool REPL::runScopes() const {
  context->getOS() << "Scopes:\n";
  for (size_t i = 0; i < executors.size(); ++i) {
    if (i == 0) {
      context->getOS() << "(global) ";
    } else if (i == executors.size() - 1) {
      context->getOS() << "(current) ";
    }
    context->getOS() << "  " << i << ": " << executors[i]->getExecutorName()
                     << "\n";
  }
  return false;
}

bool REPL::printConfiguration(llvm::StringRef arg) const {
  int memIdx = -1;
  if (arg.getAsInteger(10, memIdx) || memIdx < 0 ||
      memIdx >= static_cast<int>(executors.size())) {
    context->getERR() << "Invalid executor index: " << arg << "\n";
    return true;
  }

  auto executor = llvm::cast<CommandExecutor>(executors[memIdx].get());
  if (auto mem = llvm::dyn_cast<Memory>(executor)) {
    mem->printMemoryConfiguration();
  } else if (auto ctrl = llvm::dyn_cast<Controller>(executor)) {
    context->getERR()
        << "Controller configuration printing not implemented yet.\n";
    return true;
  } else {
    context->getERR() << "Executor at index " << memIdx
                      << " is not a memory or controller executor.\n";
    return true;
  }
  return false;
}

bool REPL::createMemory(llvm::ArrayRef<llvm::StringRef> args) {
  if (args.empty()) {
    context->getERR() << "Usage: create <executor type> <specific type> "
                         "<memory_config_file>? ...\n";
    return true;
  }

  llvm::StringRef typeStr = args[0];
  std::string lowerTypeStr = typeStr.lower();
  if (lowerTypeStr == "memory") {
    typeStr = args[1];

    if (typeStr == "DRAM") {
      if (args.size() > 3) {
        context->getERR()
            << "DRAM memory type does not take additional parameters.\n";
        return true;
      }
      loadAndCreateMemory<MemoryType::DRAM>(args[2]);
    } else if (typeStr == "Newton") {
      auto fastMode = false;
      if (args.size() > 3) {
        if (args[3] == "fast") {
          fastMode = true;
        } else {
          context->getERR()
              << "Unknown parameter for Newton memory: " << args[2] << "\n";
          return true;
        }
      }
      loadAndCreateMemory<MemoryType::Newton>(args[2], fastMode);
    } else if (typeStr == "Neupims") {
      if (args.size() > 3) {
        context->getERR()
            << "Neupims memory type does not take additional parameters.\n";
        return true;
      }
      loadAndCreateMemory<MemoryType::Neupims>(args[2]);
    } else {
      context->getERR() << "Unknown memory type: " << typeStr << "\n";
      return true;
    }
  } else if (lowerTypeStr == "controller") {
    typeStr = args[1];
    if (typeStr == "DRAM") {
      auto memories = args.drop_front(2);
      createController<ControllerType::DefaultDRAM>(memories);
    } else if (typeStr == "Newton") {
      auto memories = args.drop_front(2);
      createController<ControllerType::Newton>(memories);
    } else if (typeStr == "Neupims") {
      auto memories = args.drop_front(2);
      createController<ControllerType::Neupims>(memories);
    } else {
      context->getERR() << "Unknown controller type: " << typeStr << "\n";
      return true;
    }
  } else {
    context->getERR() << "Unknown executor type: " << typeStr << "\n";
    return true;
  }
  return false;
}

int REPL::acceptInput(llvm::StringRef input) {
  history.push_back(input.str());
  llvm::SmallVector<llvm::StringRef, 8> splited;
  input.split(splited, ' ', -1, false);
  llvm::ArrayRef<llvm::StringRef> args(splited);
  if (splited.empty()) {
    context->getERR()
        << "No command entered. Type 'help' for a list of commands.\n";
    return -1;
  }

  // Global commands
  if (splited[0] == "help") {
    printGlobalHelp();
    context->getOS() << "\nCurrent Scope" << ": "
                     << scopeStack.back().executor->getExecutorName() << "\n";
  } else if (splited[0] == "history") {
    printHistory();
    return 0;
  } else if (splited[0] == "run") {
    if (splited.size() < 2) {
      context->getERR() << "Usage: run <script_path>\n";
      return -1;
    }
    return runScript(splited[1]);
  } else if (splited[0] == "scope") {
    return runScope(args.slice(1));
  }
  return scopeStack.back().executor->command(args);
}

std::unique_ptr<dramsim3::Config>
REPL::loadMemoryConfig(llvm::StringRef arg) const {
  std::filesystem::path configPath(arg.str());
  if (configPath.is_relative()) {
    std::filesystem::path configDir = DRAMSIM3_CONFIG_DIR;
    if (!configPath.has_extension())
      configPath.replace_extension(".ini");
    configPath = configDir / configPath;
  }

  if (!std::filesystem::exists(configPath)) {
    context->getERR() << "Configuration file does not exist: " << configPath
                      << "\n";
    return nullptr;
  }
  return std::make_unique<dramsim3::Config>(configPath.string(),
                                            context->getLogDirectory().str());
}

} // namespace pimsim
