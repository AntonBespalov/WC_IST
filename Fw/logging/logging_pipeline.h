#ifndef LOGGING_PIPELINE_H
#define LOGGING_PIPELINE_H

#include <stdbool.h>
#include <stdint.h>
#include "logging_packer.h"
#include "logging_spsc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file logging_pipeline.h
 * @brief Разделение fast/slow API: publish vs pack.
 */

/**
 * @brief Быстрая публикация snapshot в SPSC (fast домен).
 * @param queue Указатель на очередь.
 * @param snapshot Указатель на snapshot-данные.
 * @return true, если snapshot опубликован.
 * @pre queue != NULL, snapshot != NULL.
 */
static inline bool logging_fast_publish(logging_spsc_t *queue, const void *snapshot)
{
  return logging_spsc_push(queue, snapshot);
}

/**
 * @brief Медленное извлечение snapshot из SPSC (slow домен).
 * @param queue Указатель на очередь.
 * @param snapshot Буфер для snapshot.
 * @return true, если snapshot получен.
 * @pre queue != NULL, snapshot != NULL.
 */
static inline bool logging_slow_consume(logging_spsc_t *queue, void *snapshot)
{
  return logging_spsc_pop(queue, snapshot);
}

/**
 * @brief Упаковать snapshot по профилю (slow домен).
 * @param snapshot Указатель на snapshot-структуру.
 * @param fields Массив описаний полей.
 * @param field_count Количество полей, [шт].
 * @param out Буфер назначения.
 * @param out_len Длина буфера назначения, [байт].
 * @param written Фактическая длина, [байт].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre snapshot != NULL, fields != NULL, out != NULL, written != NULL.
 */
static inline logging_result_t logging_slow_pack(const void *snapshot,
                                                const logging_field_desc_t *fields,
                                                uint32_t field_count,
                                                uint8_t *out,
                                                uint32_t out_len,
                                                uint32_t *written)
{
  return logging_packer_pack(snapshot, fields, field_count, out, out_len, written);
}

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_PIPELINE_H */