#include "control_core.h"
#include <math.h>
#include <stddef.h>

/**
 * @brief Проверить валидность конфигурации регулятора.
 * @param cfg Указатель на конфигурацию.
 * @return true, если конфигурация валидна.
 */
static bool control_cfg_is_valid(const control_cfg_t *cfg)
{
  if (cfg == NULL)
  {
    return false;
  }
  if (!isfinite(cfg->kp) ||
      !isfinite(cfg->ki) ||
      !isfinite(cfg->dt) ||
      !isfinite(cfg->u_min) ||
      !isfinite(cfg->u_max) ||
      !isfinite(cfg->i_ref_min) ||
      !isfinite(cfg->i_ref_max) ||
      !isfinite(cfg->di_dt_max))
  {
    return false;
  }
  if (cfg->dt <= 0.0f)
  {
    return false;
  }
  if (cfg->kp < 0.0f)
  {
    return false;
  }
  if (cfg->ki < 0.0f)
  {
    return false;
  }
  if (cfg->u_min > cfg->u_max)
  {
    return false;
  }
  if (cfg->i_ref_min > cfg->i_ref_max)
  {
    return false;
  }
  if (cfg->di_dt_max < 0.0f)
  {
    return false;
  }
  return true;
}

/**
 * @brief Ограничить значение по диапазону.
 * @param value Входное значение, [отн. ед.].
 * @param min Минимум диапазона, [отн. ед.].
 * @param max Максимум диапазона, [отн. ед.].
 * @return Ограниченное значение.
 * @pre min <= max.
 */
static float control_clamp_f(float value, float min, float max)
{
  if (value < min)
  {
    return min;
  }
  if (value > max)
  {
    return max;
  }
  return value;
}

/**
 * @brief Применить ограничитель скорости изменения уставки.
 * @param target Целевая уставка, [A].
 * @param prev Предыдущая уставка, [A].
 * @param di_dt_max Максимальная скорость, [A/с].
 * @param dt Период шага, [с].
 * @param slew_active Указатель на флаг активности ограничения.
 * @return Ограниченная уставка, [A].
 */
static float control_apply_slew(float target,
                                float prev,
                                float di_dt_max,
                                float dt,
                                bool *slew_active)
{
  float result = target; /* [A] */
  bool active = false;

  if ((di_dt_max > 0.0f) && (dt > 0.0f))
  {
    const float max_delta = di_dt_max * dt; /* [A] */
    const float delta = target - prev; /* [A] */
    if (delta > max_delta)
    {
      result = prev + max_delta;
      active = true;
    }
    else if (delta < -max_delta)
    {
      result = prev - max_delta;
      active = true;
    }
  }

  if (slew_active != NULL)
  {
    *slew_active = active;
  }

  return result;
}

/**
 * @brief Применить политику безопасного запрета управления.
 * @param cfg Указатель на конфигурацию.
 * @param state Указатель на состояние.
 * @return None.
 */
static void control_apply_disable_policy(const control_cfg_t *cfg, control_state_t *state)
{
  if (cfg->integrator_policy == CONTROL_INTEGRATOR_RESET)
  {
    state->integrator = 0.0f;
    state->i_ref_used = 0.0f;
  }
  state->limit_hi_steps = 0u;
  state->limit_lo_steps = 0u;
}

void control_init(control_ctx_t *ctx, const control_cfg_t *cfg)
{
  ctx->cfg = *cfg;
  ctx->state.integrator = 0.0f;
  ctx->state.i_ref_used = 0.0f;
  const control_cmd_t cmd_zero = {0};
  ctx->state.cmd_buf[0] = cmd_zero;
  ctx->state.cmd_buf[1] = cmd_zero;
  atomic_init(&ctx->state.active_cmd_idx, 0u);
  ctx->state.cfg_valid = false;
  ctx->state.limit_hi_steps = 0u;
  ctx->state.limit_lo_steps = 0u;
  ctx->state.cfg_valid = control_cfg_is_valid(cfg);
}

void control_slow_step(control_ctx_t *ctx, const control_cmd_t *cmd)
{
  const uint32_t active_idx = atomic_load_explicit(&ctx->state.active_cmd_idx, memory_order_relaxed) & 1u;
  const uint32_t next_idx = active_idx ^ 1u;
  ctx->state.cmd_buf[next_idx] = *cmd;
  atomic_store_explicit(&ctx->state.active_cmd_idx, next_idx, memory_order_release);
}

