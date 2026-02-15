#include "logging_core.h"
#include "logging_platform.h"
#include <stddef.h>
#include <string.h>

/**
 * @brief Проверить, есть ли место для записи Record в текущем состоянии.
 * @param core Указатель на контекст.
 * @param total_len Общая длина записи, [байт].
 * @return true, если запись допустима.
 * @pre core != NULL.
 */
static bool logging_core_can_write(const logging_core_t *core, uint32_t total_len)
{
  if (total_len > core->capture.size_bytes)
  {
    return false;
  }
  if (core->capture.state == LOGGING_CAPTURE_TRIGGERED)
  {
    const uint32_t remaining = (core->capture.post_written < core->capture.post_bytes)
        ? (core->capture.post_bytes - core->capture.post_written) : 0u;
    return (total_len <= remaining);
  }
  return true;
}

logging_result_t logging_core_init(logging_core_t *core,
                                  uint8_t *capture_buf,
                                  uint32_t capture_size,
                                  uint32_t log_format_version)
{
  if ((core == NULL) || (capture_buf == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  const logging_result_t res = logging_capture_sram_init(&core->capture, capture_buf, capture_size);
  if (res != LOGGING_RESULT_OK)
  {
    return res;
  }
  core->log_format_version = log_format_version;
  core->seq = 0u;
  core->dropped_records = 0u;
  return LOGGING_RESULT_OK;
}

logging_result_t logging_core_arm(logging_core_t *core,
                                 const logging_session_cfg_t *cfg,
                                 uint32_t session_id)
{
  if ((core == NULL) || (cfg == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  const logging_result_t res = logging_capture_sram_arm(&core->capture,
                                                       session_id,
                                                       cfg->pretrigger_bytes,
                                                       cfg->posttrigger_bytes);
  if (res != LOGGING_RESULT_OK)
  {
    return res;
  }
  core->seq = 0u;
  core->dropped_records = 0u;
  return LOGGING_RESULT_OK;
}

logging_result_t logging_core_trigger(logging_core_t *core)
{
  if (core == NULL)
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  return logging_capture_sram_trigger(&core->capture);
}

logging_result_t logging_core_stop(logging_core_t *core)
{
  if (core == NULL)
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  return logging_capture_sram_stop(&core->capture);
}

logging_result_t logging_core_clear(logging_core_t *core)
{
  if (core == NULL)
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  core->seq = 0u;
  core->dropped_records = 0u;
  return logging_capture_sram_clear(&core->capture);
}

logging_result_t logging_core_write_record(logging_core_t *core,
                                          const logging_record_header_t *hdr,
                                          const uint8_t *payload)
{
  if ((core == NULL) || (hdr == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  if ((hdr->payload_len > 0u) && (payload == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  if ((core->capture.state != LOGGING_CAPTURE_ARMED) && (core->capture.state != LOGGING_CAPTURE_TRIGGERED))
  {
    return LOGGING_RESULT_NOT_ARMED;
  }

  uint8_t header_buf[LOGGING_RECORD_HEADER_SIZE];
  const logging_result_t pack_res = logging_record_pack_header(header_buf,
                                                              LOGGING_RECORD_HEADER_SIZE,
                                                              hdr);
  if (pack_res != LOGGING_RESULT_OK)
  {
    return pack_res;
  }

  const logging_critical_state_t irq_state = LOGGING_CRITICAL_ENTER();
  const uint32_t total_len = (uint32_t)LOGGING_RECORD_HEADER_SIZE + (uint32_t)hdr->payload_len;
  if (!logging_core_can_write(core, total_len))
  {
    core->dropped_records += 1u;
    core->capture.overrun = true;
    core->capture.incomplete = true;
    LOGGING_CRITICAL_EXIT(irq_state);
    return LOGGING_RESULT_NO_SPACE;
  }

  const uint32_t written_hdr = logging_capture_sram_write(&core->capture,
                                                         header_buf,
                                                         LOGGING_RECORD_HEADER_SIZE);
  if (written_hdr != LOGGING_RECORD_HEADER_SIZE)
  {
    core->dropped_records += 1u;
    core->capture.incomplete = true;
    LOGGING_CRITICAL_EXIT(irq_state);
    return LOGGING_RESULT_NO_SPACE;
  }
  if (hdr->payload_len > 0u)
  {
    const uint32_t written_payload = logging_capture_sram_write(&core->capture,
                                                               payload,
                                                               hdr->payload_len);
    if (written_payload != hdr->payload_len)
    {
      core->dropped_records += 1u;
      core->capture.incomplete = true;
      LOGGING_CRITICAL_EXIT(irq_state);
      return LOGGING_RESULT_NO_SPACE;
    }
  }

  LOGGING_CRITICAL_EXIT(irq_state);
  return LOGGING_RESULT_OK;
}

logging_result_t logging_core_write_record_auto(logging_core_t *core,
                                               uint8_t type,
                                               uint16_t source_id,
                                               const uint8_t *payload,
                                               uint16_t payload_len,
                                               const logging_timestamp_t *timestamp,
                                               uint8_t flags)
{
  if ((core == NULL) || (timestamp == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  const logging_critical_state_t irq_state = LOGGING_CRITICAL_ENTER();
  const uint16_t seq = core->seq;
  core->seq = (uint16_t)(core->seq + 1u);
  LOGGING_CRITICAL_EXIT(irq_state);

  logging_record_header_t hdr;
  logging_record_header_init(&hdr,
                             type,
                             source_id,
                             payload_len,
                             timestamp,
                             seq,
                             flags);
  const logging_result_t res = logging_core_write_record(core, &hdr, payload);
  return res;
}

logging_result_t logging_core_read_chunk(logging_core_t *core,
                                        uint32_t offset,
                                        uint8_t *out,
                                        uint32_t max_len,
                                        uint32_t *out_len)
{
  if (core == NULL)
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  return logging_capture_sram_read(&core->capture, offset, out, max_len, out_len);
}

logging_result_t logging_core_get_status(const logging_core_t *core,
                                        logging_session_status_t *status)
{
  if ((core == NULL) || (status == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  return logging_capture_sram_get_status(&core->capture, status);
}
