#ifndef LOGGING_CAPTURE_SRAM_H
#define LOGGING_CAPTURE_SRAM_H

#include <stdbool.h>
#include <stdint.h>
#include "logging_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file logging_capture_sram.h
 * @brief Захват окна логов в SRAM (кольцевой буфер).
 */

/**
 * @brief Контекст SRAM-захвата.
 */
typedef struct {
  uint8_t *buffer;           /**< Буфер захвата, [байт]. */
  uint32_t size_bytes;       /**< Размер буфера, [байт]. */
  uint32_t wptr;             /**< Указатель записи, [байт]. */
  uint32_t pre_bytes;        /**< Окно до триггера, [байт]. */
  uint32_t post_bytes;       /**< Окно после триггера, [байт]. */
  uint32_t pre_filled;       /**< Заполнено до триггера, [байт]. */
  uint32_t post_written;     /**< Записано после триггера, [байт]. */
  uint32_t window_start;     /**< Начало окна, [байт]. */
  uint32_t window_len;       /**< Длина окна, [байт]. */
  uint32_t session_id;       /**< Идентификатор сессии, [id]. */
  uint32_t dropped_bytes;    /**< Потерянные байты, [байт]. */
  bool overrun;              /**< Признак переполнения, [bool]. */
  bool incomplete;           /**< Признак незавершённости, [bool]. */
  logging_capture_state_t state; /**< Состояние сессии, [enum]. */
} logging_capture_sram_t;

/**
 * @brief Инициализировать SRAM-захват.
 * @param capture Указатель на контекст.
 * @param buffer Буфер захвата.
 * @param size_bytes Размер буфера, [байт].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre capture != NULL, buffer != NULL.
 */
logging_result_t logging_capture_sram_init(logging_capture_sram_t *capture,
                                           uint8_t *buffer,
                                           uint32_t size_bytes);

/**
 * @brief Взвести сессию захвата.
 * @param capture Указатель на контекст.
 * @param session_id Идентификатор сессии, [id].
 * @param pre_bytes Окно до триггера, [байт].
 * @param post_bytes Окно после триггера, [байт].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre capture != NULL.
 */
logging_result_t logging_capture_sram_arm(logging_capture_sram_t *capture,
                                          uint32_t session_id,
                                          uint32_t pre_bytes,
                                          uint32_t post_bytes);

/**
 * @brief Активировать триггер захвата.
 * @param capture Указатель на контекст.
 * @return LOGGING_RESULT_OK при успехе.
 * @pre capture != NULL.
 */
logging_result_t logging_capture_sram_trigger(logging_capture_sram_t *capture);

/**
 * @brief Остановить захват (manual stop).
 * @param capture Указатель на контекст.
 * @return LOGGING_RESULT_OK при успехе.
 * @pre capture != NULL.
 */
logging_result_t logging_capture_sram_stop(logging_capture_sram_t *capture);

/**
 * @brief Очистить состояние захвата.
 * @param capture Указатель на контекст.
 * @return LOGGING_RESULT_OK при успехе.
 * @pre capture != NULL.
 */
logging_result_t logging_capture_sram_clear(logging_capture_sram_t *capture);

/**
 * @brief Записать данные в SRAM-захват.
 * @param capture Указатель на контекст.
 * @param data Данные для записи.
 * @param len Длина данных, [байт].
 * @return Число записанных байт, [байт].
 * @pre capture != NULL, data != NULL.
 */
uint32_t logging_capture_sram_write(logging_capture_sram_t *capture,
                                    const uint8_t *data,
                                    uint32_t len);

/**
 * @brief Прочитать фрагмент окна захвата (только после STOPPED).
 * @param capture Указатель на контекст.
 * @param offset Смещение от начала окна, [байт].
 * @param out Буфер назначения.
 * @param max_len Максимальная длина чтения, [байт].
 * @param out_len Фактическая длина, [байт].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre capture != NULL, out != NULL, out_len != NULL.
 */
logging_result_t logging_capture_sram_read(logging_capture_sram_t *capture,
                                           uint32_t offset,
                                           uint8_t *out,
                                           uint32_t max_len,
                                           uint32_t *out_len);

/**
 * @brief Получить статус сессии.
 * @param capture Указатель на контекст.
 * @param status Указатель на статус.
 * @return LOGGING_RESULT_OK при успехе.
 * @pre capture != NULL, status != NULL.
 */
logging_result_t logging_capture_sram_get_status(const logging_capture_sram_t *capture,
                                                 logging_session_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_CAPTURE_SRAM_H */
