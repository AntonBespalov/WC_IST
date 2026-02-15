#include "logging_capture_sram.h"
#include <stddef.h>
#include <string.h>

/**
 * @brief Нормализовать индекс в пределах буфера.
 * @param capture Указатель на контекст.
 * @param index Индекс, [байт].
 * @return Нормализованный индекс, [байт].
 * @pre capture != NULL.
 */
static uint32_t logging_capture_norm(const logging_capture_sram_t *capture, uint32_t index)
{
  return (capture->size_bytes == 0u) ? 0u : (index % capture->size_bytes);
}

/**
 * @brief Скопировать данные в кольцевой буфер (с wrap-around).
 * @param capture Указатель на контекст.
 * @param data Данные для записи.
 * @param len Длина данных, [байт].
 * @return None.
 * @pre capture != NULL, data != NULL.
 */
static void logging_capture_write_ring(logging_capture_sram_t *capture,
                                       const uint8_t *data,
                                       uint32_t len)
{
  uint32_t wptr = logging_capture_norm(capture, capture->wptr);
  const uint32_t tail = capture->size_bytes - wptr;
  if (len <= tail)
  {
    (void)memcpy(&capture->buffer[wptr], data, len);
  }
  else
  {
    (void)memcpy(&capture->buffer[wptr], data, tail);
    (void)memcpy(&capture->buffer[0], &data[tail], len - tail);
  }
  capture->wptr = logging_capture_norm(capture, capture->wptr + len);
}

/**
 * @brief Скопировать данные из окна захвата (с wrap-around).
 * @param capture Указатель на контекст.
 * @param offset Смещение от начала окна, [байт].
 * @param out Буфер назначения.
 * @param len Длина чтения, [байт].
 * @return None.
 * @pre capture != NULL, out != NULL.
 */
static void logging_capture_read_ring(const logging_capture_sram_t *capture,
                                      uint32_t offset,
                                      uint8_t *out,
                                      uint32_t len)
{
  const uint32_t start = logging_capture_norm(capture, capture->window_start + offset);
  const uint32_t tail = capture->size_bytes - start;
  if (len <= tail)
  {
    (void)memcpy(out, &capture->buffer[start], len);
  }
  else
  {
    (void)memcpy(out, &capture->buffer[start], tail);
    (void)memcpy(&out[tail], &capture->buffer[0], len - tail);
  }
}

