#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "logging_capture_sram.h"
#include "logging_core.h"
#include "logging_packer.h"
#include "logging_record.h"
#include "logging_spsc.h"
#include "logging_tx_scheduler.h"

/**
 * @brief Контекст простого тестового раннера.
 */
typedef struct {
  int failed; /**< Количество проваленных проверок, [шт]. */
} test_ctx_t;

/**
 * @brief Тип функции теста.
 */
typedef void (*test_fn_t)(test_ctx_t *ctx);

/**
 * @brief Описание одного теста.
 */
typedef struct {
  const char *name; /**< Имя теста (стабильный идентификатор), [строка]. */
  test_fn_t fn;     /**< Указатель на функцию теста. */
} test_case_t;

/**
 * @brief Проверить булево условие.
 * @param ctx Контекст тестов.
 * @param condition Условие.
 * @param message Сообщение об ошибке.
 * @return None.
 */
static void test_expect_true(test_ctx_t *ctx, bool condition, const char *message)
{
  if (!condition)
  {
    ctx->failed += 1;
    (void)printf("FAIL: %s\n", message);
  }
}

/**
 * @brief Проверить равенство значений uint32_t.
 * @param ctx Контекст тестов.
 * @param actual Фактическое значение.
 * @param expected Ожидаемое значение.
 * @param message Сообщение об ошибке.
 * @return None.
 */
static void test_expect_u32(test_ctx_t *ctx,
                            uint32_t actual,
                            uint32_t expected,
                            const char *message)
{
  if (actual != expected)
  {
    ctx->failed += 1;
    (void)printf("FAIL: %s (actual=%lu expected=%lu)\n",
                message,
                (unsigned long)actual,
                (unsigned long)expected);
  }
}

/**
 * @brief Тест: упаковка и распаковка заголовка Record.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_record_pack_roundtrip(test_ctx_t *ctx)
{
  logging_record_header_t hdr;
  logging_timestamp_t ts = {0u};
  ts.pwm_period_count = 1234u;
  ts.pwm_subtick = 7u;
  ts.domain_id = 1u;

  logging_record_header_init(&hdr, LOGGING_RECORD_TYPE_CTRL, 42u, 16u, &ts, 99u, 0x5Au);

  uint8_t buf[LOGGING_RECORD_HEADER_SIZE] = {0u};
  const logging_result_t pack_res = logging_record_pack_header(buf, sizeof(buf), &hdr);
  test_expect_true(ctx, pack_res == LOGGING_RESULT_OK, "pack header");

  logging_record_header_t out = {0u};
  const logging_result_t unpack_res = logging_record_unpack_header(&out, buf, sizeof(buf));
  test_expect_true(ctx, unpack_res == LOGGING_RESULT_OK, "unpack header");
  test_expect_u32(ctx, out.type, hdr.type, "type");
  test_expect_u32(ctx, out.source_id, hdr.source_id, "source_id");
  test_expect_u32(ctx, out.payload_len, hdr.payload_len, "payload_len");
  test_expect_u32(ctx, out.seq, hdr.seq, "seq");
  test_expect_u32(ctx, out.pwm_period_count, hdr.pwm_period_count, "pwm_period_count");
  test_expect_u32(ctx, out.pwm_subtick, hdr.pwm_subtick, "pwm_subtick");
}

/**
 * @brief Тест: базовая работа SPSC очереди.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_spsc_basic(test_ctx_t *ctx)
{
  uint8_t storage[16] = {0u};
  logging_spsc_t queue;
  const logging_result_t init_res = logging_spsc_init(&queue,
                                                     storage,
                                                     sizeof(storage),
                                                     4u,
                                                     sizeof(uint32_t));
  test_expect_true(ctx, init_res == LOGGING_RESULT_OK, "spsc init");

  uint32_t value = 11u;
  test_expect_true(ctx, logging_spsc_push(&queue, &value), "spsc push 1");
  value = 22u;
  test_expect_true(ctx, logging_spsc_push(&queue, &value), "spsc push 2");
  value = 33u;
  test_expect_true(ctx, logging_spsc_push(&queue, &value), "spsc push 3");

  uint32_t out = 0u;
  test_expect_true(ctx, logging_spsc_pop(&queue, &out), "spsc pop 1");
  test_expect_u32(ctx, out, 11u, "spsc order 1");
  test_expect_true(ctx, logging_spsc_pop(&queue, &out), "spsc pop 2");
  test_expect_u32(ctx, out, 22u, "spsc order 2");
  test_expect_true(ctx, logging_spsc_pop(&queue, &out), "spsc pop 3");
  test_expect_u32(ctx, out, 33u, "spsc order 3");
}

/**
 * @brief Тест: упаковка snapshot по профилю.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_packer_profile(test_ctx_t *ctx)
{
  typedef struct {
    uint16_t a; /**< Поле A, [id]. */
    uint32_t b; /**< Поле B, [id]. */
    uint8_t c;  /**< Поле C, [id]. */
  } test_snapshot_t;

  const test_snapshot_t snap = {0x1234u, 0x89ABCDEFu, 0x5Au};
  logging_field_desc_t fields[3] = {
    {1u, (uint16_t)offsetof(test_snapshot_t, c), sizeof(snap.c), 0u},
    {2u, (uint16_t)offsetof(test_snapshot_t, a), sizeof(snap.a), 0u},
    {3u, (uint16_t)offsetof(test_snapshot_t, b), sizeof(snap.b), 0u}
  };

  uint8_t out[16] = {0u};
  uint32_t written = 0u;
  const logging_result_t res = logging_packer_pack(&snap, fields, 3u, out, sizeof(out), &written);
  test_expect_true(ctx, res == LOGGING_RESULT_OK, "packer result");
  test_expect_u32(ctx, written, 7u, "packer length");

  const uint8_t expected[7] = {0x5Au, 0x34u, 0x12u, 0xEFu, 0xCDu, 0xABu, 0x89u};
  test_expect_true(ctx, (memcmp(out, expected, sizeof(expected)) == 0), "packer content");
}

