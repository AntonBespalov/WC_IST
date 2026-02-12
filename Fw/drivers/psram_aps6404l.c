#include "psram_aps6404l.h"

#include <string.h>

#define PSRAM_SELF_TEST_SIZE_BYTES (16u)

/**
 * @brief Сбросить диагностические счётчики ошибок.
 * @param ctx Контекст драйвера.
 * @return None.
 */
static void psram_reset_error_counters(psram_ctx_t *ctx)
{
  ctx->status.consecutive_errors = 0u;
  ctx->status.last_error = PSRAM_ERR_OK;
}

/**
 * @brief Применить деградацию при повторных ошибках.
 * @param ctx Контекст драйвера.
 * @param error Код ошибки, [enum].
 * @return None.
 */
static void psram_note_error(psram_ctx_t *ctx, psram_error_t error)
{
  ctx->status.last_error = error;
  ctx->status.consecutive_errors += 1u;

  if (ctx->status.consecutive_errors >= ctx->cfg.degrade_error_threshold)
  {
    ctx->status.state = PSRAM_STATE_DEGRADED;
  }
}

/**
 * @brief Проверить корректность конфигурации драйвера.
 * @param cfg Конфигурация драйвера.
 * @retval true Конфигурация валидна.
 * @retval false Конфигурация невалидна.
 */
static bool psram_is_cfg_valid(const psram_cfg_t *cfg)
{
  if (cfg == NULL)
  {
    return false;
  }

  if ((cfg->memory_size_bytes == 0u) || (cfg->max_chunk_bytes == 0u))
  {
    return false;
  }

  if ((cfg->max_retries_per_chunk == 0u) || (cfg->degrade_error_threshold == 0u))
  {
    return false;
  }

  return true;
}

/**
 * @brief Проверить диапазон адреса и длины операции.
 * @param ctx Контекст драйвера.
 * @param address_start Адрес начала операции, [байт].
 * @param length_bytes Длина операции, [байт].
 * @retval true Диапазон валиден.
 * @retval false Диапазон невалиден.
 */
static bool psram_is_range_valid(const psram_ctx_t *ctx, uint32_t address_start, size_t length_bytes)
{
  const uint64_t end_address_exclusive = (uint64_t)address_start + (uint64_t)length_bytes;
  return (length_bytes > 0u) && (end_address_exclusive <= (uint64_t)ctx->cfg.memory_size_bytes);
}

/**
 * @brief Проверить, что timing-конфигурация QSPI не изменилась после init/recover.
 * @param ctx Контекст драйвера.
 * @retval true Версия timing-конфигурации совпадает.
 * @retval false Версия timing-конфигурации изменилась.
 */
static bool psram_is_timing_epoch_valid(const psram_ctx_t *ctx)
{
  return (ctx->timing_epoch_snapshot == ctx->port.timing_epoch);
}

/**
 * @brief Попытаться захватить lock для сериализации task-доступа.
 * @param ctx Контекст драйвера.
 * @param requester_task_id Идентификатор task-клиента, [id].
 * @retval PSRAM_ERR_OK Lock захвачен.
 * @retval PSRAM_ERR_LOCKED Lock занят другим клиентом.
 */
static psram_error_t psram_acquire_lock(psram_ctx_t *ctx, uint32_t requester_task_id)
{
  if (ctx->lock_active)
  {
    return PSRAM_ERR_LOCKED;
  }

  ctx->lock_active = true;
  ctx->owner_task_id = requester_task_id;
  return PSRAM_ERR_OK;
}

/**
 * @brief Освободить lock сериализации task-доступа.
 * @param ctx Контекст драйвера.
 * @param requester_task_id Идентификатор task-клиента, [id].
 * @return None.
 */
static void psram_release_lock(psram_ctx_t *ctx, uint32_t requester_task_id)
{
  if (ctx->lock_active && (ctx->owner_task_id == requester_task_id))
  {
    ctx->lock_active = false;
  }
}

/**
 * @brief Преобразовать код порт-слоя QSPI в код драйвера.
 * @param port_status Код порт-слоя.
 * @return Код ошибки драйвера.
 */
