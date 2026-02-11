#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "control_core.h"

/**
 * @brief Контекст простого тестового раннера.
 */
typedef struct {
  int failed; /**< Количество проваленных проверок, [шт]. */
} test_ctx_t;

/**
 * @brief Получить модуль числа.
 * @param value Входное значение, [отн. ед.].
 * @return Модуль значения, [отн. ед.].
 */
static float test_abs_f(float value)
{
  return (value < 0.0f) ? -value : value;
}

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
 * @brief Проверить близость чисел с допуском.
 * @param ctx Контекст тестов.
 * @param actual Фактическое значение, [отн. ед.].
 * @param expected Ожидаемое значение, [отн. ед.].
 * @param tol Допуск, [отн. ед.].
 * @param message Сообщение об ошибке.
 * @return None.
 */
static void test_expect_close(test_ctx_t *ctx,
                              float actual,
                              float expected,
                              float tol,
                              const char *message)
{
  const float diff = test_abs_f(actual - expected); /* [отн. ед.] */
  if (diff > tol)
  {
    ctx->failed += 1;
    (void)printf("FAIL: %s (actual=%.6f expected=%.6f tol=%.6f)\n", message, actual, expected, tol);
  }
}

/**
 * @brief Тест: запрет управления сбрасывает интегратор.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_disable_resets_integrator(test_ctx_t *ctx)
{
  /* Единицы полей см. control_cfg_t. */
  const control_cfg_t cfg = {
    .kp = 0.0f, /* [отн. ед./A] */
    .ki = 1.0f, /* [отн. ед./(A*с)] */
    .dt = 1.0f, /* [с] */
    .u_min = -100.0f, /* [отн. ед.] */
    .u_max = 100.0f,  /* [отн. ед.] */
    .i_ref_min = 0.0f, /* [A] */
    .i_ref_max = 200.0f, /* [A] */
    .di_dt_max = 1000.0f, /* [A/с] */
    .integrator_policy = CONTROL_INTEGRATOR_RESET
  };

  control_ctx_t ctrl;
  control_init(&ctrl, &cfg);

  /* Единицы полей см. control_cmd_t. */
  const control_cmd_t cmd = {
    .i_ref_cmd = 10.0f, /* [A] */
    .enable_cmd = true,
    .cmd_valid = true
  };
  control_slow_step(&ctrl, &cmd);

  /* Единицы полей см. control_meas_t. */
  const control_meas_t meas = {
    .i_meas = 0.0f, /* [A] */
    .u_meas = 0.0f, /* [В] */
    .udc = 0.0f, /* [В] */
    .meas_valid = true
  };

  control_out_t out = {0};

  control_fast_step(&ctrl, &meas, true, &out);
  test_expect_true(ctx, ctrl.state.integrator != 0.0f, "integrator should update when enabled");

  control_fast_step(&ctrl, &meas, false, &out);
  test_expect_close(ctx, out.u, 0.0f, 1e-6f, "u should be zero when disabled");
  test_expect_true(ctx, !out.enable_request, "enable_request should be false when disabled");
  test_expect_true(ctx, (out.flags & CONTROL_FLAG_DISABLED) != 0u, "disabled flag should be set");
  test_expect_close(ctx, ctrl.state.integrator, 0.0f, 1e-6f, "integrator should reset when disabled");
  test_expect_close(ctx, ctrl.state.i_ref_used, 0.0f, 1e-6f, "i_ref_used should reset when disabled");
}

/**
 * @brief Тест: невалидные измерения блокируют управление.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_meas_invalid_blocks_control(test_ctx_t *ctx)
{
  /* Единицы полей см. control_cfg_t. */
  const control_cfg_t cfg = {
    .kp = 1.0f,
    .ki = 0.0f,
    .dt = 1.0f,
    .u_min = -10.0f,
    .u_max = 10.0f,
    .i_ref_min = 0.0f,
    .i_ref_max = 100.0f,
    .di_dt_max = 0.0f,
    .integrator_policy = CONTROL_INTEGRATOR_RESET
  };

  control_ctx_t ctrl;
  control_init(&ctrl, &cfg);

  /* Единицы полей см. control_cmd_t. */
  const control_cmd_t cmd = {
    .i_ref_cmd = 5.0f,
    .enable_cmd = true,
    .cmd_valid = true
  };
  control_slow_step(&ctrl, &cmd);

  /* Единицы полей см. control_meas_t. */
  const control_meas_t meas = {
    .i_meas = 1.0f,
    .u_meas = 0.0f,
    .udc = 0.0f,
    .meas_valid = false
  };

  control_out_t out = {0};

  control_fast_step(&ctrl, &meas, true, &out);
  test_expect_close(ctx, out.u, 0.0f, 1e-6f, "u should be zero when meas invalid");
  test_expect_true(ctx, (out.flags & CONTROL_FLAG_MEAS_INVALID) != 0u, "meas invalid flag should be set");
  test_expect_true(ctx, !out.enable_request, "enable_request should be false when meas invalid");
}

