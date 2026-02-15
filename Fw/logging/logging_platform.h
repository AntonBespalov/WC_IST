#ifndef LOGGING_PLATFORM_H
#define LOGGING_PLATFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file logging_platform.h
 * @brief Платформенные хуки (critical section) для логирования.
 * @details Переопределите LOGGING_CRITICAL_ENTER/LOGGING_CRITICAL_EXIT,
 * чтобы защитить запись Record и бюджет LOG в ISR/task окружении.
 */

/**
 * @brief Тип состояния критической секции.
 */
typedef uint32_t logging_critical_state_t;

#if defined(STM32G474xx)
#include "logging_platform_stm32g4.h"
#endif

/**
 * @brief Вход в критическую секцию.
 * @return Сохранённое состояние, [безразм.].
 */
#ifndef LOGGING_CRITICAL_ENTER
#define LOGGING_CRITICAL_ENTER() ((logging_critical_state_t)0u)
#define LOGGING_CRITICAL_DEFAULT 1
#endif

/**
 * @brief Выход из критической секции.
 * @param state Сохранённое состояние.
 * @return None.
 */
#ifndef LOGGING_CRITICAL_EXIT
#define LOGGING_CRITICAL_EXIT(state) do { (void)(state); } while (0)
#define LOGGING_CRITICAL_DEFAULT 1
#endif

/**
 * @brief Обязать настройку critical section для on-target.
 * @note Определите LOGGING_REQUIRE_CRITICAL_SECTION в конфигурации сборки.
 */
#if defined(LOGGING_REQUIRE_CRITICAL_SECTION)
#if defined(LOGGING_CRITICAL_DEFAULT)
#error "LOGGING_CRITICAL_* must be overridden for on-target build"
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_PLATFORM_H */
