#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "lzport/status.h"

/* Reserved after the current V3F/V5F linker regions. */
#define LZPORT_FLASH_STORAGE_BASE 0x080A0000U
#define LZPORT_FLASH_PAGE_SIZE    8192U
#define LZPORT_FLASH_STORAGE_SIZE (320U * 1024U)

/* The caller must serialize erase/write access between the two cores. */

lzport_status lzport_flash_read(uint32_t offset, void *data, uint32_t len);
/* offset and len must be page aligned. */
lzport_status lzport_flash_erase(uint32_t offset, uint32_t len);
/* offset and len must be half-word aligned; destination must already be erased. */
lzport_status lzport_flash_write(uint32_t offset, const void *data,
                                 uint32_t len);

#ifdef __cplusplus
}
#endif
