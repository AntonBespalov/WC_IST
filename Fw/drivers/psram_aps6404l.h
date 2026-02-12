#ifndef PSRAM_APS6404L_H
#define PSRAM_APS6404L_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "qspi_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file psram_aps6404l.h
 * @brief Драйвер PSRAM APS6404L (task-only API, без ISR вызовов).
 */

/**
 * @brief Состояние драйвера APS6404L.
 */
typedef enum {
  PSRAM_STATE_UNINIT = 0, /**< Драйвер не инициализирован. */
  PSRAM_STATE_READY = 1,  /**< Драйвер готов к операциям. */
  PSRAM_STATE_BUSY = 2,   /**< Драйвер выполняет транзакцию. */
  PSRAM_STATE_DEGRADED = 3, /**< Подсистема деградирована после повторных ошибок. */
  PSRAM_STATE_FAULT = 4   /**< Необслуживаемая ошибка, требуется recovery. */
} psram_state_t;

/**
 * @brief Формализованный код ошибки драйвера.
 */
typedef enum {
  PSRAM_ERR_OK = 0,            /**< Ошибок нет. */
  PSRAM_ERR_PARAM = 1,         /**< Невалидные параметры API. */
  PSRAM_ERR_NOT_INIT = 2,      /**< Драйвер не инициализирован. */
  PSRAM_ERR_NOT_READY = 3,     /**< Драйвер не готов (degraded/fault/busy). */
  PSRAM_ERR_TIMEOUT = 4,       /**< Таймаут низкоуровневой транзакции. */
  PSRAM_ERR_BUS = 5,           /**< Ошибка шины QSPI. */
  PSRAM_ERR_DATA_MISMATCH = 6, /**< Несовпадение write/readback. */
  PSRAM_ERR_LOCKED = 7,        /**< Конфликт сериализации доступа между task-клиентами. */
  PSRAM_ERR_TIMING_CHANGED = 8 /**< Обнаружено изменение timing-конфигурации QSPI, требуется recover. */
} psram_error_t;

/**
 * @brief Причина последней неготовности драйвера.
 */
typedef enum {
  PSRAM_NOT_READY_REASON_NONE = 0,           /**< Причина не зафиксирована. */
  PSRAM_NOT_READY_REASON_STATE = 1,          /**< Драйвер в состоянии DEGRADED/FAULT. */
  PSRAM_NOT_READY_REASON_TIMING_CHANGED = 2  /**< Изменился timing_epoch QSPI. */
} psram_not_ready_reason_t;

/**
 * @brief Конфигурация драйвера APS6404L.
 */
typedef struct {
  uint32_t memory_size_bytes;          /**< Размер доступного окна PSRAM, [байт]. */
  size_t max_chunk_bytes;              /**< Максимальный размер одной транзакции, [байт], не больше BSP tCEM-safe лимита. */
  uint8_t max_retries_per_chunk;       /**< Количество повторов на chunk, [раз]. */
  uint8_t degrade_error_threshold;     /**< Порог ошибок подряд для DEGRADED, [раз]. */
} psram_cfg_t;

/**
 * @brief Диагностический снимок состояния драйвера.
 */
typedef struct {
  psram_state_t state;                 /**< Текущее состояние драйвера. */
  psram_error_t last_error;            /**< Последняя ошибка API/транзакции. */
  psram_not_ready_reason_t last_not_ready_reason; /**< Причина последней неготовности, [enum]. */
  uint32_t consecutive_errors;         /**< Ошибок подряд, [раз]. */
  uint32_t total_read_transactions;    /**< Всего read chunk-транзакций, [раз]. */
  uint32_t total_write_transactions;   /**< Всего write chunk-транзакций, [раз]. */
} psram_status_t;

/**
 * @brief Контекст экземпляра драйвера APS6404L.
 */
typedef struct {
  psram_cfg_t cfg;                     /**< Локальная копия конфигурации. */
  qspi_port_api_t port;                /**< Таблица callback порт-слоя. */
  psram_status_t status;               /**< Текущее состояние и диагностика. */
  bool lock_active;                    /**< Признак активной транзакции/критической секции. */
  uint32_t owner_task_id;              /**< Идентификатор владельца lock, [id]. */
  uint32_t timing_epoch_snapshot;      /**< Зафиксированная версия timing-конфигурации QSPI после init/recover, [счётчик]. */
} psram_ctx_t;

/**
 * @brief Инициализировать драйвер APS6404L и проверить доступность QSPI.
 * @param ctx Контекст драйвера.
 * @param cfg Конфигурация драйвера.
 * @param port Таблица callback порт-слоя QSPI.
 * @retval PSRAM_ERR_OK Инициализация прошла успешно.
 * @retval PSRAM_ERR_PARAM Неверные аргументы конфигурации/API (в т.ч. нарушение tCEM-safe лимита chunk).
 * @retval PSRAM_ERR_TIMEOUT Таймаут инициализации порта.
 * @retval PSRAM_ERR_BUS Ошибка шины при инициализации.
 */
