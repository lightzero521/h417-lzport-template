#include "lzport/flash.h"

#include "ch32h417_flash.h"

static int range_valid(uint32_t offset, uint32_t len)
{
    return (offset <= LZPORT_FLASH_STORAGE_SIZE) &&
           (len <= (LZPORT_FLASH_STORAGE_SIZE - offset));
}

lzport_status lzport_flash_read(uint32_t offset, void *data, uint32_t len)
{
    uint8_t *out = (uint8_t *)data;
    const uint8_t *src;
    uint32_t i;

    if (((data == 0) && (len != 0U)) || !range_valid(offset, len)) {
        return LZPORT_EINVAL;
    }
    src = (const uint8_t *)(LZPORT_FLASH_STORAGE_BASE + offset);
    for (i = 0U; i < len; ++i) {
        out[i] = src[i];
    }
    return LZPORT_OK;
}

lzport_status lzport_flash_erase(uint32_t offset, uint32_t len)
{
    uint32_t address;
    uint32_t end;
    FLASH_Status status = FLASH_COMPLETE;

    if ((len == 0U) || !range_valid(offset, len) ||
        ((offset % LZPORT_FLASH_PAGE_SIZE) != 0U) ||
        ((len % LZPORT_FLASH_PAGE_SIZE) != 0U) ||
        (FLASH_GetCapacity() != FLASHCapacity_960K)) {
        return LZPORT_EINVAL;
    }
    address = LZPORT_FLASH_STORAGE_BASE + offset;
    end = address + len;
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_WRPRTERR);
    while ((address < end) && (status == FLASH_COMPLETE)) {
        status = FLASH_ErasePage(address);
        address += LZPORT_FLASH_PAGE_SIZE;
    }
    FLASH_Lock();
    return (status == FLASH_COMPLETE) ? LZPORT_OK : LZPORT_EIO;
}

lzport_status lzport_flash_write(uint32_t offset, const void *data,
                                 uint32_t len)
{
    const uint8_t *src = (const uint8_t *)data;
    uint32_t address;
    uint32_t i;
    FLASH_Status status = FLASH_COMPLETE;

    if (((data == 0) && (len != 0U)) || !range_valid(offset, len) ||
        ((offset & 1U) != 0U) || ((len & 1U) != 0U) ||
        (FLASH_GetCapacity() != FLASHCapacity_960K)) {
        return LZPORT_EINVAL;
    }
    address = LZPORT_FLASH_STORAGE_BASE + offset;
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_WRPRTERR);
    for (i = 0U; (i < len) && (status == FLASH_COMPLETE); i += 2U) {
        uint16_t value = (uint16_t)src[i] | ((uint16_t)src[i + 1U] << 8);
        status = FLASH_ProgramHalfWord(address + i, value);
    }
    FLASH_Lock();
    if (status != FLASH_COMPLETE) {
        return LZPORT_EIO;
    }
    for (i = 0U; i < len; ++i) {
        if (*(const uint8_t *)(address + i) != src[i]) {
            return LZPORT_EIO;
        }
    }
    return LZPORT_OK;
}
