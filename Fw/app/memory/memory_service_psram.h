#ifndef MEMORY_SERVICE_PSRAM_H
#define MEMORY_SERVICE_PSRAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "psram_aps6404l.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file memory_service_psram.h
 * @brief Сервис-обёртка для использования PSRAM в slow task домене.
 */

/**
 * @brief Контекст сервиса памяти PSRAM.
 */
typedef struct {
  bool enabled;             /**< Флаг включения подсистемы PSRAM. */
  uint32_t service_task_id; /**< Идентификатор владельца task-сервиса, [id]. */
  psram_ctx_t psram;        /**< Экземпляр драйвера PSRAM. */
} memory_service_psram_t;

/**
 * @brief Инициализировать сервис PSRAM.
 * @param service Контекст сервиса.
 * @param enable_feature Флаг feature enable.
 * @param service_task_id Идентификатор task-сервиса, [id].
 * @param cfg Конфигурация драйвера.
 * @param port Таблица callback порт-слоя QSPI.
 * @retval PSRAM_ERR_OK Успешная инициализация.
 * @retval PSRAM_ERR_PARAM Невалидные аргументы.
 * @retval Прочие значения Ошибка инициализации драйвера.
 */
psram_error_t memory_service_psram_init(memory_service_psram_t *service,
                                        bool enable_feature,
                                        uint32_t service_task_id,
                                        const psram_cfg_t *cfg,
                                        const qspi_port_api_t *port);

/**
 * @brief Записать буфер в PSRAM через сервис slow task.
 * @param service Контекст сервиса.
 * @param address_start Адрес начала записи, [байт].
 * @param buffer_src Буфер источника.
 * @param length_bytes Длина записи, [байт].
 * @retval PSRAM_ERR_OK Операция выполнена.
 * @retval PSRAM_ERR_NOT_READY Сервис отключён/деградирован.
 * @retval Прочие значения Ошибка драйвера.
 */
psram_error_t memory_service_psram_write(memory_service_psram_t *service,
                                         uint32_t address_start,
                                         const uint8_t *buffer_src,
                                         size_t length_bytes);

/**
 * @brief Прочитать буфер из PSRAM через сервис slow task.
 * @param service Контекст сервиса.
 * @param address_start Адрес начала чтения, [байт].
 * @param buffer_dst Буфер назначения.
 * @param length_bytes Длина чтения, [байт].
 * @retval PSRAM_ERR_OK Операция выполнена.
 * @retval PSRAM_ERR_NOT_READY Сервис отключён/деградирован.
 * @retval Прочие значения Ошибка драйвера.
 */
psram_error_t memory_service_psram_read(memory_service_psram_t *service,
                                        uint32_t address_start,
                                        uint8_t *buffer_dst,
                                        size_t length_bytes);

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_SERVICE_PSRAM_H */
