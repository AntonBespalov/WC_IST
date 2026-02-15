#ifndef LOGGING_PLATFORM_STM32G4_H
#define LOGGING_PLATFORM_STM32G4_H

#include <stdint.h>
#include "stm32g4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file logging_platform_stm32g4.h
 * @brief Реализация critical section для STM32G4.
 * @details Использует PRIMASK для защиты записи логов в ISR/task окружении.
 */

/**
 * @brief Войти в critical section (PRIMASK).
 * @return Сохранённое состояние PRIMASK, [безразм.].
 */
static inline uint32_t logging_platform_stm32g4_enter(void)
{
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

/**
 * @brief Выйти из critical section (PRIMASK).
 * @param state Сохранённое состояние PRIMASK, [безразм.].
 * @return None.
 */
static inline void logging_platform_stm32g4_exit(uint32_t state)
{
  __set_PRIMASK(state);
}

#ifndef LOGGING_CRITICAL_ENTER
#define LOGGING_CRITICAL_ENTER() logging_platform_stm32g4_enter()
#endif

#ifndef LOGGING_CRITICAL_EXIT
#define LOGGING_CRITICAL_EXIT(state) logging_platform_stm32g4_exit((state))
#endif

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_PLATFORM_STM32G4_H */