static psram_error_t psram_map_port_error(qspi_port_status_t port_status)
{
  if (port_status == QSPI_PORT_TIMEOUT)
  {
    return PSRAM_ERR_TIMEOUT;
  }

  if (port_status == QSPI_PORT_BUS)
  {
    return PSRAM_ERR_BUS;
  }

  return PSRAM_ERR_OK;
}

/**
 * @brief Выполнить chunked операцию чтения/записи с ретраями.
 * @param ctx Контекст драйвера.
 * @param address_start Начальный адрес, [байт].
 * @param buffer Буфер операции.
 * @param length_bytes Длина операции, [байт].
 * @param is_write true=write, false=read.
 * @retval PSRAM_ERR_OK Операция успешна.
 * @retval PSRAM_ERR_TIMEOUT Таймаут транзакции.
 * @retval PSRAM_ERR_BUS Ошибка шины транзакции.
 */
static psram_error_t psram_transfer_chunked(psram_ctx_t *ctx,
                                            uint32_t address_start,
                                            uint8_t *buffer,
                                            size_t length_bytes,
                                            bool is_write)
{
  /* Шаг 1: разбиваем запрос на chunks фиксированного размера. */
  /* Шаг 2: для каждого chunk выполняем bounded retries. */
  /* Шаг 3: обновляем счётчики транзакций и смещаем окно. */
  size_t offset_bytes = 0u;

  while (offset_bytes < length_bytes)
  {
    const size_t remaining_bytes = length_bytes - offset_bytes;
    const size_t chunk_bytes = (remaining_bytes > ctx->cfg.max_chunk_bytes)
                                 ? ctx->cfg.max_chunk_bytes
                                 : remaining_bytes;
    const uint32_t chunk_address = address_start + (uint32_t)offset_bytes;
    uint8_t attempt = 0u;
    qspi_port_status_t port_status = QSPI_PORT_BUS;

    while (attempt < ctx->cfg.max_retries_per_chunk)
    {
      if (is_write)
      {
        port_status = ctx->port.write(ctx->port.low_level_ctx,
                                      chunk_address,
                                      &buffer[offset_bytes],
                                      chunk_bytes);
      }
      else
      {
        port_status = ctx->port.read(ctx->port.low_level_ctx,
                                     chunk_address,
                                     &buffer[offset_bytes],
                                     chunk_bytes);
      }

      if (port_status == QSPI_PORT_OK)
      {
        break;
      }

      attempt += 1u;
    }

    if (port_status != QSPI_PORT_OK)
    {
      return psram_map_port_error(port_status);
    }

    if (is_write)
    {
      ctx->status.total_write_transactions += 1u;
    }
    else
    {
      ctx->status.total_read_transactions += 1u;
    }

    offset_bytes += chunk_bytes;
  }

  return PSRAM_ERR_OK;
}

psram_error_t psram_init(psram_ctx_t *ctx,
                        const psram_cfg_t *cfg,
                        const qspi_port_api_t *port)
{
  /* Шаг 1: проверка входных параметров и конфигурации. */
  /* Шаг 2: копирование конфигурации/портовых callbacks в локальный контекст. */
  /* Шаг 3: инициализация low-level QSPI и перевод состояния в READY/FAULT. */

  // SAFETY: cfg.max_chunk_bytes ограничен tCEM-safe пределом BSP (CE# low pulse width).
  if ((ctx == NULL) || (port == NULL) || (port->init == NULL) || (port->read == NULL) || (port->write == NULL))
  {
    return PSRAM_ERR_PARAM;
  }

  if (!psram_is_cfg_valid(cfg))
  {
    return PSRAM_ERR_PARAM;
  }

  if ((port->tcem_safe_max_chunk_bytes == 0u) || (cfg->max_chunk_bytes > port->tcem_safe_max_chunk_bytes))
  {
    return PSRAM_ERR_PARAM;
  }

  ctx->cfg = *cfg;
  ctx->port = *port;
  ctx->status.state = PSRAM_STATE_UNINIT;
  ctx->status.last_error = PSRAM_ERR_OK;
  ctx->status.consecutive_errors = 0u;
  ctx->status.total_read_transactions = 0u;
  ctx->status.total_write_transactions = 0u;
  ctx->lock_active = false;
  ctx->owner_task_id = 0u;
  ctx->timing_epoch_snapshot = ctx->port.timing_epoch;

  const qspi_port_status_t init_status = ctx->port.init(ctx->port.low_level_ctx);
  if (init_status != QSPI_PORT_OK)
  {
    const psram_error_t error = psram_map_port_error(init_status);
    ctx->status.state = PSRAM_STATE_FAULT;
    ctx->status.last_error = error;
    return error;
  }

  ctx->timing_epoch_snapshot = ctx->port.timing_epoch;
  ctx->status.state = PSRAM_STATE_READY;
  psram_reset_error_counters(ctx);
  return PSRAM_ERR_OK;
}

