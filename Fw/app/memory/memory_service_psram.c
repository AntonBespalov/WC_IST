#include "memory_service_psram.h"

/**
 * @brief Инициализировать сервис PSRAM и feature-режим.
 * @param service Контекст сервиса.
 * @param enable_feature Флаг разрешения функции PSRAM.
 * @param service_task_id Идентификатор task-владельца, [id].
 * @param cfg Конфигурация драйвера PSRAM.
 * @param port Таблица callback порт-слоя QSPI.
 * @retval PSRAM_ERR_OK Инициализация завершена.
 * @retval Прочие значения Ошибка инициализации драйвера.
 */
psram_error_t memory_service_psram_init(memory_service_psram_t *service,
                                        bool enable_feature,
                                        uint32_t service_task_id,
                                        const psram_cfg_t *cfg,
                                        const qspi_port_api_t *port)
{
  if (service == NULL)
  {
    return PSRAM_ERR_PARAM;
  }

  service->enabled = false;
  service->service_task_id = service_task_id;

  if (!enable_feature)
  {
    return PSRAM_ERR_OK;
  }

  const psram_error_t result = psram_init(&service->psram, cfg, port);
  if (result == PSRAM_ERR_OK)
  {
    service->enabled = true;
  }

  return result;
}

/**
 * @brief Записать данные в PSRAM через сервис slow task.
 * @param service Контекст сервиса.
 * @param address_start Адрес начала записи, [байт].
 * @param buffer_src Буфер источника.
 * @param length_bytes Длина записи, [байт].
 * @retval PSRAM_ERR_OK Запись выполнена.
 * @retval PSRAM_ERR_NOT_READY Сервис отключён или не готов.
 * @retval Прочие значения Ошибка драйвера.
 */
psram_error_t memory_service_psram_write(memory_service_psram_t *service,
                                         uint32_t address_start,
                                         const uint8_t *buffer_src,
                                         size_t length_bytes)
{
  if ((service == NULL) || (!service->enabled))
  {
    return PSRAM_ERR_NOT_READY;
  }

  // SAFETY: сервис выполняет доступ к PSRAM только из slow task-контекста.
  return psram_write(&service->psram,
                     service->service_task_id,
                     address_start,
                     buffer_src,
                     length_bytes);
}

/**
 * @brief Прочитать данные из PSRAM через сервис slow task.
 * @param service Контекст сервиса.
 * @param address_start Адрес начала чтения, [байт].
 * @param buffer_dst Буфер назначения.
 * @param length_bytes Длина чтения, [байт].
 * @retval PSRAM_ERR_OK Чтение выполнено.
 * @retval PSRAM_ERR_NOT_READY Сервис отключён или не готов.
 * @retval Прочие значения Ошибка драйвера.
 */
psram_error_t memory_service_psram_read(memory_service_psram_t *service,
                                        uint32_t address_start,
                                        uint8_t *buffer_dst,
                                        size_t length_bytes)
{
  if ((service == NULL) || (!service->enabled))
  {
    return PSRAM_ERR_NOT_READY;
  }

  // SAFETY: fast/ISR пути не вызывают данный API, только slow task сервис.
  return psram_read(&service->psram,
                    service->service_task_id,
                    address_start,
                    buffer_dst,
                    length_bytes);
}