/**
 * @brief Тест: невалидная команда блокирует управление.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_cmd_invalid_blocks_control(test_ctx_t *ctx)
{
  const control_cfg_t cfg = {
    .kp = 1.0f,
    .ki = 0.0f,
    .dt = 1.0f,
    .u_min = -10.0f,
    .u_max = 10.0f,
    .i_ref_min = 0.0f,
    .i_ref_max = 100.0f,
    .di_dt_max = 0.0f,
    .integrator_policy = CONTROL_INTEGRATOR_RESET
  };

  control_ctx_t ctrl;
  control_init(&ctrl, &cfg);

  const control_cmd_t cmd = {
    .i_ref_cmd = 5.0f,
    .enable_cmd = true,
    .cmd_valid = false
  };
  control_slow_step(&ctrl, &cmd);

  const control_meas_t meas = {
    .i_meas = 1.0f,
    .u_meas = 0.0f,
    .udc = 0.0f,
    .meas_valid = true
  };

  control_out_t out = {0};

  control_fast_step(&ctrl, &meas, true, &out);
  test_expect_close(ctx, out.u, 0.0f, 1e-6f, "u should be zero when cmd invalid");
  test_expect_true(ctx, (out.flags & CONTROL_FLAG_CMD_INVALID) != 0u, "cmd invalid flag should be set");
  test_expect_true(ctx, !out.enable_request, "enable_request should be false when cmd invalid");
}

/**
 * @brief Тест: невалидные числа блокируют управление.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_num_invalid_blocks_control(test_ctx_t *ctx)
{
  const control_cfg_t cfg = {
    .kp = 1.0f,
    .ki = 0.0f,
    .dt = 1.0f,
    .u_min = -10.0f,
    .u_max = 10.0f,
    .i_ref_min = 0.0f,
    .i_ref_max = 100.0f,
    .di_dt_max = 0.0f,
    .integrator_policy = CONTROL_INTEGRATOR_RESET
  };

  control_ctx_t ctrl;
  control_init(&ctrl, &cfg);

  const control_cmd_t cmd = {
    .i_ref_cmd = NAN,
    .enable_cmd = true,
    .cmd_valid = true
  };
  control_slow_step(&ctrl, &cmd);

  const control_meas_t meas = {
    .i_meas = 1.0f,
    .u_meas = 0.0f,
    .udc = 0.0f,
    .meas_valid = true
  };

  control_out_t out = {0};

  control_fast_step(&ctrl, &meas, true, &out);
  test_expect_close(ctx, out.u, 0.0f, 1e-6f, "u should be zero when numbers invalid");
  test_expect_true(ctx, (out.flags & CONTROL_FLAG_NUM_INVALID) != 0u, "num invalid flag should be set");
  test_expect_true(ctx, !out.enable_request, "enable_request should be false when numbers invalid");
}

/**
 * @brief Тест: невалидная конфигурация блокирует управление.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_cfg_invalid_blocks_control(test_ctx_t *ctx)
{
  const control_cfg_t cfg = {
    .kp = 1.0f,
    .ki = 0.0f,
    .dt = 1.0f,
    .u_min = 5.0f,
    .u_max = 1.0f,
    .i_ref_min = 0.0f,
    .i_ref_max = 100.0f,
    .di_dt_max = 0.0f,
    .integrator_policy = CONTROL_INTEGRATOR_RESET
  };

  control_ctx_t ctrl;
  control_init(&ctrl, &cfg);

  const control_cmd_t cmd = {
    .i_ref_cmd = 5.0f,
    .enable_cmd = true,
    .cmd_valid = true
  };
  control_slow_step(&ctrl, &cmd);

  const control_meas_t meas = {
    .i_meas = 1.0f,
    .u_meas = 0.0f,
    .udc = 0.0f,
    .meas_valid = true
  };

  control_out_t out = {0};

  control_fast_step(&ctrl, &meas, true, &out);
  test_expect_close(ctx, out.u, 0.0f, 1e-6f, "u should be zero when cfg invalid");
  test_expect_true(ctx, (out.flags & CONTROL_FLAG_CFG_INVALID) != 0u, "cfg invalid flag should be set");
  test_expect_true(ctx, !out.enable_request, "enable_request should be false when cfg invalid");
}

/**
 * @brief Тест: интегратор ограничивается по u_min/u_max.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_integrator_clamp(test_ctx_t *ctx)
{
  const control_cfg_t cfg = {
    .kp = 0.0f,
    .ki = 10.0f,
    .dt = 1.0f,
    .u_min = 0.0f,
    .u_max = 1.0f,
    .i_ref_min = 0.0f,
    .i_ref_max = 100.0f,
    .di_dt_max = 0.0f,
    .integrator_policy = CONTROL_INTEGRATOR_RESET
  };

  control_ctx_t ctrl;
  control_init(&ctrl, &cfg);

  const control_cmd_t cmd = {
    .i_ref_cmd = 10.0f,
    .enable_cmd = true,
    .cmd_valid = true
  };
  control_slow_step(&ctrl, &cmd);

  const control_meas_t meas = {
    .i_meas = 0.0f,
    .u_meas = 0.0f,
    .udc = 0.0f,
    .meas_valid = true
  };

  control_out_t out = {0};

  control_fast_step(&ctrl, &meas, true, &out);
  test_expect_true(ctx, ctrl.state.integrator <= cfg.u_max, "integrator should clamp to u_max");
}

/**
 * @brief Тест: slew-rate ограничивает скорость изменения уставки.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_slew_rate_limit(test_ctx_t *ctx)
{
  /* Единицы полей см. control_cfg_t. */
  const control_cfg_t cfg = {
    .kp = 0.0f,
    .ki = 0.0f,
    .dt = 0.001f, /* [с] */
    .u_min = -1.0f,
    .u_max = 1.0f,
    .i_ref_min = 0.0f,
    .i_ref_max = 100.0f,
    .di_dt_max = 1000.0f, /* [A/с] => 1 A за шаг */
    .integrator_policy = CONTROL_INTEGRATOR_RESET
  };

  control_ctx_t ctrl;
  control_init(&ctrl, &cfg);

  /* Единицы полей см. control_cmd_t. */
  const control_cmd_t cmd = {
    .i_ref_cmd = 5.0f,
    .enable_cmd = true,
    .cmd_valid = true
  };
  control_slow_step(&ctrl, &cmd);

  /* Единицы полей см. control_meas_t. */
  const control_meas_t meas = {
    .i_meas = 0.0f,
    .u_meas = 0.0f,
    .udc = 0.0f,
    .meas_valid = true
  };

  control_out_t out = {0};

  control_fast_step(&ctrl, &meas, true, &out);
  test_expect_close(ctx, out.i_ref_used, 1.0f, 1e-6f, "slew step 1 should be 1 A");
  test_expect_true(ctx, (out.flags & CONTROL_FLAG_SLEW_ACTIVE) != 0u, "slew flag should be active during ramp");

  control_fast_step(&ctrl, &meas, true, &out);
  test_expect_close(ctx, out.i_ref_used, 2.0f, 1e-6f, "slew step 2 should be 2 A");

  control_fast_step(&ctrl, &meas, true, &out);
  test_expect_close(ctx, out.i_ref_used, 3.0f, 1e-6f, "slew step 3 should be 3 A");

  control_fast_step(&ctrl, &meas, true, &out);
  control_fast_step(&ctrl, &meas, true, &out);
  test_expect_close(ctx, out.i_ref_used, 5.0f, 1e-6f, "slew should reach target");
  test_expect_true(ctx, (out.flags & CONTROL_FLAG_SLEW_ACTIVE) == 0u, "slew flag should clear at target");
}