/**
 * @brief Тест: окно pre/post trigger в SRAM-захвате.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_capture_window(test_ctx_t *ctx)
{
  uint8_t buffer[32] = {0u};
  logging_capture_sram_t cap;
  test_expect_true(ctx,
                   logging_capture_sram_init(&cap, buffer, sizeof(buffer)) == LOGGING_RESULT_OK,
                   "capture init");

  test_expect_true(ctx,
                   logging_capture_sram_arm(&cap, 1u, 8u, 8u) == LOGGING_RESULT_OK,
                   "capture arm");

  uint8_t pre[10];
  for (uint8_t i = 0u; i < 10u; ++i)
  {
    pre[i] = (uint8_t)(i + 1u);
  }
  (void)logging_capture_sram_write(&cap, pre, sizeof(pre));

  test_expect_true(ctx,
                   logging_capture_sram_trigger(&cap) == LOGGING_RESULT_OK,
                   "capture trigger");

  uint8_t post[8];
  for (uint8_t i = 0u; i < 8u; ++i)
  {
    post[i] = (uint8_t)(i + 11u);
  }
  (void)logging_capture_sram_write(&cap, post, sizeof(post));

  uint8_t out[16] = {0u};
  uint32_t out_len = 0u;
  test_expect_true(ctx,
                   logging_capture_sram_read(&cap, 0u, out, sizeof(out), &out_len) == LOGGING_RESULT_OK,
                   "capture read");
  test_expect_u32(ctx, out_len, 16u, "capture window len");

  uint8_t expected[16];
  for (uint8_t i = 0u; i < 8u; ++i)
  {
    expected[i] = (uint8_t)(i + 3u);
  }
  for (uint8_t i = 0u; i < 8u; ++i)
  {
    expected[8u + i] = (uint8_t)(i + 11u);
  }
  test_expect_true(ctx, (memcmp(out, expected, sizeof(expected)) == 0), "capture content");
}

/**
 * @brief Контекст тестового планировщика TX.
 */
typedef struct {
  bool pdo_ready;  /**< Наличие PDO, [bool]. */
  bool log_ready;  /**< Наличие LOG, [bool]. */
  uint32_t log_min_len; /**< Минимальная длина LOG, [байт]. */
  uint32_t pdo_len; /**< Длина PDO, [байт]. */
} test_tx_ctx_t;

/**
 * @brief Проверить наличие PDO.
 * @param ctx Контекст.
 * @return true, если есть PDO.
 */
static bool test_tx_has_pdo(void *ctx)
{
  return ((test_tx_ctx_t *)ctx)->pdo_ready;
}

