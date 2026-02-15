#include "logging_record.h"
#include <stddef.h>

/**
 * @brief Записать uint16_t в буфер в формате LE.
 * @param dst Буфер назначения.
 * @param value Значение, [безразм.].
 * @return None.
 * @pre dst != NULL.
 */
static void logging_write_u16_le(uint8_t *dst, uint16_t value)
{
  dst[0] = (uint8_t)(value & 0xFFu);
  dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

/**
 * @brief Записать uint32_t в буфер в формате LE.
 * @param dst Буфер назначения.
 * @param value Значение, [безразм.].
 * @return None.
 * @pre dst != NULL.
 */
static void logging_write_u32_le(uint8_t *dst, uint32_t value)
{
  dst[0] = (uint8_t)(value & 0xFFu);
  dst[1] = (uint8_t)((value >> 8) & 0xFFu);
  dst[2] = (uint8_t)((value >> 16) & 0xFFu);
  dst[3] = (uint8_t)((value >> 24) & 0xFFu);
}

/**
 * @brief Прочитать uint16_t из буфера в формате LE.
 * @param src Буфер источника.
 * @return Значение, [безразм.].
 * @pre src != NULL.
 */
static uint16_t logging_read_u16_le(const uint8_t *src)
{
  return (uint16_t)(src[0] | ((uint16_t)src[1] << 8));
}

/**
 * @brief Прочитать uint32_t из буфера в формате LE.
 * @param src Буфер источника.
 * @return Значение, [безразм.].
 * @pre src != NULL.
 */
static uint32_t logging_read_u32_le(const uint8_t *src)
{
  return (uint32_t)(src[0] |
                    ((uint32_t)src[1] << 8) |
                    ((uint32_t)src[2] << 16) |
                    ((uint32_t)src[3] << 24));
}

void logging_record_header_init(logging_record_header_t *hdr,
                                uint8_t type,
                                uint16_t source_id,
                                uint16_t payload_len,
                                const logging_timestamp_t *timestamp,
                                uint16_t seq,
                                uint8_t flags)
{
  if ((hdr == NULL) || (timestamp == NULL))
  {
    return;
  }
  hdr->magic = LOGGING_RECORD_MAGIC;
  hdr->type = type;
  hdr->flags = flags;
  hdr->source_id = source_id;
  hdr->payload_len = payload_len;
  hdr->seq = seq;
  hdr->pwm_period_count = timestamp->pwm_period_count;
  hdr->pwm_subtick = timestamp->pwm_subtick;
}

logging_result_t logging_record_pack_header(uint8_t *dst,
                                            uint32_t dst_len,
                                            const logging_record_header_t *hdr)
{
  if ((dst == NULL) || (hdr == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  if (dst_len < LOGGING_RECORD_HEADER_SIZE)
  {
    return LOGGING_RESULT_NO_SPACE;
  }
  logging_write_u16_le(&dst[0], hdr->magic);
  dst[2] = hdr->type;
  dst[3] = hdr->flags;
  logging_write_u16_le(&dst[4], hdr->source_id);
  logging_write_u16_le(&dst[6], hdr->payload_len);
  logging_write_u16_le(&dst[8], hdr->seq);
  logging_write_u32_le(&dst[10], hdr->pwm_period_count);
  logging_write_u16_le(&dst[14], hdr->pwm_subtick);
  return LOGGING_RESULT_OK;
}

logging_result_t logging_record_unpack_header(logging_record_header_t *hdr,
                                              const uint8_t *src,
                                              uint32_t src_len)
{
  if ((hdr == NULL) || (src == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  if (src_len < LOGGING_RECORD_HEADER_SIZE)
  {
    return LOGGING_RESULT_NO_SPACE;
  }
  hdr->magic = logging_read_u16_le(&src[0]);
  if (hdr->magic != LOGGING_RECORD_MAGIC)
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  hdr->type = src[2];
  hdr->flags = src[3];
  hdr->source_id = logging_read_u16_le(&src[4]);
  hdr->payload_len = logging_read_u16_le(&src[6]);
  hdr->seq = logging_read_u16_le(&src[8]);
  hdr->pwm_period_count = logging_read_u32_le(&src[10]);
  hdr->pwm_subtick = logging_read_u16_le(&src[14]);
  return LOGGING_RESULT_OK;
}
