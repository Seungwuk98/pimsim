#include "API.h"
#include "Allocator.h"
#include "DeviceImpl.h"
#include "pimsim/Context.h"
#include "pimsim/DefaultDRAM.h"
#include "pimsim/Memory.h"
#include "pimsim/Neupims.h"
#include "pimsim/Newton.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include <cstdarg>

namespace pimsim::driver {

pimsim_command_status_t createMemoryDeviceImpl(
    llvm::StringRef memoryType, llvm::ArrayRef<llvm::StringRef> configFiles,
    pimsim_memory_device_t *deviceOut, bool enableCommandLog) {

  deviceOut->context = new Context("/dev/null");
  auto context = deviceOut->context;
  std::unique_ptr<Controller> controller;
  if (memoryType == "newton") {
    controller = context->createController<ControllerType::Newton>();
  } else if (memoryType == "neupims") {
    controller = context->createController<ControllerType::Neupims>();
  } else if (memoryType == "dram") {
    controller = context->createController<ControllerType::DefaultDRAM>();
  } else {
    context->getERR() << "Unsupported memory type: " << memoryType << "\n";
    return PIMSIM_COMMAND_INVALID;
  }

  deviceOut->allocator = new Allocator(deviceOut);

  for (const auto &[idx, configFile] : llvm::enumerate(configFiles)) {
    auto config = context->createConfig(configFile);
    if (!config) {
      context->getERR() << "Failed to load memory configuration from file: "
                        << configFile << "\n";
      return PIMSIM_COMMAND_INVALID;
    }

    std::unique_ptr<Memory> memory;
    if (memoryType == "newton") {
      memory = context->createMemory<MemoryType::Newton>(std::move(config));
    } else if (memoryType == "neupims") {
      memory = context->createMemory<MemoryType::Neupims>(std::move(config));
    } else if (memoryType == "dram") {
      memory = context->createMemory<MemoryType::DRAM>(std::move(config));
    } else {
      context->getERR() << "Unsupported memory type: " << memoryType << "\n";
      return PIMSIM_COMMAND_INVALID;
    }

    controller->pushMemory(memory.get());
    deviceOut->memories.emplace_back(std::move(memory));

    std::unique_ptr<Module> module = std::make_unique<Module>(
        idx, deviceOut->allocator, &controller->getMemory(idx)->getConfig());
    deviceOut->allocator->pushModule(std::move(module));
  }

  if (enableCommandLog) {
    deviceOut->commandLog = std::make_unique<std::vector<std::string>>();
  }

  deviceOut->controller = std::move(controller);
  auto configNames = llvm::join(configFiles, ",");
  deviceOut->deviceName = std::format("{}_{}", memoryType.str(), configNames);
  llvm::errs() << "Created memory device: " << deviceOut->deviceName << "\n";

  return PIMSIM_COMMAND_SUCCESS;
}

struct Logger {
  Logger(pimsim_memory_device_t *device) : device(device) {}
  ~Logger() { flush(); }

  void logCommand(llvm::StringRef commandStr) {
    if (device->commandLog) {
      os << commandStr;
    }
  }

  void logAddress(size_t address) {
    if (!device->commandLog)
      return;
    os << ' ' << llvm::format_hex_no_prefix(address, 16);
  }

  void logSize(size_t size) {
    if (!device->commandLog)
      return;
    os << ' ' << size;
  }

  void logByte(llvm::ArrayRef<pimsim::Byte> data) {
    if (!device->commandLog)
      return;
    os << ' ';
    for (auto byte : llvm::reverse(data)) {
      std::string byteStr = std::format("{:02x}", byte);
      assert(byteStr.size() == 2 && "Byte string should be 2 characters");
      os << byteStr[1]
         << byteStr[0]; // Reverse the byte order for little-endian
    }
  }

  void logIdentifier(llvm::StringRef identifier) {
    if (device->commandLog) {
      os << ' ' << identifier;
    }
  }

  void flush() {
    if (device->commandLog) {
#ifndef NDEBUG
      llvm::errs() << str << '\n';
#endif

      device->commandLog->emplace_back(str);
      str.clear();
    }
  }

private:
  std::string str;
  llvm::raw_string_ostream os{str};
  pimsim_memory_device_t *device;
};

} // namespace pimsim::driver

EXPORT_PIMSIM_API
pimsim_command_status_t
pimsim_create_memory_device(const char *config_file,
                            pimsim_memory_device_t **deviceOut,
                            bool enable_command_log) {
  *deviceOut = new pimsim_memory_device_t;
  llvm::StringRef configFile(config_file);
  configFile = configFile.trim();

  if (!configFile.starts_with("type=")) {
    llvm::errs() << "Configuration string must start with 'type=': "
                 << configFile << "\n";
    return PIMSIM_COMMAND_INVALID;
  }

  auto blank = configFile.find(' ', 5);

  auto typeStr = configFile.slice(5, blank);

  configFile = configFile.substr(blank).trim();

  if (!configFile.starts_with("config=")) {
    llvm::errs() << "Configuration string must contain 'config=': "
                 << configFile << "\n";
    return PIMSIM_COMMAND_INVALID;
  }

  auto configStr = configFile.substr(7);
  llvm::SmallVector<llvm::StringRef> splited;
  configStr.split(splited, ',');

  return pimsim::driver::createMemoryDeviceImpl(typeStr, configStr, *deviceOut,
                                                enable_command_log);
}

