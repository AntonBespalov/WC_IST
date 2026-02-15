#ifndef LOGGING_CRC32_H
#define LOGGING_CRC32_H

#include <stdint.h>
#include "logging_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file logging_crc32.h
 * @brief CRC32 для payload логов (slow домен).
 */

/**
 * @brief Вычислить CRC32 (IEEE 802.3, отражённый), [безразм.].
 * @param data Указатель на данные.
 * @param len Длина данных, [байт].
 * @return CRC32, [безразм.].
 * @pre data != NULL.
 */
uint32_t logging_crc32_compute(const uint8_t *data, uint32_t len);

/**
 * @brief Добавить CRC32 в конец payload (LE).
 * @param payload Буфер payload.
 * @param payload_len Текущая длина payload, [байт].
 * @param payload_capacity Ёмкость буфера, [байт].
 * @param out_len Итоговая длина payload, [байт].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre payload != NULL, out_len != NULL.
 */
logging_result_t logging_crc32_append(uint8_t *payload,
                                      uint16_t payload_len,
                                      uint16_t payload_capacity,
                                      uint16_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_CRC32_H */