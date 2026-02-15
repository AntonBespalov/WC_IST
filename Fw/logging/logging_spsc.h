#ifndef LOGGING_SPSC_H
#define LOGGING_SPSC_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include "logging_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file logging_spsc.h
 * @brief SPSC очередь для передачи snapshot из fast->slow домена.
 */

/**
 * @brief Контекст SPSC очереди фиксированных элементов.
 */
typedef struct {
  uint8_t *storage;               /**< Буфер хранения, [байт]. */
  uint32_t storage_size_bytes;    /**< Размер буфера хранения, [байт]. */
  uint32_t capacity;              /**< Ёмкость в элементах, [шт]. */
  uint32_t item_size;             /**< Размер элемента, [байт]. */
  atomic_uint_fast32_t write_idx; /**< Индекс записи, [шаги]. */
  atomic_uint_fast32_t read_idx;  /**< Индекс чтения, [шаги]. */
  atomic_uint_fast32_t count;     /**< Текущее число элементов, [шт]. */
} logging_spsc_t;

/**
 * @brief Инициализировать SPSC очередь.
 * @param queue Указатель на очередь.
 * @param storage Буфер хранения.
 * @param storage_size_bytes Размер буфера, [байт].
 * @param capacity Ёмкость в элементах.
 * @param item_size Размер элемента, [байт].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre queue != NULL, storage != NULL.
 * @note storage_size_bytes должен равняться capacity * item_size и быть кратным 4 байтам.
 */
logging_result_t logging_spsc_init(logging_spsc_t *queue,
                                  uint8_t *storage,
                                  uint32_t storage_size_bytes,
                                  uint32_t capacity,
                                  uint32_t item_size);

/**
 * @brief Поместить элемент в очередь (fast домен).
 * @param queue Указатель на очередь.
 * @param item Указатель на данные элемента.
 * @return true, если элемент добавлен.
 * @pre queue != NULL, item != NULL.
 */
bool logging_spsc_push(logging_spsc_t *queue, const void *item);

/**
 * @brief Извлечь элемент из очереди (slow домен).
 * @param queue Указатель на очередь.
 * @param out Указатель на буфер для элемента.
 * @return true, если элемент извлечён.
 * @pre queue != NULL, out != NULL.
 */
bool logging_spsc_pop(logging_spsc_t *queue, void *out);

/**
 * @brief Получить оценку числа элементов в очереди.
 * @param queue Указатель на очередь.
 * @return Количество элементов, [шт].
 * @pre queue != NULL.
 */
uint32_t logging_spsc_count(const logging_spsc_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_SPSC_H */
