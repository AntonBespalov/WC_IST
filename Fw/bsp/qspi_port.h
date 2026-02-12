#ifndef QSPI_PORT_H
#define QSPI_PORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file qspi_port.h
 * @brief Порт-слой QSPI для APS6404L (адаптер к HAL/BSP).
 */

/**
 * @brief Коды возврата низкоуровневого QSPI порта.
 */
typedef enum {
  QSPI_PORT_OK = 0,      /**< Транзакция выполнена успешно. */
  QSPI_PORT_TIMEOUT = 1, /**< Таймаут выполнения транзакции. */
  QSPI_PORT_BUS = 2      /**< Ошибка шины/контроллера QSPI. */
} qspi_port_status_t;

/**
 * @brief Сигнатура callback инициализации QSPI.
 * @param low_level_ctx Указатель на контекст порта/драйвера HAL.
 * @retval QSPI_PORT_OK Успешная инициализация.
 * @retval QSPI_PORT_TIMEOUT Таймаут инициализации.
 * @retval QSPI_PORT_BUS Ошибка шины/конфигурации.
 */
typedef qspi_port_status_t (*qspi_port_init_fn_t)(void *low_level_ctx);

/**
 * @brief Сигнатура callback чтения из QSPI памяти.
 * @param low_level_ctx Указатель на контекст порта/драйвера HAL.
 * @param address_start Адрес начала чтения, [байт].
 * @param buffer_dst Буфер назначения.
 * @param length_bytes Длина чтения, [байт].
 * @retval QSPI_PORT_OK Чтение успешно.
 * @retval QSPI_PORT_TIMEOUT Таймаут транзакции.
 * @retval QSPI_PORT_BUS Ошибка шины/контроллера.
 */
typedef qspi_port_status_t (*qspi_port_read_fn_t)(void *low_level_ctx,
                                                  uint32_t address_start,
                                                  uint8_t *buffer_dst,
                                                  size_t length_bytes);

/**
 * @brief Сигнатура callback записи в QSPI память.
 * @param low_level_ctx Указатель на контекст порта/драйвера HAL.
 * @param address_start Адрес начала записи, [байт].
 * @param buffer_src Буфер источника.
 * @param length_bytes Длина записи, [байт].
 * @retval QSPI_PORT_OK Запись успешна.
 * @retval QSPI_PORT_TIMEOUT Таймаут транзакции.
 * @retval QSPI_PORT_BUS Ошибка шины/контроллера.
 */
typedef qspi_port_status_t (*qspi_port_write_fn_t)(void *low_level_ctx,
                                                   uint32_t address_start,
                                                   const uint8_t *buffer_src,
                                                   size_t length_bytes);

/**
 * @brief Таблица функций порт-слоя QSPI.
 */
typedef struct {
  void *low_level_ctx;               /**< Контекст HAL/BSP для callbacks. */
  qspi_port_init_fn_t init;          /**< Инициализация контроллера/микросхемы. */
  qspi_port_read_fn_t read;          /**< Чтение из PSRAM. */
  qspi_port_write_fn_t write;        /**< Запись в PSRAM. */
} qspi_port_api_t;

#ifdef __cplusplus
}
#endif

#endif /* QSPI_PORT_H */