EXPORT_PIMSIM_API void
pimsim_destroy_memory_device(pimsim_memory_device_t *device) {
  if (device->commandLog) {
    llvm::outs() << "Command log for device " << device->deviceName << ":\n";
    for (const auto &entry : *device->commandLog) {
      llvm::outs() << entry << "\n";
    }
  }
  delete device->allocator;
  delete device->context;
  delete device;
}

EXPORT_PIMSIM_API pimsim_command_status_t pimsim_allocate_row_groups(
    pimsim_memory_device_t *device, size_t group_cnt,
    pimsim_row_group_t *groups, pimsim_module_allocate_policy_t md_policy,
    pimsim_ch_allocate_policy_t ch_policy) {
  return device->allocator->allocateRowGroups(group_cnt, groups, md_policy,
                                              ch_policy)
             ? PIMSIM_COMMAND_INVALID
             : PIMSIM_COMMAND_SUCCESS;
}

EXPORT_PIMSIM_API void pimsim_free_row_groups(pimsim_memory_device_t *device,
                                              size_t group_cnt,
                                              pimsim_row_group_t *groups) {
  for (size_t idx = 0; idx < group_cnt; ++idx) {
    device->allocator->freeRowGroup(&groups[idx]);
  }
}

EXPORT_PIMSIM_API pimsim_command_status_t
pimsim_get_config(pimsim_memory_device_t *device, size_t module_idx,
                  pimsim_config_t *out_config) {
  if (module_idx >= device->memories.size()) {
    return PIMSIM_COMMAND_INVALID;
  }
  auto &config = device->memories[module_idx]->getConfig();
  out_config->channels = config.channels;
  out_config->ranks = config.ranks;
  out_config->bankgroups = config.bankgroups;
  out_config->banks = config.banks_per_group;
  out_config->rows = config.rows;
  out_config->columns = config.columns;
  out_config->memory_type = device->deviceName.c_str();
  return PIMSIM_COMMAND_SUCCESS;
}

EXPORT_PIMSIM_API pimsim_command_status_t pimsim_issue_command(
    pimsim_command_type_t command, pimsim_memory_device_t *device, ...) {
  llvm::SmallVector<llvm::StringRef, 8> args;

  va_list vargs;
  va_start(vargs, device);

  pimsim::DefaultDRAMController *dramController =
      static_cast<pimsim::DefaultDRAMController *>(device->controller.get());

  switch (command) {
  case PIMSIM_READ: {
    size_t address = va_arg(vargs, size_t);
    void *buffer = va_arg(vargs, void *);
    size_t size = va_arg(vargs, size_t);

    auto [memory, dramAddr] = dramController->decodeAddress(address);
    if (!memory)
      return PIMSIM_COMMAND_INVALID;

    llvm::MutableArrayRef<pimsim::Byte> buf(static_cast<pimsim::Byte *>(buffer),
                                            size);
    int readResult = dramController->read(memory, dramAddr, buf);
    if (readResult != 0)
      return PIMSIM_COMMAND_INVALID;

    break;
  }

  case PIMSIM_WRITE: {
    size_t address = va_arg(vargs, size_t);
    const void *buffer = va_arg(vargs, const void *);
    size_t size = va_arg(vargs, size_t);

    auto [memory, dramAddr] = dramController->decodeAddress(address);
    if (!memory)
      return PIMSIM_COMMAND_INVALID;

    llvm::ArrayRef<pimsim::Byte> buf(static_cast<const pimsim::Byte *>(buffer),
                                     size);
    int writeResult = dramController->write(memory, dramAddr, buf);
    if (writeResult != 0)
      return PIMSIM_COMMAND_INVALID;

    break;
  }
  case PIMSIM_HEADER: {
    llvm_unreachable("Header command is not supported in this implementation");
  }
  case PIMSIM_GWRITE: {
    assert((device->deviceName.starts_with("newton") ||
            device->deviceName.starts_with("neupims")) &&
           "GWRITE command is only supported for Newton and NeuPIMs memory "
           "types");

    pimsim::NewtonController *newtonController =
        static_cast<pimsim::NewtonController *>(dramController);

    size_t address = va_arg(vargs, size_t);
    void *srcAddress = va_arg(vargs, void *);

    auto [memory, dramAddr] = newtonController->decodeAddress(address);
    if (!memory)
      return PIMSIM_COMMAND_INVALID;

    const auto &config = memory->getConfig();
    const auto &colSize = config.columns;

    llvm::ArrayRef<pimsim::Byte> srcData(
        static_cast<const pimsim::Byte *>(srcAddress), colSize);

    if (newtonController->gwrite(memory, dramAddr, srcData))
      return PIMSIM_COMMAND_INVALID;
    break;
  }
  case PIMSIM_COMPUTE: {
    assert((device->deviceName.starts_with("newton") ||
            device->deviceName.starts_with("neupims")) &&
           "COMPUTE command is only supported for Newton memory type");

    pimsim::NewtonController *newtonController =
        static_cast<pimsim::NewtonController *>(dramController);

    pimsim_row_group_t *rowGroup = va_arg(vargs, pimsim_row_group_t *);
    size_t col = va_arg(vargs, size_t);
    size_t chAddr = rowGroup->channel_addr;

    auto [memory, dramAddr] = newtonController->decodeAddress(chAddr);
    if (!memory)
      return PIMSIM_COMMAND_INVALID;

    if (newtonController->comp(memory, dramAddr, col))
      return PIMSIM_COMMAND_INVALID;
    break;
  }
  case PIMSIM_READRES: {
    assert((device->deviceName.starts_with("newton") ||
            device->deviceName.starts_with("neupims")) &&
           "READRES command is only supported for Newton "
           "memory type");

    pimsim::NewtonController *newtonController =
        static_cast<pimsim::NewtonController *>(dramController);

    size_t chAddr = va_arg(vargs, size_t);
    void *dstAddr = va_arg(vargs, void *);

    auto [memory, dramAddr] = newtonController->decodeAddress(chAddr);
    if (!memory)
      return PIMSIM_COMMAND_INVALID;

    const auto &config = memory->getConfig();
    llvm::MutableArrayRef<pimsim::Byte> dstDramAddr(
        static_cast<pimsim::Byte *>(dstAddr),
        config.banks * sizeof(pimsim::f16));

    if (newtonController->readRes(memory, dramAddr, dstDramAddr))
      return PIMSIM_COMMAND_INVALID;
    break;
  }
  case PIMSIM_VERIFY: {
    llvm_unreachable("VERIFY command is not supported in this implementation");
  }
  case PIMSIM_HOOK:
    break;
  }

  va_end(vargs);
  return PIMSIM_COMMAND_SUCCESS;
}

