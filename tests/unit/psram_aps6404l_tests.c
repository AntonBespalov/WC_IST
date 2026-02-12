#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "psram_aps6404l.h"

#define MOCK_PSRAM_SIZE_BYTES (1024u)

typedef struct {
  int failed;
} test_ctx_t;

typedef struct {
  uint8_t memory[MOCK_PSRAM_SIZE_BYTES];
  bool fail_next_timeout;
  bool fail_next_bus;
  bool fail_always_timeout;
  bool fail_always_bus;
  uint32_t read_calls;
  uint32_t write_calls;
  uint32_t timing_epoch;
  bool is_idle;
  psram_ctx_t *driver_for_reentry;
  bool reentry_test_enable;
  uint32_t reentry_task_id;
  psram_error_t reentry_result;
} mock_qspi_t;

static float test_abs_f(float value)
{
  return (value < 0.0f) ? -value : value;
}

static void test_expect_true(test_ctx_t *ctx, bool condition, const char *message)
{
  if (!condition)
  {
    ctx->failed += 1;
    (void)printf("FAIL: %s\n", message);
  }
}

static void test_expect_u32(test_ctx_t *ctx, uint32_t actual, uint32_t expected, const char *message)
{
  if (actual != expected)
  {
    ctx->failed += 1;
    (void)printf("FAIL: %s (actual=%u expected=%u)\n", message, (unsigned)actual, (unsigned)expected);
  }
}

static qspi_port_status_t mock_qspi_init(void *low_level_ctx)
{
  mock_qspi_t *mock = (mock_qspi_t *)low_level_ctx;
  mock->fail_next_timeout = false;
  mock->fail_next_bus = false;
  return QSPI_PORT_OK;
}

/**
 * @brief Получить текущую версию timing-конфигурации mock QSPI.
 * @param low_level_ctx Контекст mock.
 * @return Версия timing-конфигурации QSPI, [счётчик].
 */
static uint32_t mock_qspi_get_timing_epoch(void *low_level_ctx)
{
  mock_qspi_t *mock = (mock_qspi_t *)low_level_ctx;
  return mock->timing_epoch;
}

/**
 * @brief Проверить idle-состояние mock QSPI.
 * @param low_level_ctx Контекст mock.
 * @retval true Mock-порт idle.
 * @retval false Mock-порт занят.
 */
static bool mock_qspi_is_idle(void *low_level_ctx)
{
  mock_qspi_t *mock = (mock_qspi_t *)low_level_ctx;
  return mock->is_idle;
}

static qspi_port_status_t mock_qspi_read(void *low_level_ctx,
                                         uint32_t address_start,
                                         uint8_t *buffer_dst,
                                         size_t length_bytes)
{
  mock_qspi_t *mock = (mock_qspi_t *)low_level_ctx;
  mock->read_calls += 1u;

  if (mock->fail_always_timeout || mock->fail_next_timeout)
  {
    mock->fail_next_timeout = false;
    return QSPI_PORT_TIMEOUT;
  }

  if (mock->fail_always_bus || mock->fail_next_bus)
  {
    mock->fail_next_bus = false;
    return QSPI_PORT_BUS;
  }

  if (((uint64_t)address_start + (uint64_t)length_bytes) > MOCK_PSRAM_SIZE_BYTES)
  {
    return QSPI_PORT_BUS;
  }

  (void)memcpy(buffer_dst, &mock->memory[address_start], length_bytes);
  return QSPI_PORT_OK;
}

static qspi_port_status_t mock_qspi_write(void *low_level_ctx,
                                          uint32_t address_start,
                                          const uint8_t *buffer_src,
                                          size_t length_bytes)
{
  mock_qspi_t *mock = (mock_qspi_t *)low_level_ctx;
  mock->write_calls += 1u;

  if (mock->reentry_test_enable && (mock->driver_for_reentry != NULL))
  {
    uint8_t readback = 0u;
    mock->reentry_result = psram_read(mock->driver_for_reentry, mock->reentry_task_id, 0u, &readback, 1u);
    mock->reentry_test_enable = false;
  }

  if (mock->fail_always_timeout || mock->fail_next_timeout)
  {
    mock->fail_next_timeout = false;
    return QSPI_PORT_TIMEOUT;
  }

  if (mock->fail_always_bus || mock->fail_next_bus)
  {
    mock->fail_next_bus = false;
    return QSPI_PORT_BUS;
  }

  if (((uint64_t)address_start + (uint64_t)length_bytes) > MOCK_PSRAM_SIZE_BYTES)
  {
    return QSPI_PORT_BUS;
  }

  (void)memcpy(&mock->memory[address_start], buffer_src, length_bytes);
  return QSPI_PORT_OK;
}