/**
 * @brief Завершить операцию и стабилизировать состояние драйвера после transfer.
 * @param ctx Контекст драйвера.
 * @param result Результат операции transfer.
 * @return None.
 */
static void psram_finalize_transfer(psram_ctx_t *ctx, psram_error_t result)
{
  if (result == PSRAM_ERR_OK)
  {
    ctx->status.state = PSRAM_STATE_READY;
    psram_reset_error_counters(ctx);
    return;
  }

  psram_note_error(ctx, result);

  if (ctx->status.state == PSRAM_STATE_BUSY)
  {
    ctx->status.state = PSRAM_STATE_READY;
  }
}

psram_error_t psram_read(psram_ctx_t *ctx,
                        uint32_t requester_task_id,
                        uint32_t address_start,
                        uint8_t *buffer_dst,
                        size_t length_bytes)
{
  /* Шаг 1: проверяем состояние драйвера и валидность диапазона. */
  /* Шаг 2: захватываем task-lock для сериализации доступа. */
  /* Шаг 3: выполняем chunked read и обновляем состояние/ошибки. */
  psram_error_t result = PSRAM_ERR_OK;

  if ((ctx == NULL) || (buffer_dst == NULL))
  {
    return PSRAM_ERR_PARAM;
  }

  if (ctx->status.state == PSRAM_STATE_UNINIT)
  {
    return PSRAM_ERR_NOT_INIT;
  }

  if ((ctx->status.state == PSRAM_STATE_DEGRADED) || (ctx->status.state == PSRAM_STATE_FAULT))
  {
    return PSRAM_ERR_NOT_READY;
  }

  if (!psram_is_timing_epoch_valid(ctx))
  {
    psram_note_error(ctx, PSRAM_ERR_NOT_READY);
    ctx->status.state = PSRAM_STATE_DEGRADED;
    return PSRAM_ERR_NOT_READY;
  }

  if (!psram_is_range_valid(ctx, address_start, length_bytes))
  {
    return PSRAM_ERR_PARAM;
  }

  result = psram_acquire_lock(ctx, requester_task_id);
  if (result != PSRAM_ERR_OK)
  {
    return result;
  }

  ctx->status.state = PSRAM_STATE_BUSY;
  result = psram_transfer_chunked(ctx, address_start, buffer_dst, length_bytes, false);

  psram_finalize_transfer(ctx, result);

  psram_release_lock(ctx, requester_task_id);
  return result;
}

psram_error_t psram_write(psram_ctx_t *ctx,
                         uint32_t requester_task_id,
                         uint32_t address_start,
                         const uint8_t *buffer_src,
                         size_t length_bytes)
{
  /* Шаг 1: проверяем состояние драйвера и валидность диапазона. */
  /* Шаг 2: захватываем task-lock для сериализации доступа. */
  /* Шаг 3: выполняем chunked write и обновляем состояние/ошибки. */
  psram_error_t result = PSRAM_ERR_OK;

  if ((ctx == NULL) || (buffer_src == NULL))
  {
    return PSRAM_ERR_PARAM;
  }

  if (ctx->status.state == PSRAM_STATE_UNINIT)
  {
    return PSRAM_ERR_NOT_INIT;
  }

  if ((ctx->status.state == PSRAM_STATE_DEGRADED) || (ctx->status.state == PSRAM_STATE_FAULT))
  {
    return PSRAM_ERR_NOT_READY;
  }

  if (!psram_is_timing_epoch_valid(ctx))
  {
    psram_note_error(ctx, PSRAM_ERR_NOT_READY);
    ctx->status.state = PSRAM_STATE_DEGRADED;
    return PSRAM_ERR_NOT_READY;
  }

  if (!psram_is_range_valid(ctx, address_start, length_bytes))
  {
    return PSRAM_ERR_PARAM;
  }

  result = psram_acquire_lock(ctx, requester_task_id);
  if (result != PSRAM_ERR_OK)
  {
    return result;
  }

  ctx->status.state = PSRAM_STATE_BUSY;
  result = psram_transfer_chunked(ctx, address_start, (uint8_t *)buffer_src, length_bytes, true);

  psram_finalize_transfer(ctx, result);

  psram_release_lock(ctx, requester_task_id);
  return result;
}

