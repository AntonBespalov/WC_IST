#ifndef LOGGING_CORE_H
#define LOGGING_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include "logging_capture_sram.h"
#include "logging_record.h"
#include "logging_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file logging_core.h
 * @brief Ядро логирования: сессии, запись Records, чтение окна.
 */

/**
 * @brief Контекст ядра логирования.
 */
typedef struct {
  logging_capture_sram_t capture; /**< SRAM-захват, [контекст]. */
  uint32_t log_format_version;    /**< Версия формата логов, [id]. */
  uint16_t seq;                   /**< Номер последовательности, [шаги]. */
  uint32_t dropped_records;       /**< Потерянные записи, [шт]. */
} logging_core_t;

/**
 * @brief Инициализировать ядро логирования.
 * @param core Указатель на контекст.
 * @param capture_buf Буфер захвата.
 * @param capture_size Размер буфера, [байт].
 * @param log_format_version Версия формата, [id].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre core != NULL, capture_buf != NULL.
 */
logging_result_t logging_core_init(logging_core_t *core,
                                  uint8_t *capture_buf,
                                  uint32_t capture_size,
                                  uint32_t log_format_version);

/**
 * @brief Взвести сессию логирования.
 * @param core Указатель на контекст.
 * @param cfg Конфигурация сессии.
 * @param session_id Идентификатор сессии, [id].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre core != NULL, cfg != NULL.
 */
logging_result_t logging_core_arm(logging_core_t *core,
                                 const logging_session_cfg_t *cfg,
                                 uint32_t session_id);

/**
 * @brief Триггер захвата.
 * @param core Указатель на контекст.
 * @return LOGGING_RESULT_OK при успехе.
 * @pre core != NULL.
 */
logging_result_t logging_core_trigger(logging_core_t *core);

/**
 * @brief Остановить сессию.
 * @param core Указатель на контекст.
 * @return LOGGING_RESULT_OK при успехе.
 * @pre core != NULL.
 */
logging_result_t logging_core_stop(logging_core_t *core);

/**
 * @brief Очистить состояние ядра.
 * @param core Указатель на контекст.
 * @return LOGGING_RESULT_OK при успехе.
 * @pre core != NULL.
 */
logging_result_t logging_core_clear(logging_core_t *core);

/**
 * @brief Записать Record (заголовок + payload) в окно захвата.
 * @param core Указатель на контекст.
 * @param hdr Заголовок Record.
 * @param payload Payload-данные.
 * @return LOGGING_RESULT_OK при успехе.
 * @pre core != NULL, hdr != NULL, payload != NULL.
 * @note Для ISR/task окружения настройте critical section в logging_platform.h.
 */
logging_result_t logging_core_write_record(logging_core_t *core,
                                          const logging_record_header_t *hdr,
                                          const uint8_t *payload);

/**
 * @brief Записать Record с авто-инкрементом seq.
 * @param core Указатель на контекст.
 * @param type Тип записи.
 * @param source_id Идентификатор источника.
 * @param payload Payload-данные.
 * @param payload_len Длина payload, [байт].
 * @param timestamp Таймстемп записи.
 * @param flags Флаги записи, [битовая маска].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre core != NULL, payload != NULL, timestamp != NULL.
 */
logging_result_t logging_core_write_record_auto(logging_core_t *core,
                                               uint8_t type,
                                               uint16_t source_id,
                                               const uint8_t *payload,
                                               uint16_t payload_len,
                                               const logging_timestamp_t *timestamp,
                                               uint8_t flags);

/**
 * @brief Прочитать фрагмент окна захвата.
 * @param core Указатель на контекст.
 * @param offset Смещение от начала окна, [байт].
 * @param out Буфер назначения.
 * @param max_len Максимальная длина чтения, [байт].
 * @param out_len Фактическая длина, [байт].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre core != NULL, out != NULL, out_len != NULL.
 */
logging_result_t logging_core_read_chunk(logging_core_t *core,
                                        uint32_t offset,
                                        uint8_t *out,
                                        uint32_t max_len,
                                        uint32_t *out_len);

/**
 * @brief Получить статус сессии.
 * @param core Указатель на контекст.
 * @param status Указатель на статус.
 * @return LOGGING_RESULT_OK при успехе.
 * @pre core != NULL, status != NULL.
 */
logging_result_t logging_core_get_status(const logging_core_t *core,
                                        logging_session_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_CORE_H */