void control_fast_step(control_ctx_t *ctx, const control_meas_t *meas, bool allow, control_out_t *out)
{
  // SAFETY: при запрете управления или невалидных измерениях запрос на управление = 0.
  // SAFETY: ядро не принимает решений о latch/recovery и не управляет аппаратным shutdown-path.

  uint32_t flags = CONTROL_FLAG_NONE; /* [битовая маска] */
  control_cmd_t cmd_snapshot = {0};

  const uint32_t cmd_idx = atomic_load_explicit(&ctx->state.active_cmd_idx, memory_order_acquire) & 1u;

  cmd_snapshot = ctx->state.cmd_buf[cmd_idx];

  if (!cmd_snapshot.cmd_valid)
  {
    flags |= CONTROL_FLAG_CMD_INVALID;
  }
  if (!ctx->state.cfg_valid)
  {
    flags |= CONTROL_FLAG_CFG_INVALID;
  }
  if (!isfinite(cmd_snapshot.i_ref_cmd) || !isfinite(meas->i_meas))
  {
    flags |= CONTROL_FLAG_NUM_INVALID;
  }

  const bool allow_cmd = (allow && cmd_snapshot.cmd_valid && cmd_snapshot.enable_cmd);

  if (!allow_cmd)
  {
    flags |= CONTROL_FLAG_DISABLED;
  }
  if (!meas->meas_valid)
  {
    flags |= CONTROL_FLAG_MEAS_INVALID;
  }

  if ((!allow_cmd) || (!meas->meas_valid) ||
      ((flags & (CONTROL_FLAG_CFG_INVALID | CONTROL_FLAG_CMD_INVALID | CONTROL_FLAG_NUM_INVALID)) != 0u))
  {
    // Шаг 1: Безопасный выход при запрете или невалидных измерениях.
    control_apply_disable_policy(&ctx->cfg, &ctx->state);
    out->u = 0.0f;
    out->i_ref_used = ctx->state.i_ref_used;
    out->enable_request = false;
    out->flags = flags;
    out->limit_hi_steps = ctx->state.limit_hi_steps;
    out->limit_lo_steps = ctx->state.limit_lo_steps;
    return;
  }

  // Шаг 2: Ограничить уставку по диапазону.
  const float i_ref_cmd = cmd_snapshot.i_ref_cmd; /* [A] */
  const float i_ref_clamped = control_clamp_f(i_ref_cmd, ctx->cfg.i_ref_min, ctx->cfg.i_ref_max); /* [A] */
  if (i_ref_clamped != i_ref_cmd)
  {
    flags |= CONTROL_FLAG_IREF_CLAMP;
  }

  // Шаг 3: Применить slew-rate лимитер.
  bool slew_active = false;
  const float i_ref_used = control_apply_slew(i_ref_clamped,
                                              ctx->state.i_ref_used,
                                              ctx->cfg.di_dt_max,
                                              ctx->cfg.dt,
                                              &slew_active); /* [A] */
  if (slew_active)
  {
    flags |= CONTROL_FLAG_SLEW_ACTIVE;
  }
  ctx->state.i_ref_used = i_ref_used;

  // Шаг 4: Вычислить ошибку по току.
  const float error = i_ref_used - meas->i_meas; /* [A] */

  // Шаг 5: PI + anti-windup (conditional integration).
  // Anti-windup: запрещаем "ухудшающее" интегрирование в насыщении и ограничиваем интегратор,
  // чтобы после выхода из лимита не получить длительный выброс управления.
  const float u_p = ctx->cfg.kp * error; /* [отн. ед.] */
  float u_i = ctx->state.integrator; /* [отн. ед.] */
  float u_unsat = u_p + u_i; /* [отн. ед.] */
  bool sat_hi = (u_unsat > ctx->cfg.u_max);
  bool sat_lo = (u_unsat < ctx->cfg.u_min);
  bool integrate = true;

  if ((ctx->cfg.dt <= 0.0f) || (ctx->cfg.ki == 0.0f))
  {
    integrate = false;
  }
  if (sat_hi && (error > 0.0f))
  {
    flags |= CONTROL_FLAG_WINDUP_BLOCK;
    integrate = false;
  }
  if (sat_lo && (error < 0.0f))
  {
    flags |= CONTROL_FLAG_WINDUP_BLOCK;
    integrate = false;
  }

  if (integrate)
  {
    u_i = u_i + (ctx->cfg.ki * error * ctx->cfg.dt);
  }

  if (!isfinite(u_i))
  {
    flags |= CONTROL_FLAG_NUM_INVALID;
    control_apply_disable_policy(&ctx->cfg, &ctx->state);
    out->u = 0.0f;
    out->i_ref_used = ctx->state.i_ref_used;
    out->enable_request = false;
    out->flags = flags;
    out->limit_hi_steps = ctx->state.limit_hi_steps;
    out->limit_lo_steps = ctx->state.limit_lo_steps;
    return;
  }

  const float u_i_clamped = control_clamp_f(u_i, ctx->cfg.u_min, ctx->cfg.u_max);
  if (u_i_clamped != u_i)
  {
    flags |= CONTROL_FLAG_WINDUP_BLOCK;
  }
  u_i = u_i_clamped;

  u_unsat = u_p + u_i;
  const float u = control_clamp_f(u_unsat, ctx->cfg.u_min, ctx->cfg.u_max); /* [отн. ед.] */
  sat_hi = (u_unsat > ctx->cfg.u_max);
  sat_lo = (u_unsat < ctx->cfg.u_min);

  if (sat_hi)
  {
    ctx->state.limit_hi_steps += 1u;
    ctx->state.limit_lo_steps = 0u;
    flags |= CONTROL_FLAG_LIMIT_HI;
    flags |= CONTROL_FLAG_SATURATED;
  }
  else if (sat_lo)
  {
    ctx->state.limit_lo_steps += 1u;
    ctx->state.limit_hi_steps = 0u;
    flags |= CONTROL_FLAG_LIMIT_LO;
    flags |= CONTROL_FLAG_SATURATED;
  }
  else
  {
    ctx->state.limit_hi_steps = 0u;
    ctx->state.limit_lo_steps = 0u;
  }

  ctx->state.integrator = u_i;

  // Шаг 6: Сформировать выход.
  out->u = u;
  out->i_ref_used = i_ref_used;
  out->enable_request = true;
  out->flags = flags;
  out->limit_hi_steps = ctx->state.limit_hi_steps;
  out->limit_lo_steps = ctx->state.limit_lo_steps;
}