psram_error_t psram_self_test(psram_ctx_t *ctx, uint32_t requester_task_id)
{
  static const uint8_t test_pattern[PSRAM_SELF_TEST_SIZE_BYTES] = {
    0xA5u, 0x5Au, 0x3Cu, 0xC3u, 0x55u, 0xAAu, 0x0Fu, 0xF0u,
    0x96u, 0x69u, 0x12u, 0x21u, 0xDEu, 0xEDu, 0xBEu, 0xEFu
  };
  uint8_t readback[PSRAM_SELF_TEST_SIZE_BYTES] = {0u};

  if (ctx == NULL)
  {
    return PSRAM_ERR_PARAM;
  }

  /* Шаг 1: записываем тестовый паттерн в начало PSRAM. */
  /* Шаг 2: читаем паттерн обратно и сравниваем с эталоном. */
  /* Шаг 3: фиксируем DATA_MISMATCH при несовпадении. */

  // SAFETY: self-test выполняется только через task-only API и не используется fast/ISR контуром.

  psram_error_t result = psram_write(ctx,
                                     requester_task_id,
                                     0u,
                                     test_pattern,
                                     PSRAM_SELF_TEST_SIZE_BYTES);
  if (result != PSRAM_ERR_OK)
  {
    return result;
  }

  result = psram_read(ctx,
                      requester_task_id,
                      0u,
                      readback,
                      PSRAM_SELF_TEST_SIZE_BYTES);
  if (result != PSRAM_ERR_OK)
  {
    return result;
  }

  if (memcmp(test_pattern, readback, PSRAM_SELF_TEST_SIZE_BYTES) != 0)
  {
    psram_note_error(ctx, PSRAM_ERR_DATA_MISMATCH);
    return PSRAM_ERR_DATA_MISMATCH;
  }

  return PSRAM_ERR_OK;
}

psram_error_t psram_get_status(const psram_ctx_t *ctx, psram_status_t *status_out)
{
  if ((ctx == NULL) || (status_out == NULL))
  {
    return PSRAM_ERR_PARAM;
  }

  *status_out = ctx->status;
  return PSRAM_ERR_OK;
}

psram_error_t psram_recover(psram_ctx_t *ctx, uint32_t requester_task_id)
{
  /* Шаг 1: проверяем состояние и захватываем lock. */
  /* Шаг 2: повторно запускаем low-level init порта QSPI. */
  /* Шаг 3: переводим драйвер в READY либо FAULT с кодом ошибки. */
  if (ctx == NULL)
  {
    return PSRAM_ERR_PARAM;
  }

  if (ctx->status.state == PSRAM_STATE_UNINIT)
  {
    return PSRAM_ERR_NOT_INIT;
  }

  psram_error_t result = psram_acquire_lock(ctx, requester_task_id);
  if (result != PSRAM_ERR_OK)
  {
    return result;
  }

  const qspi_port_status_t init_status = ctx->port.init(ctx->port.low_level_ctx);
  if (init_status != QSPI_PORT_OK)
  {
    result = psram_map_port_error(init_status);
    ctx->status.state = PSRAM_STATE_FAULT;
    ctx->status.last_error = result;
    psram_release_lock(ctx, requester_task_id);
    return result;
  }

  ctx->timing_epoch_snapshot = ctx->port.timing_epoch;
  ctx->status.state = PSRAM_STATE_READY;
  psram_reset_error_counters(ctx);
  psram_release_lock(ctx, requester_task_id);
  return PSRAM_ERR_OK;
}
