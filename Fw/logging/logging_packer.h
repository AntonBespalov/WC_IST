#ifndef LOGGING_PACKER_H
#define LOGGING_PACKER_H

#include <stdbool.h>
#include <stdint.h>
#include "logging_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file logging_packer.h
 * @brief Упаковка snapshot по профилям/каналам.
 */

/**
 * @brief Описание одного поля для упаковки.
 */
typedef struct {
  uint16_t field_id;   /**< Идентификатор поля, [id]. */
  uint16_t offset_bytes; /**< Смещение поля в snapshot, [байт]. */
  uint16_t size_bytes; /**< Размер поля, [байт]. */
  uint16_t reserved;   /**< Резерв, [0]. */
} logging_field_desc_t;

/**
 * @brief Упаковать snapshot по списку полей (little-endian).
 * @param snapshot Указатель на snapshot-структуру.
 * @param fields Массив описаний полей.
 * @param field_count Количество полей, [шт].
 * @param out Буфер назначения.
 * @param out_len Длина буфера назначения, [байт].
 * @param written Фактическая длина, [байт].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre snapshot != NULL, fields != NULL, out != NULL, written != NULL.
 */
logging_result_t logging_packer_pack(const void *snapshot,
                                    const logging_field_desc_t *fields,
                                    uint32_t field_count,
                                    uint8_t *out,
                                    uint32_t out_len,
                                    uint32_t *written);

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_PACKER_H */