/**
 * @brief Тест: clamp уставки выставляет флаг.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_iref_clamp_flag(test_ctx_t *ctx)
{
  /* Единицы полей см. control_cfg_t. */
  const control_cfg_t cfg = {
    .kp = 0.0f,
    .ki = 0.0f,
    .dt = 1.0f,
    .u_min = -1.0f,
    .u_max = 1.0f,
    .i_ref_min = 0.0f,
    .i_ref_max = 10.0f,
    .di_dt_max = 0.0f,
    .integrator_policy = CONTROL_INTEGRATOR_RESET
  };

  control_ctx_t ctrl;
  control_init(&ctrl, &cfg);

  /* Единицы полей см. control_cmd_t. */
  const control_cmd_t cmd = {
    .i_ref_cmd = 25.0f,
    .enable_cmd = true,
    .cmd_valid = true
  };
  control_slow_step(&ctrl, &cmd);

  /* Единицы полей см. control_meas_t. */
  const control_meas_t meas = {
    .i_meas = 0.0f,
    .u_meas = 0.0f,
    .udc = 0.0f,
    .meas_valid = true
  };

  control_out_t out = {0};

  control_fast_step(&ctrl, &meas, true, &out);
  test_expect_close(ctx, out.i_ref_used, 10.0f, 1e-6f, "clamped i_ref should equal i_ref_max");
  test_expect_true(ctx, (out.flags & CONTROL_FLAG_IREF_CLAMP) != 0u, "iref clamp flag should be set");
}