static void test_make_driver(psram_ctx_t *driver, mock_qspi_t *mock)
{
  mock->timing_epoch = 1u;
  mock->is_idle = true;

  const psram_cfg_t cfg = {
    .memory_size_bytes = MOCK_PSRAM_SIZE_BYTES,
    .max_chunk_bytes = 16u,
    .max_retries_per_chunk = 2u,
    .degrade_error_threshold = 2u
  };
  const qspi_port_api_t port = {
    .low_level_ctx = mock,
    .init = mock_qspi_init,
    .read = mock_qspi_read,
    .write = mock_qspi_write,
    .get_timing_epoch = mock_qspi_get_timing_epoch,
    .is_idle = mock_qspi_is_idle,
    .tcem_safe_max_chunk_bytes = 16u,
  };

  const psram_error_t init_result = psram_init(driver, &cfg, &port);
  if (init_result != PSRAM_ERR_OK)
  {
    (void)printf("driver init failed in test fixture\n");
  }
}


/**
 * @brief Тест: init отклоняет chunk больше tCEM-safe лимита BSP.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_init_rejects_chunk_above_tcem_limit(test_ctx_t *ctx)
{
  psram_ctx_t driver;
  mock_qspi_t mock = {0};

  const psram_cfg_t cfg = {
    .memory_size_bytes = MOCK_PSRAM_SIZE_BYTES,
    .max_chunk_bytes = 32u,
    .max_retries_per_chunk = 2u,
    .degrade_error_threshold = 2u
  };
  const qspi_port_api_t port = {
    .low_level_ctx = &mock,
    .init = mock_qspi_init,
    .read = mock_qspi_read,
    .write = mock_qspi_write,
    .get_timing_epoch = mock_qspi_get_timing_epoch,
    .is_idle = mock_qspi_is_idle,
    .tcem_safe_max_chunk_bytes = 16u,
  };

  test_expect_true(ctx,
                   psram_init(&driver, &cfg, &port) == PSRAM_ERR_PARAM,
                   "init should reject chunk above tCEM-safe limit");
}

static void test_bounds_and_params(test_ctx_t *ctx)
{
  psram_ctx_t driver;
  mock_qspi_t mock = {0};
  test_make_driver(&driver, &mock);

  uint8_t buffer[8] = {0u};

  test_expect_true(ctx,
                   psram_read(&driver, 1u, MOCK_PSRAM_SIZE_BYTES - 4u, buffer, 8u) == PSRAM_ERR_PARAM,
                   "read out of bounds should fail with PARAM");
  test_expect_true(ctx,
                   psram_write(&driver, 1u, MOCK_PSRAM_SIZE_BYTES - 4u, buffer, 8u) == PSRAM_ERR_PARAM,
                   "write out of bounds should fail with PARAM");
  test_expect_true(ctx,
                   psram_read(&driver, 1u, 0u, NULL, 8u) == PSRAM_ERR_PARAM,
                   "read null buffer should fail with PARAM");
  test_expect_true(ctx,
                   psram_write(&driver, 1u, 0u, NULL, 8u) == PSRAM_ERR_PARAM,
                   "write null buffer should fail with PARAM");
}

static void test_write_read_and_chunking(test_ctx_t *ctx)
{
  psram_ctx_t driver;
  mock_qspi_t mock = {0};
  test_make_driver(&driver, &mock);

  uint8_t write_data[40];
  uint8_t read_data[40];

  for (uint32_t i = 0u; i < 40u; ++i)
  {
    write_data[i] = (uint8_t)(i + 1u);
  }

  const psram_error_t write_result = psram_write(&driver, 10u, 100u, write_data, sizeof(write_data));
  const psram_error_t read_result = psram_read(&driver, 10u, 100u, read_data, sizeof(read_data));

  test_expect_true(ctx, write_result == PSRAM_ERR_OK, "write should succeed");
  test_expect_true(ctx, read_result == PSRAM_ERR_OK, "read should succeed");
  test_expect_true(ctx, memcmp(write_data, read_data, sizeof(write_data)) == 0, "readback should match write data");
  test_expect_u32(ctx, mock.write_calls, 3u, "write should be split into 3 chunks");
  test_expect_u32(ctx, mock.read_calls, 3u, "read should be split into 3 chunks");
}

/**
 * @brief Тест: после единичной ошибки состояние возвращается в READY.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_state_recovers_to_ready_after_single_error(test_ctx_t *ctx)
{
  psram_ctx_t driver;
  mock_qspi_t mock = {0};
  test_make_driver(&driver, &mock);

  uint8_t buffer[4] = {1u, 2u, 3u, 4u};
  mock.fail_always_timeout = true;

  test_expect_true(ctx,
                   psram_write(&driver, 10u, 0u, buffer, sizeof(buffer)) == PSRAM_ERR_TIMEOUT,
                   "single timeout should be reported");

  psram_status_t status = {0};
  (void)psram_get_status(&driver, &status);
  test_expect_true(ctx, status.state == PSRAM_STATE_READY, "state should return to READY after single error");
  test_expect_true(ctx, status.consecutive_errors == 1u, "consecutive_errors should be incremented");
}

/**
 * @brief Тест: смена timing_epoch переводит драйвер в DEGRADED и блокирует операции.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_timing_epoch_mismatch_degrades_driver(test_ctx_t *ctx)
{
  psram_ctx_t driver;
  mock_qspi_t mock = {0};
  test_make_driver(&driver, &mock);

  mock.timing_epoch += 1u;

  uint8_t buffer[4] = {0u};
  const psram_error_t read_result = psram_read(&driver, 5u, 0u, buffer, sizeof(buffer));
  test_expect_true(ctx, read_result == PSRAM_ERR_TIMING_CHANGED, "timing mismatch should return TIMING_CHANGED");

  psram_status_t status = {0};
  (void)psram_get_status(&driver, &status);
  test_expect_true(ctx, status.state == PSRAM_STATE_DEGRADED, "timing mismatch should force DEGRADED");
  test_expect_true(ctx,
                   status.last_not_ready_reason == PSRAM_NOT_READY_REASON_TIMING_CHANGED,
                   "timing mismatch should set not-ready reason to TIMING_CHANGED");
}

static void test_degraded_after_repeated_errors(test_ctx_t *ctx)
{
  psram_ctx_t driver;
  mock_qspi_t mock = {0};
  test_make_driver(&driver, &mock);

  uint8_t buffer[4] = {1u, 2u, 3u, 4u};
  mock.fail_always_timeout = true;
  test_expect_true(ctx,
                   psram_write(&driver, 1u, 0u, buffer, sizeof(buffer)) == PSRAM_ERR_TIMEOUT,
                   "first timeout should be reported");

  mock.fail_always_timeout = false;
  mock.fail_always_bus = true;
  test_expect_true(ctx,
                   psram_write(&driver, 1u, 0u, buffer, sizeof(buffer)) == PSRAM_ERR_BUS,
                   "second error should be reported");

  psram_status_t status = {0};
  (void)psram_get_status(&driver, &status);
  test_expect_true(ctx, status.state == PSRAM_STATE_DEGRADED, "driver should enter DEGRADED after threshold");
  test_expect_true(ctx,
                   psram_write(&driver, 1u, 0u, buffer, sizeof(buffer)) == PSRAM_ERR_NOT_READY,
                   "operations in DEGRADED should return NOT_READY");
}

static void test_lock_serialization_conflict(test_ctx_t *ctx)
{
  psram_ctx_t driver;
  mock_qspi_t mock = {0};
  test_make_driver(&driver, &mock);

  uint8_t write_data[4] = {7u, 8u, 9u, 10u};
  mock.driver_for_reentry = &driver;
  mock.reentry_test_enable = true;
  mock.reentry_result = PSRAM_ERR_OK;
  mock.reentry_task_id = 200u;

  const psram_error_t write_result = psram_write(&driver, 100u, 0u, write_data, sizeof(write_data));
  test_expect_true(ctx, write_result == PSRAM_ERR_OK, "primary write should succeed");
  test_expect_true(ctx,
                   mock.reentry_result == PSRAM_ERR_LOCKED,
                   "reentrant access from another task should return LOCKED");
}

/**
 * @brief Тест: повторный вход тем же task-id блокируется (нерекурсивный lock).
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_lock_serialization_same_task_conflict(test_ctx_t *ctx)
{
  psram_ctx_t driver;
  mock_qspi_t mock = {0};
  test_make_driver(&driver, &mock);

  uint8_t write_data[4] = {7u, 8u, 9u, 10u};
  mock.driver_for_reentry = &driver;
  mock.reentry_test_enable = true;
  mock.reentry_task_id = 100u;
  mock.reentry_result = PSRAM_ERR_OK;

  const psram_error_t write_result = psram_write(&driver, 100u, 0u, write_data, sizeof(write_data));
  test_expect_true(ctx, write_result == PSRAM_ERR_OK, "primary write should succeed");
  test_expect_true(ctx,
                   mock.reentry_result == PSRAM_ERR_LOCKED,
                   "reentrant access from same task should return LOCKED");
}

/**
 * @brief Тест: recover с новым timing_epoch восстанавливает READY.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_recover_after_timing_epoch_change(test_ctx_t *ctx)
{
  psram_ctx_t driver;
  mock_qspi_t mock = {0};
  test_make_driver(&driver, &mock);

  mock.timing_epoch += 1u;
  uint8_t buffer[2] = {1u, 2u};
  test_expect_true(ctx,
                   psram_write(&driver, 11u, 0u, buffer, sizeof(buffer)) == PSRAM_ERR_TIMING_CHANGED,
                   "write should fail with TIMING_CHANGED before recover");

  test_expect_true(ctx,
                   psram_recover(&driver, 11u) == PSRAM_ERR_OK,
                   "recover should succeed after timing change");

  psram_status_t status = {0};
  (void)psram_get_status(&driver, &status);
  test_expect_true(ctx, status.state == PSRAM_STATE_READY, "state should be READY after recover");

  test_expect_true(ctx,
                   psram_write(&driver, 11u, 3u, buffer, sizeof(buffer)) == PSRAM_ERR_OK,
                   "write should succeed after recover");
  uint8_t readback[2] = {0u};
  test_expect_true(ctx,
                   psram_read(&driver, 11u, 3u, readback, sizeof(readback)) == PSRAM_ERR_OK,
                   "read should succeed after recover");
  test_expect_true(ctx,
                   memcmp(buffer, readback, sizeof(buffer)) == 0,
                   "readback should match after recover");
}

/**
 * @brief Тест: recover завершаетcя BUS, если QSPI-порт не idle после init.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_recover_fails_when_port_not_idle(test_ctx_t *ctx)
{
  psram_ctx_t driver;
  mock_qspi_t mock = {0};
  test_make_driver(&driver, &mock);

  mock.timing_epoch += 1u;
  uint8_t buffer[2] = {3u, 4u};
  (void)psram_write(&driver, 7u, 0u, buffer, sizeof(buffer));

  mock.is_idle = false;
  test_expect_true(ctx,
                   psram_recover(&driver, 7u) == PSRAM_ERR_BUS,
                   "recover should fail with BUS if port is not idle");

  psram_status_t status = {0};
  (void)psram_get_status(&driver, &status);
  test_expect_true(ctx, status.state == PSRAM_STATE_FAULT, "state should be FAULT when recover sees non-idle port");
}

/**
 * @brief Тест: self-test блокируется при mismatch timing_epoch.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_self_test_rejects_timing_mismatch(test_ctx_t *ctx)
{
  psram_ctx_t driver;
  mock_qspi_t mock = {0};
  test_make_driver(&driver, &mock);

  mock.timing_epoch += 1u;
  test_expect_true(ctx,
                   psram_self_test(&driver, 3u) == PSRAM_ERR_TIMING_CHANGED,
                   "self-test should fail with TIMING_CHANGED when epoch mismatches");
}

static void test_self_test_ok(test_ctx_t *ctx)
{
  psram_ctx_t driver;
  mock_qspi_t mock = {0};
  test_make_driver(&driver, &mock);

  const psram_error_t self_test_result = psram_self_test(&driver, 7u);
  test_expect_true(ctx, self_test_result == PSRAM_ERR_OK, "self-test should succeed on healthy memory");
}

int main(void)
{
  const struct {
    const char *name;
    void (*fn)(test_ctx_t *ctx);
  } cases[] = {
    {"init_rejects_chunk_above_tcem_limit", test_init_rejects_chunk_above_tcem_limit},
    {"bounds_and_params", test_bounds_and_params},
    {"write_read_and_chunking", test_write_read_and_chunking},
    {"state_recovers_to_ready_after_single_error", test_state_recovers_to_ready_after_single_error},
    {"timing_epoch_mismatch_degrades_driver", test_timing_epoch_mismatch_degrades_driver},
    {"degraded_after_repeated_errors", test_degraded_after_repeated_errors},
    {"lock_serialization_conflict", test_lock_serialization_conflict},
    {"lock_serialization_same_task_conflict", test_lock_serialization_same_task_conflict},
    {"recover_after_timing_epoch_change", test_recover_after_timing_epoch_change},
    {"recover_fails_when_port_not_idle", test_recover_fails_when_port_not_idle},
    {"self_test_ok", test_self_test_ok},
    {"self_test_rejects_timing_mismatch", test_self_test_rejects_timing_mismatch}
  };

  test_ctx_t ctx = {0};

  for (size_t i = 0u; i < (sizeof(cases) / sizeof(cases[0])); ++i)
  {
    (void)printf("RUN: %s\n", cases[i].name);
    cases[i].fn(&ctx);
  }

  if (ctx.failed != 0)
  {
    (void)printf("FAILED: %d checks\n", ctx.failed);
    return 1;
  }

  (void)printf("OK: all psram tests passed\n");
  return 0;
}
