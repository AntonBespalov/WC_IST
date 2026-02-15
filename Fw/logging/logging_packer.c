#include "logging_packer.h"
#include <stddef.h>
#include <string.h>

/**
 * @brief Проверить endianness платформы.
 * @return true, если little-endian.
 */
static bool logging_is_little_endian(void)
{
  const uint16_t value = 1u;
  const uint8_t *bytes = (const uint8_t *)&value;
  return (bytes[0] == 1u);
}

/**
 * @brief Скопировать поле в LE-порядке.
 * @param dst Буфер назначения.
 * @param src Буфер источника.
 * @param size Размер поля, [байт].
 * @return None.
 * @pre dst != NULL, src != NULL.
 */
static void logging_copy_le(uint8_t *dst, const uint8_t *src, uint16_t size)
{
  if (logging_is_little_endian())
  {
    (void)memcpy(dst, src, size);
    return;
  }
  for (uint16_t i = 0u; i < size; ++i)
  {
    dst[i] = src[size - 1u - i];
  }
}

logging_result_t logging_packer_pack(const void *snapshot,
                                    const logging_field_desc_t *fields,
                                    uint32_t field_count,
                                    uint8_t *out,
                                    uint32_t out_len,
                                    uint32_t *written)
{
  if ((snapshot == NULL) || (fields == NULL) || (out == NULL) || (written == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  uint32_t out_pos = 0u;
  const uint8_t *base = (const uint8_t *)snapshot;
  for (uint32_t i = 0u; i < field_count; ++i)
  {
    const uint16_t size_bytes = fields[i].size_bytes;
    const uint16_t offset_bytes = fields[i].offset_bytes;
    if (size_bytes == 0u)
    {
      continue;
    }
    if ((out_pos + size_bytes) > out_len)
    {
      *written = out_pos;
      return LOGGING_RESULT_NO_SPACE;
    }
    logging_copy_le(&out[out_pos], &base[offset_bytes], size_bytes);
    out_pos += size_bytes;
  }
  *written = out_pos;
  return LOGGING_RESULT_OK;
}