pimsim_command_status_t pimsim_issue_read(pimsim_memory_device_t *device,
                                          size_t address, void *buffer,
                                          size_t size) {
  pimsim::driver::Logger logger(device);
  logger.logCommand("read");
  logger.logAddress(address);
  logger.logIdentifier("<out>");
  logger.logSize(size);
  return pimsim_issue_command(PIMSIM_READ, device, address, buffer, size);
}

pimsim_command_status_t pimsim_issue_write(pimsim_memory_device_t *device,
                                           size_t address, const void *buffer,
                                           size_t size) {
  pimsim::driver::Logger logger(device);
  logger.logCommand("write");
  logger.logAddress(address);
  logger.logByte({static_cast<const pimsim::Byte *>(buffer), size});
  return pimsim_issue_command(PIMSIM_WRITE, device, address, buffer, size);
}

pimsim_command_status_t pimsim_issue_write_f16(pimsim_memory_device_t *device,
                                               size_t address,
                                               const uint16_t *buffer,
                                               size_t size) {
  pimsim::driver::Logger logger(device);
  logger.logCommand("write_f16");
  logger.logAddress(address);
  logger.logByte({reinterpret_cast<const pimsim::Byte *>(buffer), size});
  return pimsim_issue_command(PIMSIM_WRITE, device, address, buffer, size);
}

pimsim_command_status_t pimsim_issue_compute(pimsim_memory_device_t *device,
                                             const pimsim_row_group_t *rowGroup,
                                             size_t col) {
  pimsim::driver::Logger logger(device);
  logger.logCommand("compute");
  logger.logAddress(rowGroup->channel_addr);
  logger.logSize(col);
  return pimsim_issue_command(PIMSIM_COMPUTE, device, rowGroup, col);
}

pimsim_command_status_t pimsim_issue_read_result(pimsim_memory_device_t *device,
                                                 size_t ch_address,
                                                 void *result) {
  pimsim::driver::Logger logger(device);
  logger.logCommand("read_result");
  logger.logAddress(ch_address);
  logger.logIdentifier("<out>");
  return pimsim_issue_command(PIMSIM_READRES, device, ch_address, result);
}

pimsim_command_status_t pimsim_issue_gwrite(pimsim_memory_device_t *device,
                                            size_t ch_address, void *src) {
  pimsim::driver::Logger logger(device);
  logger.logCommand("gwrite");
  logger.logAddress(ch_address);

  auto [memory, dramAddr] = device->controller->decodeAddress(ch_address);
  if (!memory)
    return PIMSIM_COMMAND_INVALID;

  auto col = memory->getConfig().columns;
  logger.logByte(
      {static_cast<const pimsim::Byte *>(src), static_cast<size_t>(col)});

  return pimsim_issue_command(PIMSIM_GWRITE, device, ch_address, src);
}
