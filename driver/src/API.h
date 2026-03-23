#ifndef PIMSIM_DRIVER_API_H
#define PIMSIM_DRIVER_API_H

#include <cwchar>
#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
#define EXPORT_PIMSIM_API extern "C"
#else
#define EXPORT_PIMSIM_API
#endif

#define UNSAFE

typedef struct pimsim_memory_device_t pimsim_memory_device_t;

typedef struct {
  size_t channels;
  size_t ranks;
  size_t bankgroups;
  size_t banks;
  size_t rows;
  size_t columns;
  const char *memory_type;
} pimsim_config_t;

typedef enum {
  PIMSIM_COMMAND_SUCCESS = 0,
  PIMSIM_COMMAND_INVALID = -1,
  PIMSIM_COMMAND_UNKNOWN = -2,
} pimsim_command_status_t;

inline int pimsim_is_success(pimsim_command_status_t status) {
  return status == PIMSIM_COMMAND_SUCCESS;
}

inline int pimsim_is_failure(pimsim_command_status_t status) {
  return status != PIMSIM_COMMAND_SUCCESS;
}

inline int pimsim_is_invalid(pimsim_command_status_t status) {
  return status == PIMSIM_COMMAND_INVALID;
}

inline int pimsim_is_unknown(pimsim_command_status_t status) {
  return status == PIMSIM_COMMAND_UNKNOWN;
}

EXPORT_PIMSIM_API pimsim_command_status_t pimsim_create_memory_device(
    const char *config_file, pimsim_memory_device_t **deviceOut,
    bool enable_command_log = false);

EXPORT_PIMSIM_API void
pimsim_destroy_memory_device(pimsim_memory_device_t *device);

EXPORT_PIMSIM_API pimsim_command_status_t
pimsim_get_config(pimsim_memory_device_t *device, size_t module_idx,
                  pimsim_config_t *out_config);

typedef struct pimsim_row_group_t {
  size_t channel_addr; // this address include module / row information
  size_t *row_addrs;
  size_t row_cnt;     // this size is always same with config.banks
} pimsim_row_group_t; // address of channel + row

typedef enum {
  PIMSIM_PREFER_ON_ONE_MODULE = 0,
  PIMSIM_PREFER_BALANCED_MODULES = 1,
} pimsim_module_allocate_policy_t;

typedef enum {
  PIMSIM_PREFER_ON_ONE_CHANNEL = 0,
  PIMSIM_PREFER_BALANCED_CHANNELS = 1,
} pimsim_ch_allocate_policy_t;

EXPORT_PIMSIM_API pimsim_command_status_t pimsim_allocate_row_groups(
    pimsim_memory_device_t *device, size_t group_cnt,
    pimsim_row_group_t *groups, pimsim_module_allocate_policy_t md_policy,
    pimsim_ch_allocate_policy_t ch_policy);

EXPORT_PIMSIM_API void pimsim_free_row_groups(pimsim_memory_device_t *device,
                                              size_t group_cnt,
                                              pimsim_row_group_t *groups);

EXPORT_PIMSIM_API pimsim_command_status_t
pimsim_try_extend(pimsim_memory_device_t *device, pimsim_row_group_t *group,
                  size_t rows, pimsim_row_group_t *new_group);

UNSAFE EXPORT_PIMSIM_API pimsim_command_status_t
pimsim_allocate_explicit_row_group(pimsim_memory_device_t *device,
                                   size_t module_idx, size_t channel_idx,
                                   size_t row_idx,
                                   pimsim_row_group_t *out_row_group);

// Byte size 32
#define PIMSIM_DEFAULT_PIM_COMP_COL_SIZE 16

typedef enum {
  PIMSIM_READ = 0,
  PIMSIM_WRITE = 1,
  PIMSIM_HEADER = 2,
  PIMSIM_GWRITE = 3,
  PIMSIM_COMPUTE = 4,
  PIMSIM_READRES = 5,
  PIMSIM_VERIFY = 6,
  PIMSIM_HOOK = 255,
} pimsim_command_type_t;

EXPORT_PIMSIM_API pimsim_command_status_t pimsim_issue_command(
    pimsim_command_type_t command, pimsim_memory_device_t *device, ...);

pimsim_command_status_t pimsim_issue_read(pimsim_memory_device_t *device,
                                          size_t address, void *buffer,
                                          size_t size);

pimsim_command_status_t pimsim_issue_write(pimsim_memory_device_t *device,
                                           size_t address, const void *buffer,
                                           size_t size);

pimsim_command_status_t pimsim_issue_write_f16(pimsim_memory_device_t *device,
                                               size_t address,
                                               const uint16_t *buffer,
                                               size_t size);

pimsim_command_status_t pimsim_issue_compute(pimsim_memory_device_t *device,
                                             const pimsim_row_group_t *rowGroup,
                                             size_t col);

pimsim_command_status_t pimsim_issue_read_result(pimsim_memory_device_t *device,
                                                 size_t ch_address,
                                                 void *result);

pimsim_command_status_t pimsim_issue_gwrite(pimsim_memory_device_t *device,
                                            size_t ch_address, void *src);

#endif // PIMSIM_DRIVER_API_H
