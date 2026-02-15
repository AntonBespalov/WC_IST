#include "logging_spsc.h"
#include <stddef.h>
#include <string.h>

/**
 * @brief Рассчитать смещение элемента в буфере.
 * @param queue Указатель на очередь.
 * @param index Индекс элемента, [шаги].
 * @return Смещение в байтах, [байт].
 * @pre queue != NULL.
 */
static uint32_t logging_spsc_offset(const logging_spsc_t *queue, uint32_t index)
{
  return index * queue->item_size;
}

logging_result_t logging_spsc_init(logging_spsc_t *queue,
                                  uint8_t *storage,
                                  uint32_t storage_size_bytes,
                                  uint32_t capacity,
                                  uint32_t item_size)
{
  if ((queue == NULL) || (storage == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  if ((capacity == 0u) || (item_size == 0u))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  const uint32_t required_size = capacity * item_size;
  if ((storage_size_bytes == 0u) || (storage_size_bytes != required_size))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  if ((storage_size_bytes % 4u) != 0u)
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  queue->storage = storage;
  queue->storage_size_bytes = storage_size_bytes;
  queue->capacity = capacity;
  queue->item_size = item_size;
  atomic_store_explicit(&queue->write_idx, 0u, memory_order_relaxed);
  atomic_store_explicit(&queue->read_idx, 0u, memory_order_relaxed);
  atomic_store_explicit(&queue->count, 0u, memory_order_relaxed);
  return LOGGING_RESULT_OK;
}

bool logging_spsc_push(logging_spsc_t *queue, const void *item)
{
  if ((queue == NULL) || (item == NULL))
  {
    return false;
  }
  uint32_t count = atomic_load_explicit(&queue->count, memory_order_acquire);
  if (count >= queue->capacity)
  {
    return false;
  }
  const uint32_t write_idx = atomic_load_explicit(&queue->write_idx, memory_order_relaxed);
  const uint32_t offset = logging_spsc_offset(queue, write_idx);
  (void)memcpy(&queue->storage[offset], item, queue->item_size);
  const uint32_t next_idx = (write_idx + 1u) % queue->capacity;
  atomic_store_explicit(&queue->write_idx, next_idx, memory_order_release);
  (void)atomic_fetch_add_explicit(&queue->count, 1u, memory_order_release);
  return true;
}

bool logging_spsc_pop(logging_spsc_t *queue, void *out)
{
  if ((queue == NULL) || (out == NULL))
  {
    return false;
  }
  uint32_t count = atomic_load_explicit(&queue->count, memory_order_acquire);
  if (count == 0u)
  {
    return false;
  }
  const uint32_t read_idx = atomic_load_explicit(&queue->read_idx, memory_order_relaxed);
  const uint32_t offset = logging_spsc_offset(queue, read_idx);
  (void)memcpy(out, &queue->storage[offset], queue->item_size);
  const uint32_t next_idx = (read_idx + 1u) % queue->capacity;
  atomic_store_explicit(&queue->read_idx, next_idx, memory_order_release);
  (void)atomic_fetch_sub_explicit(&queue->count, 1u, memory_order_release);
  return true;
}

uint32_t logging_spsc_count(const logging_spsc_t *queue)
{
  if (queue == NULL)
  {
    return 0u;
  }
  return atomic_load_explicit(&queue->count, memory_order_relaxed);
}