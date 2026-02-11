#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "control_core.h"

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
 * @brief Проверить, что имя теста совпадает с фильтром.
 * @param name Имя теста, [строка].
 * @param filter Фильтр (подстрока) или NULL, [строка].
 * @return true, если тест должен быть запущен.
 */
static bool test_matches_filter(const char *name, const char *filter)
{
  if (filter == NULL)
  {
    return true;
  }
  if (filter[0] == '\0')
  {
    return true;
  }
  return (strstr(name, filter) != NULL);
}

/**
 * @brief Вывести список доступных тестов.
 * @param tests Массив тестов.
 * @param count Количество тестов, [шт].
 * @return None.
 */
static void test_print_list(const test_case_t *tests, size_t count)
{
  (void)printf("Available tests (%zu):\n", count);
  for (size_t i = 0; i < count; ++i)
  {
    (void)printf("  %s\n", tests[i].name);
  }
}

/**
 * @brief Запустить набор тестов с фильтром по имени.
 * @param ctx Контекст тестов.
 * @param tests Массив тестов.
 * @param count Количество тестов, [шт].
 * @param filter Фильтр по имени (подстрока) или NULL.
 * @return Количество реально запущенных тестов, [шт].
 */
static size_t test_run_filtered(test_ctx_t *ctx,
                                const test_case_t *tests,
                                size_t count,
                                const char *filter)
{
  size_t executed = 0;

  for (size_t i = 0; i < count; ++i)
  {
    if (!test_matches_filter(tests[i].name, filter))
    {
      continue;
    }
    executed += 1u;
    tests[i].fn(ctx);
  }

  return executed;
}

/**
 * @brief Точка входа для L1 unit tests.
 * @param argc Количество аргументов командной строки, [шт].
 * @param argv Массив аргументов командной строки.
 * @return Код завершения (0 = OK).
 *
 * @details
 * Поддерживаемые режимы:
 * - без аргументов: запустить все тесты;
 * - `--list`: вывести список тестов;
 * - `--filter <substring>`: запустить тесты, чьи имена содержат подстроку;
 * - `--run <name>`: запустить один тест по точному имени.
 */
int main(int argc, char **argv)
{
  test_ctx_t ctx = {0};

  const test_case_t tests[] = {
    {"disable_resets_integrator", test_disable_resets_integrator},
    {"meas_invalid_blocks_control", test_meas_invalid_blocks_control},
    {"cmd_invalid_blocks_control", test_cmd_invalid_blocks_control},
    {"num_invalid_blocks_control", test_num_invalid_blocks_control},
    {"cfg_invalid_blocks_control", test_cfg_invalid_blocks_control},
    {"integrator_clamp", test_integrator_clamp},
    {"slew_rate_limit", test_slew_rate_limit},
    {"iref_clamp_flag", test_iref_clamp_flag},
    {"saturation_flags_and_counters", test_saturation_flags_and_counters},
    {"anti_windup_holds_integrator", test_anti_windup_holds_integrator},
  };
  const size_t test_count = sizeof(tests) / sizeof(tests[0]);

  const char *filter = NULL;
  bool list_only = false;
  bool exact_run = false;

  if (argc == 1)
  {
    /* default */
  }
  else if ((argc == 2) && (strcmp(argv[1], "--list") == 0))
  {
    list_only = true;
  }
  else if ((argc == 3) && (strcmp(argv[1], "--filter") == 0))
  {
    filter = argv[2];
  }
  else if ((argc == 3) && (strcmp(argv[1], "--run") == 0))
  {
    filter = argv[2];
    exact_run = true;
  }
  else
  {
    (void)printf("Usage:\n");
    (void)printf("  %s\n", argv[0]);
    (void)printf("  %s --list\n", argv[0]);
    (void)printf("  %s --filter <substring>\n", argv[0]);
    (void)printf("  %s --run <name>\n", argv[0]);
    return 2;
  }

  if (list_only)
  {
    test_print_list(tests, test_count);
    return 0;
  }

  const size_t executed = test_run_filtered(&ctx, tests, test_count, filter);
  if (exact_run && (executed != 1u))
  {
    (void)printf("FAIL: test '%s' not found.\n", filter);
    test_print_list(tests, test_count);
    return 2;
  }

  if (ctx.failed != 0)
  {
    (void)printf("Tests failed: %d\n", ctx.failed);
    return 1;
  }

  (void)printf("All tests passed.\n");
  return 0;
}