/**
 * @brief Тест: насыщение выставляет флаги и счётчики.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_saturation_flags_and_counters(test_ctx_t *ctx)
{
  /* Единицы полей см. control_cfg_t. */
  const control_cfg_t cfg = {
    .kp = 1.0f,
    .ki = 0.0f,
    .dt = 1.0f,
    .u_min = 0.0f,
    .u_max = 1.0f,
    .i_ref_min = 0.0f,
    .i_ref_max = 100.0f,
    .di_dt_max = 0.0f,
    .integrator_policy = CONTROL_INTEGRATOR_RESET
  };

  control_ctx_t ctrl;
  control_init(&ctrl, &cfg);

  /* Единицы полей см. control_cmd_t. */
  control_cmd_t cmd = {
    .i_ref_cmd = 10.0f,
    .enable_cmd = true,
    .cmd_valid = true
  };
  control_slow_step(&ctrl, &cmd);

  /* Единицы полей см. control_meas_t. */
  const control_meas_t meas = {
    .i_meas = 0.0f,
    .u_meas = 0.0f,
    .udc = 0.0f,
    .meas_valid = true
  };

  control_out_t out = {0};

  control_fast_step(&ctrl, &meas, true, &out);
  test_expect_close(ctx, out.u, 1.0f, 1e-6f, "u should clamp to u_max");
  test_expect_true(ctx, (out.flags & CONTROL_FLAG_LIMIT_HI) != 0u, "limit_hi flag should be set");
  test_expect_true(ctx, (out.flags & CONTROL_FLAG_SATURATED) != 0u, "saturated flag should be set");
  test_expect_true(ctx, out.limit_hi_steps == 1u, "limit_hi_steps should increment");

  cmd.i_ref_cmd = 0.0f;
  control_slow_step(&ctrl, &cmd);
  control_fast_step(&ctrl, &meas, true, &out);
  test_expect_true(ctx, out.limit_hi_steps == 0u, "limit_hi_steps should reset when not saturated");
}

/**
 * @brief Тест: anti-windup предотвращает рост интегратора при насыщении.
 * @param ctx Контекст тестов.
 * @return None.
 */
static void test_anti_windup_holds_integrator(test_ctx_t *ctx)
{
  /* Единицы полей см. control_cfg_t. */
  const control_cfg_t cfg = {
    .kp = 0.0f,
    .ki = 1.0f,
    .dt = 1.0f,
    .u_min = 0.0f,
    .u_max = 1.0f,
    .i_ref_min = 0.0f,
    .i_ref_max = 100.0f,
    .di_dt_max = 0.0f,
    .integrator_policy = CONTROL_INTEGRATOR_RESET
  };

  control_ctx_t ctrl;
  control_init(&ctrl, &cfg);

  /* Единицы полей см. control_cmd_t. */
  const control_cmd_t cmd = {
    .i_ref_cmd = 10.0f,
    .enable_cmd = true,
    .cmd_valid = true
  };
  control_slow_step(&ctrl, &cmd);

  /* Единицы полей см. control_meas_t. */
  const control_meas_t meas = {
    .i_meas = 0.0f,
    .u_meas = 0.0f,
    .udc = 0.0f,
    .meas_valid = true
  };

  control_out_t out = {0};

  control_fast_step(&ctrl, &meas, true, &out);
  const float integrator_after_first = ctrl.state.integrator; /* [отн. ед.] */
  const uint32_t flags_after_first = out.flags;

  control_fast_step(&ctrl, &meas, true, &out);
  const float integrator_after_second = ctrl.state.integrator; /* [отн. ед.] */

  test_expect_close(ctx, integrator_after_second, integrator_after_first, 1e-6f,
                    "integrator should not grow under sustained saturation");
  test_expect_true(ctx, (flags_after_first & CONTROL_FLAG_WINDUP_BLOCK) != 0u,
                   "windup block flag should be set when integration is blocked");
}

/**
 * @brief Точка входа для L1 unit tests.
 * @return Код завершения (0 = OK).
 */
int main(void)
{
  test_ctx_t ctx = {0};

  test_disable_resets_integrator(&ctx);
  test_meas_invalid_blocks_control(&ctx);
  test_cmd_invalid_blocks_control(&ctx);
  test_num_invalid_blocks_control(&ctx);
  test_cfg_invalid_blocks_control(&ctx);
  test_integrator_clamp(&ctx);
  test_slew_rate_limit(&ctx);
  test_iref_clamp_flag(&ctx);
  test_saturation_flags_and_counters(&ctx);
  test_anti_windup_holds_integrator(&ctx);

  if (ctx.failed != 0)
  {
    (void)printf("Tests failed: %d\n", ctx.failed);
    return 1;
  }

  (void)printf("All tests passed.\n");
  return 0;
}