/**
 * @brief Проверить наличие LOG.
 * @param ctx Контекст.
 * @return true, если есть LOG.
 */
static bool test_tx_has_log(void *ctx)
{
  return ((test_tx_ctx_t *)ctx)->log_ready;
}

/**
 * @brief Выдать PDO кадр.
 * @param ctx Контекст.
 * @param out Буфер.
 * @param max_len Макс. длина, [байт].
 * @return Фактическая длина, [байт].
 */
static uint32_t test_tx_pop_pdo(void *ctx, uint8_t *out, uint32_t max_len)
{
  const test_tx_ctx_t *state = (const test_tx_ctx_t *)ctx;
  if (max_len < state->pdo_len)
  {
    return 0u;
  }
  out[0] = 0xA5u;
  return state->pdo_len;
}

/**
 * @brief Выдать LOG кадр.
 * @param ctx Контекст.
 * @param out Буфер.
 * @param max_len Макс. длина, [байт].
 * @return Фактическая длина, [байт].
 */
static uint32_t test_tx_pop_log(void *ctx, uint8_t *out, uint32_t max_len)
{
  const test_tx_ctx_t *state = (const test_tx_ctx_t *)ctx;
  if (max_len < state->log_min_len)
  {
    return 0u;
  }
  out[0] = 0x5Au;
  return state->log_min_len;
}

