#ifndef CONTROL_CORE_H
#define CONTROL_CORE_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file control_core.h
 * @brief Платформо-независимое ядро управления током (MFDC).
 * @details
 * Ядро не использует HAL/CMSIS/FreeRTOS и не имеет права управлять TIM1/BKIN/MOE.
 * Управление рассчитывается детерминированно в fast-домене (PWM), команды принимаются в slow-домене (250 мкс / 4 кГц).
 * В fast-домене используется последняя валидная команда (command latch), защёлкнутая на границе периода PWM.
 * Маппинг `u` в аппаратные регистры выполняется в `pwm_hal`.
 */

/**
 * @brief Политика обработки интегратора при запрете управления.
 */
typedef enum {
  CONTROL_INTEGRATOR_RESET = 0, /**< Сброс интегратора при запрете/невалидных измерениях. */
  CONTROL_INTEGRATOR_HOLD = 1   /**< Удержание интегратора при запрете/невалидных измерениях. */
} control_integrator_policy_t;

/**
 * @brief Флаги состояния/диагностики ядра управления.
 */
typedef enum {
  CONTROL_FLAG_NONE = 0u,             /**< Нет флагов. */
  CONTROL_FLAG_DISABLED = (1u << 0),  /**< Управление запрещено (allow/enable_cmd=0). */
  CONTROL_FLAG_MEAS_INVALID = (1u << 1), /**< Невалидные измерения. */
  CONTROL_FLAG_IREF_CLAMP = (1u << 2),   /**< Уставка зажата по диапазону. */
  CONTROL_FLAG_SLEW_ACTIVE = (1u << 3),  /**< Активен slew-rate лимитер. */
  CONTROL_FLAG_LIMIT_HI = (1u << 4),     /**< Насыщение по верхнему пределу u. */
  CONTROL_FLAG_LIMIT_LO = (1u << 5),     /**< Насыщение по нижнему пределу u. */
  CONTROL_FLAG_SATURATED = (1u << 6),      /**< Сатурация u (любой предел). */
  CONTROL_FLAG_CFG_INVALID = (1u << 7),    /**< Невалидная конфигурация. */
  CONTROL_FLAG_CMD_INVALID = (1u << 8),    /**< Невалидная команда/устаревшая команда. */
  CONTROL_FLAG_NUM_INVALID = (1u << 9),    /**< Некорректные численные значения (NaN/Inf). */
  CONTROL_FLAG_WINDUP_BLOCK = (1u << 10)   /**< Блокировка интегрирования от усугубления насыщения. */
} control_status_flag_t;

/**
 * @brief Конфигурация ядра управления.
 */
typedef struct {
  float kp; /**< Коэффициент P, [отн. ед./A]. */
  float ki; /**< Коэффициент I, [отн. ед./(A*с)]. */
  float dt; /**< Период шага управления, [с]. */
  float u_min; /**< Минимальное управляющее воздействие, [отн. ед.]. */
  float u_max; /**< Максимальное управляющее воздействие, [отн. ед.]. */
  float i_ref_min; /**< Минимальная уставка тока, [A]. */
  float i_ref_max; /**< Максимальная уставка тока, [A]. */
  float di_dt_max; /**< Максимальная скорость изменения уставки, [A/с]. */
  control_integrator_policy_t integrator_policy; /**< Политика интегратора при запрете. */
} control_cfg_t;

/**
 * @brief Команда управления (slow-домен, 250 мкс / 4 кГц).
 * @details Используется последняя валидная команда, защёлкнутая на границе периода PWM (command latch).
 */
typedef struct {
  float i_ref_cmd; /**< Команда уставки тока от ТК, [A]. */
  bool enable_cmd; /**< Команда разрешения управления от ТК. */
  bool cmd_valid; /**< Признак валидности/актуальности команды. */
} control_cmd_t;

/**
 * @brief Измерения и признаки качества (fast-домен, PWM).
 */
typedef struct {
  float i_meas; /**< Измеренный ток (среднее за период PWM), [A]. */
  float u_meas; /**< Измеренное напряжение (опционально), [В]. */
  float udc; /**< Напряжение звена DC (опционально), [В]. */
  bool meas_valid; /**< Признак валидности измерений. */
} control_meas_t;

/**
 * @brief Выходы ядра управления (fast-домен, PWM).
 */
typedef struct {
  float u; /**< Управляющее воздействие (нормализовано), [отн. ед.]. */
  float i_ref_used; /**< Уставка после ограничений/ slew-rate, [A]. */
  bool enable_request; /**< Запрос на применение управления. */
  uint32_t flags; /**< Битовая маска control_status_flag_t. */
  uint32_t limit_hi_steps; /**< Шаги подряд в верхнем насыщении, [шаги]. */
  uint32_t limit_lo_steps; /**< Шаги подряд в нижнем насыщении, [шаги]. */
} control_out_t;

/**
 * @brief Состояние ядра управления.
 */
typedef struct {
  float integrator; /**< Состояние интегратора, [отн. ед.]. */
  float i_ref_used; /**< Последняя использованная уставка, [A]. */
  control_cmd_t cmd_buf[2]; /**< Два буфера команды (double-buffer), [отн. ед.]. */
  atomic_uint_fast32_t active_cmd_idx; /**< Индекс активного буфера, [индекс]. */
  bool cfg_valid; /**< Признак валидности конфигурации. */
  uint32_t limit_hi_steps; /**< Счётчик верхнего насыщения, [шаги]. */
  uint32_t limit_lo_steps; /**< Счётчик нижнего насыщения, [шаги]. */
} control_state_t;

/**
 * @brief Контекст ядра управления.
 */
typedef struct {
  control_cfg_t cfg; /**< Конфигурация регулятора. */
  control_state_t state; /**< Состояние регулятора. */
} control_ctx_t;

/**
 * @brief Инициализировать контекст ядра управления.
 * @param ctx Указатель на контекст.
 * @param cfg Указатель на конфигурацию.
 * @return None.
 * @pre ctx != NULL, cfg != NULL.
 */
void control_init(control_ctx_t *ctx, const control_cfg_t *cfg);

/**
 * @brief Принять команду из slow-домена (250 мкс / 4 кГц).
 * @param ctx Указатель на контекст.
 * @param cmd Указатель на команду.
 * @return None.
 * @pre ctx != NULL, cmd != NULL.
 * @note Функция не выполняет тяжёлых вычислений и не блокирует fast-домен.
 */
void control_slow_step(control_ctx_t *ctx, const control_cmd_t *cmd);

/**
 * @brief Выполнить детерминированный шаг управления в fast-домене (PWM).
 * @param ctx Указатель на контекст.
 * @param meas Указатель на измерения и признак качества.
 * @param allow Разрешение управления от safety_supervisor.
 * @param out Указатель на выходные данные.
 * @return None.
 * @pre ctx != NULL, meas != NULL, out != NULL.
 * @details
 * Шаг управления должен вызываться ровно один раз на период PWM.
 * Функция не использует HAL/RTOS и не выполняет логирования.
 * Алгоритм (высокий уровень):
 * 1) снапшот команды + базовая валидация/deny-by-default;
 * 2) conditioning уставки (`clamp` + `slew-rate`);
 * 3) gating по разрешениям/валидности измерений;
 * 4) PI + anti-windup + лимиты + диагностика.
 */
void control_fast_step(control_ctx_t *ctx, const control_meas_t *meas, bool allow, control_out_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_CORE_H */
