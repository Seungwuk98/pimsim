#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include <filesystem>
#include <string>

#include "configuration.h"
#include "pimsim/Configs.h"
#include "pimsim/Context.h"
#include "pimsim/DefaultDRAM.h"
#include "pimsim/Memory.h"
#include "pimsim/REPL.h"

namespace llvm {

cl::opt<std::string> logDir("log-dir",
                            cl::desc("Directory to store logs and outputs"),
                            cl::value_desc("directory"),
                            cl::init(PIMSIM_LOG_DIR));
cl::opt<std::string>
    scriptFile("script-file", cl::desc("Path to a script file to run in REPL"),
               cl::value_desc("file"));

cl::alias scriptFileAlias("s", cl::desc("Alias for --script-file"),
                          cl::aliasopt(scriptFile));

cl::opt<bool> replMode("repl", cl::desc("Start in REPL mode (default: true)"),
                       cl::init(true));

cl::alias replModeAlias("r", cl::desc("Alias for --repl"),
                        cl::aliasopt(replMode));

} // namespace llvm

namespace pimsim {

namespace fs = std::filesystem;

int pimsimMain(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv, "PIM Memory Simulator\n");

  fs::path logDirectory(llvm::logDir.getValue());
  fs::create_directories(logDirectory);
  Context ctx(PIMSIM_LOG_DIR);

  REPL repl(&ctx);

  if (llvm::scriptFile.getNumOccurrences() > 0) {
    repl.runScript(llvm::scriptFile.getValue());
    if (llvm::replMode.getNumOccurrences() == 0) {
      llvm::replMode = false;
    }
  } else if (!llvm::replMode) {
    llvm::errs()
        << "No script file provided and REPL mode is disabled. Exiting.\n";
    return -1;
  }

  if (llvm::replMode && repl.run()) {
    llvm::errs() << "Error running REPL\n";
    return -1;
  }
  return 0;
}

} // namespace pimsim

int main(int argc, char **argv) {
  llvm::InitLLVM X(argc, argv);
  return pimsim::pimsimMain(argc, argv);
}