/**
 * @brief Тест: приоритет PDO и бюджет LOG.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_tx_scheduler_basic(test_ctx_t *ctx)
{
  logging_tx_scheduler_t sched;
  (void)logging_tx_scheduler_init(&sched, 100u, 200u);

  test_tx_ctx_t state = {false, true, 150u, 8u};
  logging_tx_queue_if_t iface = {
    .ctx = &state,
    .has_pdo = test_tx_has_pdo,
    .has_log = test_tx_has_log,
    .pop_pdo = test_tx_pop_pdo,
    .pop_log = test_tx_pop_log
  };

  logging_tx_scheduler_on_tick(&sched);

  uint8_t out[256] = {0u};
  uint32_t out_len = 0u;
  logging_tx_class_t out_class = LOGGING_TX_CLASS_NONE;
  logging_result_t res = logging_tx_scheduler_next(&sched, &iface, out, sizeof(out), &out_class, &out_len);
  test_expect_true(ctx, res == LOGGING_RESULT_NOT_READY, "tx log over budget");
  test_expect_u32(ctx, out_len, 0u, "tx log len 0");

  state.pdo_ready = true;
  res = logging_tx_scheduler_next(&sched, &iface, out, sizeof(out), &out_class, &out_len);
  test_expect_true(ctx, res == LOGGING_RESULT_OK, "tx pdo ok");
  test_expect_true(ctx, out_class == LOGGING_TX_CLASS_PDO, "tx pdo class");
  test_expect_u32(ctx, out_len, state.pdo_len, "tx pdo len");

  logging_tx_scheduler_on_tick(&sched);
  state.pdo_ready = false;
  res = logging_tx_scheduler_next(&sched, &iface, out, sizeof(out), &out_class, &out_len);
  test_expect_true(ctx, res == LOGGING_RESULT_OK, "tx log ok");
  test_expect_true(ctx, out_class == LOGGING_TX_CLASS_LOG, "tx log class");
  test_expect_u32(ctx, out_len, state.log_min_len, "tx log len");
}

/**
 * @brief Тест: запрет частичной записи Record при нехватке места.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_core_write_no_partial(test_ctx_t *ctx)
{
  uint8_t buffer[32] = {0u};
  logging_core_t core;
  logging_session_cfg_t cfg = {0u, (uint32_t)(LOGGING_RECORD_HEADER_SIZE + 1u)};
  logging_timestamp_t ts = {0u};
  ts.pwm_period_count = 1u;
  ts.pwm_subtick = 0u;
  ts.domain_id = 0u;

  test_expect_true(ctx,
                   logging_core_init(&core, buffer, sizeof(buffer), 1u) == LOGGING_RESULT_OK,
                   "core init");
  test_expect_true(ctx,
                   logging_core_arm(&core, &cfg, 1u) == LOGGING_RESULT_OK,
                   "core arm");
  test_expect_true(ctx,
                   logging_core_trigger(&core) == LOGGING_RESULT_OK,
                   "core trigger");

  logging_record_header_t hdr;
  logging_record_header_init(&hdr,
                             LOGGING_RECORD_TYPE_CTRL,
                             1u,
                             2u,
                             &ts,
                             0u,
                             LOGGING_RECORD_FLAG_NONE);

  const uint8_t payload[2] = {0x11u, 0x22u};
  const logging_result_t res = logging_core_write_record(&core, &hdr, payload);
  test_expect_true(ctx, res == LOGGING_RESULT_NO_SPACE, "core write no space");

  logging_session_status_t status = {0u};
  (void)logging_core_get_status(&core, &status);
  test_expect_u32(ctx, status.window_len, 0u, "core window unchanged");
}

/**
 * @brief Тест: успешная запись Record при достаточном месте.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_core_write_success(test_ctx_t *ctx)
{
  uint8_t buffer[64] = {0u};
  logging_core_t core;
  const uint32_t payload_len = 4u;
  logging_session_cfg_t cfg = {0u, (uint32_t)(LOGGING_RECORD_HEADER_SIZE + payload_len)};
  logging_timestamp_t ts = {0u};
  ts.pwm_period_count = 1u;
  ts.pwm_subtick = 0u;
  ts.domain_id = 0u;

  test_expect_true(ctx,
                   logging_core_init(&core, buffer, sizeof(buffer), 1u) == LOGGING_RESULT_OK,
                   "core init ok");
  test_expect_true(ctx,
                   logging_core_arm(&core, &cfg, 1u) == LOGGING_RESULT_OK,
                   "core arm ok");
  test_expect_true(ctx,
                   logging_core_trigger(&core) == LOGGING_RESULT_OK,
                   "core trigger ok");

  logging_record_header_t hdr;
  logging_record_header_init(&hdr,
                             LOGGING_RECORD_TYPE_CTRL,
                             1u,
                             (uint16_t)payload_len,
                             &ts,
                             0u,
                             LOGGING_RECORD_FLAG_NONE);

  const uint8_t payload[4] = {0x11u, 0x22u, 0x33u, 0x44u};
  const logging_result_t res = logging_core_write_record(&core, &hdr, payload);
  test_expect_true(ctx, res == LOGGING_RESULT_OK, "core write ok");

  logging_session_status_t status = {0u};
  (void)logging_core_get_status(&core, &status);
  test_expect_u32(ctx,
                  status.window_len,
                  (uint32_t)(LOGGING_RECORD_HEADER_SIZE + payload_len),
                  "core window len");
}

/**
 * @brief Тест: последовательность seq при auto-write.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_core_seq_unique(test_ctx_t *ctx)
{
  uint8_t buffer[128] = {0u};
  logging_core_t core;
  logging_session_cfg_t cfg = {0u, (uint32_t)(2u * LOGGING_RECORD_HEADER_SIZE)};
  logging_timestamp_t ts = {0u};
  ts.pwm_period_count = 1u;
  ts.pwm_subtick = 0u;
  ts.domain_id = 0u;

  test_expect_true(ctx,
                   logging_core_init(&core, buffer, sizeof(buffer), 1u) == LOGGING_RESULT_OK,
                   "seq init");
  test_expect_true(ctx,
                   logging_core_arm(&core, &cfg, 1u) == LOGGING_RESULT_OK,
                   "seq arm");
  test_expect_true(ctx,
                   logging_core_trigger(&core) == LOGGING_RESULT_OK,
                   "seq trigger");

  const logging_result_t res1 = logging_core_write_record_auto(&core,
                                                               LOGGING_RECORD_TYPE_CTRL,
                                                               1u,
                                                               NULL,
                                                               0u,
                                                               &ts,
                                                               LOGGING_RECORD_FLAG_NONE);
  const logging_result_t res2 = logging_core_write_record_auto(&core,
                                                               LOGGING_RECORD_TYPE_CTRL,
                                                               1u,
                                                               NULL,
                                                               0u,
                                                               &ts,
                                                               LOGGING_RECORD_FLAG_NONE);
  test_expect_true(ctx, res1 == LOGGING_RESULT_OK, "seq write 1");
  test_expect_true(ctx, res2 == LOGGING_RESULT_OK, "seq write 2");

  (void)logging_core_stop(&core);

  uint8_t out[2u * LOGGING_RECORD_HEADER_SIZE] = {0u};
  uint32_t out_len = 0u;
  test_expect_true(ctx,
                   logging_core_read_chunk(&core, 0u, out, sizeof(out), &out_len) == LOGGING_RESULT_OK,
                   "seq read");
  test_expect_u32(ctx, out_len, (uint32_t)(2u * LOGGING_RECORD_HEADER_SIZE), "seq len");

  logging_record_header_t h1 = {0u};
  logging_record_header_t h2 = {0u};
  (void)logging_record_unpack_header(&h1, &out[0], LOGGING_RECORD_HEADER_SIZE);
  (void)logging_record_unpack_header(&h2, &out[LOGGING_RECORD_HEADER_SIZE], LOGGING_RECORD_HEADER_SIZE);
  test_expect_u32(ctx, h1.seq, 0u, "seq first");
  test_expect_u32(ctx, h2.seq, 1u, "seq second");
}

/**
 * @brief Тест: последовательность seq на серии записей.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_core_seq_stress(test_ctx_t *ctx)
{
  const uint32_t record_count = 32u;
  const uint32_t buffer_len = record_count * LOGGING_RECORD_HEADER_SIZE;
  uint8_t buffer[32u * LOGGING_RECORD_HEADER_SIZE] = {0u};
  logging_core_t core;
  logging_session_cfg_t cfg = {0u, buffer_len};
  logging_timestamp_t ts = {0u};
  ts.pwm_period_count = 1u;
  ts.pwm_subtick = 0u;
  ts.domain_id = 0u;

  test_expect_true(ctx,
                   logging_core_init(&core, buffer, sizeof(buffer), 1u) == LOGGING_RESULT_OK,
                   "seq stress init");
  test_expect_true(ctx,
                   logging_core_arm(&core, &cfg, 1u) == LOGGING_RESULT_OK,
                   "seq stress arm");
  test_expect_true(ctx,
                   logging_core_trigger(&core) == LOGGING_RESULT_OK,
                   "seq stress trigger");

  for (uint32_t i = 0u; i < record_count; ++i)
  {
    const logging_result_t res = logging_core_write_record_auto(&core,
                                                                LOGGING_RECORD_TYPE_CTRL,
                                                                1u,
                                                                NULL,
                                                                0u,
                                                                &ts,
                                                                LOGGING_RECORD_FLAG_NONE);
    test_expect_true(ctx, res == LOGGING_RESULT_OK, "seq stress write");
  }

  (void)logging_core_stop(&core);

  uint8_t out[32u * LOGGING_RECORD_HEADER_SIZE] = {0u};
  uint32_t out_len = 0u;
  test_expect_true(ctx,
                   logging_core_read_chunk(&core, 0u, out, sizeof(out), &out_len) == LOGGING_RESULT_OK,
                   "seq stress read");
  test_expect_u32(ctx, out_len, buffer_len, "seq stress len");

  for (uint32_t i = 0u; i < record_count; ++i)
  {
    logging_record_header_t hdr = {0u};
    const uint32_t offset = i * LOGGING_RECORD_HEADER_SIZE;
    (void)logging_record_unpack_header(&hdr, &out[offset], LOGGING_RECORD_HEADER_SIZE);
    test_expect_u32(ctx, hdr.seq, i, "seq stress value");
  }
}

/**
 * @brief Точка входа для unit-тестов.
 * @return 0 при успехе.
 */
int main(void)
{
  test_case_t tests[] = {
    {"record_pack_roundtrip", test_record_pack_roundtrip},
    {"spsc_basic", test_spsc_basic},
    {"packer_profile", test_packer_profile},
    {"capture_window", test_capture_window},
    {"tx_scheduler_basic", test_tx_scheduler_basic},
    {"core_write_no_partial", test_core_write_no_partial},
    {"core_write_success", test_core_write_success},
    {"core_seq_unique", test_core_seq_unique},
    {"core_seq_stress", test_core_seq_stress}
  };
  test_ctx_t ctx = {0};
  for (size_t i = 0u; i < (sizeof(tests) / sizeof(tests[0])); ++i)
  {
    (void)printf("RUN %s\n", tests[i].name);
    tests[i].fn(&ctx);
  }
  if (ctx.failed != 0)
  {
    (void)printf("FAILED: %d tests\n", ctx.failed);
    return 1;
  }
  (void)printf("OK\n");
  return 0;
}
