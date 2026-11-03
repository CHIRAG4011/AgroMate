#pragma once
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Fast, crash-free YUV422 (Y0, Cb, Y1, Cr) to JPEG encoder for ESP32.
 * 
 * Guarantees that 100% of writable working buffers (MCU lines, scanline,
 * stream output) are allocated exclusively in internal DRAM (MALLOC_CAP_INTERNAL).
 * This completely avoids the ESP32 hardware errata where 8-bit stores to external
 * PSRAM trigger Guru Meditation StoreProhibited panics.
 * 
 * @param yuv422    Source buffer in YUV422 format (Y0, Cb, Y1, Cr)
 * @param width     Image width in pixels (e.g. 640 for VGA)
 * @param height    Image height in pixels (e.g. 480 for VGA)
 * @param quality   JPEG quality 1..100 (recommended 20..30 for crisp VGA)
 * @param outJpg    Pointer to receive allocated JPEG buffer (caller must free())
 * @param outLen    Pointer to receive JPEG buffer length in bytes
 * @return true on success, false on error
 */
bool encodeYUV422ToJpeg(const uint8_t* yuv422, int width, int height, int quality, uint8_t** outJpg, size_t* outLen);

/**
 * @brief Fast, crash-free RGB565 to JPEG encoder for ESP32.
 */
bool encodeRGB565ToJpeg(const uint8_t* rgb565, int width, int height, int quality, uint8_t** outJpg, size_t* outLen);
