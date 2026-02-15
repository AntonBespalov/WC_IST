#include "logging_crc32.h"
#include <stddef.h>

/**
 * @brief Записать uint32_t в буфер в формате LE.
 * @param dst Буфер назначения.
 * @param value Значение, [безразм.].
 * @return None.
 * @pre dst != NULL.
 */
static void logging_crc32_write_le(uint8_t *dst, uint32_t value)
{
  dst[0] = (uint8_t)(value & 0xFFu);
  dst[1] = (uint8_t)((value >> 8) & 0xFFu);
  dst[2] = (uint8_t)((value >> 16) & 0xFFu);
  dst[3] = (uint8_t)((value >> 24) & 0xFFu);
}

uint32_t logging_crc32_compute(const uint8_t *data, uint32_t len)
{
  if (data == NULL)
  {
    return 0u;
  }
  uint32_t crc = 0xFFFFFFFFu;
  for (uint32_t i = 0u; i < len; ++i)
  {
    crc ^= (uint32_t)data[i];
    for (uint32_t bit = 0u; bit < 8u; ++bit)
    {
      const uint32_t lsb = crc & 1u;
      crc >>= 1u;
      if (lsb != 0u)
      {
        crc ^= 0xEDB88320u;
      }
    }
  }
  return ~crc;
}

logging_result_t logging_crc32_append(uint8_t *payload,
                                      uint16_t payload_len,
                                      uint16_t payload_capacity,
                                      uint16_t *out_len)
{
  if ((payload == NULL) || (out_len == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  if ((uint32_t)payload_len + LOGGING_CRC32_SIZE > payload_capacity)
  {
    return LOGGING_RESULT_NO_SPACE;
  }
  const uint32_t crc = logging_crc32_compute(payload, payload_len);
  logging_crc32_write_le(&payload[payload_len], crc);
  *out_len = (uint16_t)(payload_len + LOGGING_CRC32_SIZE);
  return LOGGING_RESULT_OK;
}