logging_result_t logging_capture_sram_init(logging_capture_sram_t *capture,
                                           uint8_t *buffer,
                                           uint32_t size_bytes)
{
  if ((capture == NULL) || (buffer == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  if (size_bytes == 0u)
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  capture->buffer = buffer;
  capture->size_bytes = size_bytes;
  capture->wptr = 0u;
  capture->pre_bytes = 0u;
  capture->post_bytes = 0u;
  capture->pre_filled = 0u;
  capture->post_written = 0u;
  capture->window_start = 0u;
  capture->window_len = 0u;
  capture->session_id = 0u;
  capture->dropped_bytes = 0u;
  capture->overrun = false;
  capture->incomplete = false;
  capture->state = LOGGING_CAPTURE_IDLE;
  return LOGGING_RESULT_OK;
}

logging_result_t logging_capture_sram_arm(logging_capture_sram_t *capture,
                                          uint32_t session_id,
                                          uint32_t pre_bytes,
                                          uint32_t post_bytes)
{
  if (capture == NULL)
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  if ((pre_bytes + post_bytes) > capture->size_bytes)
  {
    return LOGGING_RESULT_NO_SPACE;
  }
  capture->wptr = 0u;
  capture->pre_bytes = pre_bytes;
  capture->post_bytes = post_bytes;
  capture->pre_filled = 0u;
  capture->post_written = 0u;
  capture->window_start = 0u;
  capture->window_len = 0u;
  capture->session_id = session_id;
  capture->dropped_bytes = 0u;
  capture->overrun = false;
  capture->incomplete = false;
  capture->state = LOGGING_CAPTURE_ARMED;
  return LOGGING_RESULT_OK;
}

logging_result_t logging_capture_sram_trigger(logging_capture_sram_t *capture)
{
  if (capture == NULL)
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  if (capture->state != LOGGING_CAPTURE_ARMED)
  {
    return LOGGING_RESULT_NOT_ARMED;
  }
  const uint32_t available_pre = (capture->pre_filled < capture->pre_bytes) ?
      capture->pre_filled : capture->pre_bytes;
  capture->window_start = logging_capture_norm(capture, capture->wptr + capture->size_bytes - available_pre);
  capture->window_len = available_pre;
  capture->post_written = 0u;
  capture->state = LOGGING_CAPTURE_TRIGGERED;
  return LOGGING_RESULT_OK;
}

logging_result_t logging_capture_sram_stop(logging_capture_sram_t *capture)
{
  if (capture == NULL)
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  if ((capture->state == LOGGING_CAPTURE_TRIGGERED) && (capture->post_written < capture->post_bytes))
  {
    capture->incomplete = true;
  }
  if (capture->state == LOGGING_CAPTURE_ARMED)
  {
    capture->incomplete = true;
  }
  capture->state = LOGGING_CAPTURE_STOPPED;
  return LOGGING_RESULT_OK;
}

logging_result_t logging_capture_sram_clear(logging_capture_sram_t *capture)
{
  if (capture == NULL)
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  capture->wptr = 0u;
  capture->pre_bytes = 0u;
  capture->post_bytes = 0u;
  capture->pre_filled = 0u;
  capture->post_written = 0u;
  capture->window_start = 0u;
  capture->window_len = 0u;
  capture->session_id = 0u;
  capture->dropped_bytes = 0u;
  capture->overrun = false;
  capture->incomplete = false;
  capture->state = LOGGING_CAPTURE_IDLE;
  return LOGGING_RESULT_OK;
}

uint32_t logging_capture_sram_write(logging_capture_sram_t *capture,
                                    const uint8_t *data,
                                    uint32_t len)
{
  if ((capture == NULL) || (data == NULL))
  {
    return 0u;
  }
  if ((capture->state != LOGGING_CAPTURE_ARMED) && (capture->state != LOGGING_CAPTURE_TRIGGERED))
  {
    return 0u;
  }
  if (len == 0u)
  {
    return 0u;
  }

  if (capture->state == LOGGING_CAPTURE_ARMED)
  {
    if (len > capture->size_bytes)
    {
      const uint32_t drop = len - capture->size_bytes;
      capture->dropped_bytes += drop;
      data = &data[drop];
      len = capture->size_bytes;
    }
    const uint32_t pre_before = capture->pre_filled;
    const uint32_t pre_after = pre_before + len;
    if (pre_after > capture->size_bytes)
    {
      capture->dropped_bytes += (pre_after - capture->size_bytes);
    }
    capture->pre_filled = (pre_after > capture->size_bytes) ? capture->size_bytes : pre_after;
    logging_capture_write_ring(capture, data, len);
    return len;
  }

  if (capture->state == LOGGING_CAPTURE_TRIGGERED)
  {
    if (capture->post_written >= capture->post_bytes)
    {
      capture->state = LOGGING_CAPTURE_STOPPED;
      return 0u;
    }
    uint32_t allowed = capture->post_bytes - capture->post_written;
    uint32_t write_len = (len < allowed) ? len : allowed;
    if (len > allowed)
    {
      capture->dropped_bytes += (len - allowed);
    }
    logging_capture_write_ring(capture, data, write_len);
    capture->post_written += write_len;
    capture->window_len += write_len;
    if (capture->post_written >= capture->post_bytes)
    {
      capture->state = LOGGING_CAPTURE_STOPPED;
    }
    return write_len;
  }

  return 0u;
}

logging_result_t logging_capture_sram_read(logging_capture_sram_t *capture,
                                           uint32_t offset,
                                           uint8_t *out,
                                           uint32_t max_len,
                                           uint32_t *out_len)
{
  if ((capture == NULL) || (out == NULL) || (out_len == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  if (capture->state != LOGGING_CAPTURE_STOPPED)
  {
    *out_len = 0u;
    return LOGGING_RESULT_NOT_READY;
  }
  if (offset >= capture->window_len)
  {
    *out_len = 0u;
    return LOGGING_RESULT_OK;
  }
  uint32_t available = capture->window_len - offset;
  uint32_t read_len = (available < max_len) ? available : max_len;
  logging_capture_read_ring(capture, offset, out, read_len);
  *out_len = read_len;
  return LOGGING_RESULT_OK;
}

logging_result_t logging_capture_sram_get_status(const logging_capture_sram_t *capture,
                                                 logging_session_status_t *status)
{
  if ((capture == NULL) || (status == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  status->session_id = capture->session_id;
  status->state = capture->state;
  status->window_len = capture->window_len;
  status->dropped_bytes = capture->dropped_bytes;
  status->overrun = capture->overrun;
  status->incomplete = capture->incomplete;
  return LOGGING_RESULT_OK;
}