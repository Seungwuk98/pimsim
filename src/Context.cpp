#include "pimsim/Context.h"
#include "pimsim/Configs.h"
#include "pimsim/Memory.h"
#include <filesystem>

namespace pimsim {

std::unique_ptr<dramsim3::Config>
Context::createConfig(llvm::StringRef configFile) {
  std::filesystem::path configPath(configFile.str());
  if (configPath.is_relative()) {
    std::filesystem::path configDir = DRAMSIM3_CONFIG_DIR;
    if (!configPath.has_extension())
      configPath.replace_extension(".ini");
    configPath = configDir / configPath;
  }

  if (!std::filesystem::exists(configPath)) {
    getERR() << "Configuration file does not exist: " << configPath << "\n";
    return nullptr;
  }
  return std::make_unique<dramsim3::Config>(configPath.string(),
                                            getLogDirectory().str());
}

} // namespace pimsim
