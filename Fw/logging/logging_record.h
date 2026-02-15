#ifndef LOGGING_RECORD_H
#define LOGGING_RECORD_H

#include <stdint.h>
#include "logging_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file logging_record.h
 * @brief Упаковка/распаковка заголовка Record (little-endian).
 */

/**
 * @brief Размер заголовка Record, [байт].
 */
#define LOGGING_RECORD_HEADER_SIZE (16u)

/**
 * @brief Инициализировать заголовок Record.
 * @param hdr Указатель на заголовок.
 * @param type Тип записи.
 * @param source_id Идентификатор источника.
 * @param payload_len Длина payload, [байт].
 * @param timestamp Таймстемп записи.
 * @param seq Номер последовательности, [шаги].
 * @param flags Флаги записи, [битовая маска].
 * @return None.
 * @pre hdr != NULL.
 * @note Поле magic заполняется значением LOGGING_RECORD_MAGIC.
 */
void logging_record_header_init(logging_record_header_t *hdr,
                                uint8_t type,
                                uint16_t source_id,
                                uint16_t payload_len,
                                const logging_timestamp_t *timestamp,
                                uint16_t seq,
                                uint8_t flags);

/**
 * @brief Упаковать заголовок Record в буфер (LE).
 * @param dst Буфер назначения.
 * @param dst_len Длина буфера, [байт].
 * @param hdr Указатель на заголовок.
 * @return LOGGING_RESULT_OK при успехе.
 * @pre dst != NULL, hdr != NULL.
 */
logging_result_t logging_record_pack_header(uint8_t *dst,
                                            uint32_t dst_len,
                                            const logging_record_header_t *hdr);

/**
 * @brief Распаковать заголовок Record из буфера (LE).
 * @param hdr Указатель на заголовок.
 * @param src Буфер источника.
 * @param src_len Длина буфера, [байт].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre hdr != NULL, src != NULL.
 */
logging_result_t logging_record_unpack_header(logging_record_header_t *hdr,
                                              const uint8_t *src,
                                              uint32_t src_len);

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_RECORD_H */