psram_error_t psram_init(psram_ctx_t *ctx,
                        const psram_cfg_t *cfg,
                        const qspi_port_api_t *port);

/**
 * @brief Прочитать блок данных из APS6404L с chunked transfer.
 * @param ctx Контекст драйвера.
 * @param requester_task_id Идентификатор task-клиента, [id].
 * @param address_start Адрес начала чтения, [байт].
 * @param buffer_dst Буфер назначения.
 * @param length_bytes Длина чтения, [байт].
 * @retval PSRAM_ERR_OK Операция успешна.
 * @retval PSRAM_ERR_PARAM Невалидные аргументы или выход за границы памяти.
 * @retval PSRAM_ERR_NOT_INIT Драйвер не инициализирован.
 * @retval PSRAM_ERR_NOT_READY Драйвер в состоянии неготовности.
 * @retval PSRAM_ERR_TIMING_CHANGED Обнаружено изменение timing-конфигурации QSPI, требуется recover.
 * @retval PSRAM_ERR_TIMEOUT Таймаут на уровне port API.
 * @retval PSRAM_ERR_BUS Ошибка шины на уровне port API.
 * @retval PSRAM_ERR_LOCKED Конфликт сериализации доступа.
 */
psram_error_t psram_read(psram_ctx_t *ctx,
                        uint32_t requester_task_id,
                        uint32_t address_start,
                        uint8_t *buffer_dst,
                        size_t length_bytes);

/**
 * @brief Записать блок данных в APS6404L с chunked transfer.
 * @param ctx Контекст драйвера.
 * @param requester_task_id Идентификатор task-клиента, [id].
 * @param address_start Адрес начала записи, [байт].
 * @param buffer_src Буфер источника.
 * @param length_bytes Длина записи, [байт].
 * @retval PSRAM_ERR_OK Операция успешна.
 * @retval PSRAM_ERR_PARAM Невалидные аргументы или выход за границы памяти.
 * @retval PSRAM_ERR_NOT_INIT Драйвер не инициализирован.
 * @retval PSRAM_ERR_NOT_READY Драйвер в состоянии неготовности.
 * @retval PSRAM_ERR_TIMING_CHANGED Обнаружено изменение timing-конфигурации QSPI, требуется recover.
 * @retval PSRAM_ERR_TIMEOUT Таймаут на уровне port API.
 * @retval PSRAM_ERR_BUS Ошибка шины на уровне port API.
 * @retval PSRAM_ERR_LOCKED Конфликт сериализации доступа.
 */
psram_error_t psram_write(psram_ctx_t *ctx,
                         uint32_t requester_task_id,
                         uint32_t address_start,
                         const uint8_t *buffer_src,
                         size_t length_bytes);

/**
 * @brief Выполнить базовый self-test (write/readback тестовый паттерн).
 * @param ctx Контекст драйвера.
 * @param requester_task_id Идентификатор task-клиента, [id].
 * @retval PSRAM_ERR_OK Self-test успешен.
 * @retval PSRAM_ERR_DATA_MISMATCH Данные readback не совпали с эталоном.
 * @retval Прочие значения Ошибки чтения/записи/состояния.
 */
psram_error_t psram_self_test(psram_ctx_t *ctx, uint32_t requester_task_id);

/**
 * @brief Получить диагностический снимок состояния драйвера.
 * @param ctx Контекст драйвера.
 * @param status_out Буфер для снимка статуса.
 * @retval PSRAM_ERR_OK Статус успешно прочитан.
 * @retval PSRAM_ERR_PARAM Невалидные аргументы.
 */
psram_error_t psram_get_status(const psram_ctx_t *ctx, psram_status_t *status_out);

/**
 * @brief Явно восстановить драйвер из состояния DEGRADED/FAULT.
 * @param ctx Контекст драйвера.
 * @param requester_task_id Идентификатор task-клиента, [id].
 * @retval PSRAM_ERR_OK Recovery завершён успешно.
 * @retval PSRAM_ERR_NOT_INIT Драйвер не инициализирован.
 * @retval PSRAM_ERR_TIMEOUT Таймаут инициализации порта.
 * @retval PSRAM_ERR_BUS Ошибка шины при инициализации.
 * @retval PSRAM_ERR_LOCKED Конфликт сериализации доступа.
 */
psram_error_t psram_recover(psram_ctx_t *ctx, uint32_t requester_task_id);

#ifdef __cplusplus
}
#endif

#endif /* PSRAM_APS6404L_H */
