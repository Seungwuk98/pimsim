#ifndef PIMSIM_DRIVER_DEVICE_IMPL_H
#define PIMSIM_DRIVER_DEVICE_IMPL_H

#include "API.h"
#include "Allocator.h"
#include "pimsim/Context.h"
#include "pimsim/Controller.h"

struct pimsim_memory_device_t {
  pimsim::Context *context;
  std::unique_ptr<pimsim::Controller> controller;
  std::vector<std::unique_ptr<pimsim::Memory>> memories;
  pimsim::driver::Allocator *allocator;
  std::string deviceName;

  std::unique_ptr<std::vector<std::string>> commandLog;
  pimsim_memory_device_t() = default;
};

#endif // PIMSIM_DRIVER_DEVICE_IMPL_H
