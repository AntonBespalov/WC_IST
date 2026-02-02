/*
 * FreeRTOSConfig.h
 *
 * Конфигурация FreeRTOS для WC_IST (STM32G474, Cortex-M4F, STM32CubeIDE/HAL).
 *
 * Источник Kernel: FreeRTOS 202406.04-LTS (Kernel V11.1.0),
 * см. каталог `FreeRTOSv202406.04-LTS/` в корне репозитория.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>
#include <stddef.h>

#include "stm32g4xx.h"

/*-----------------------------------------------------------
 * Базовые настройки планировщика.
 *----------------------------------------------------------*/

#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0

#define configCPU_CLOCK_HZ                      ( SystemCoreClock )
#define configTICK_RATE_HZ                      1000U

#define configMAX_PRIORITIES                    8
#define configMINIMAL_STACK_SIZE                ( ( uint16_t ) 128 )
#define configMAX_TASK_NAME_LEN                 16

#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1

/*-----------------------------------------------------------
 * Память.
 *----------------------------------------------------------*/

#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1

/* Под heap_4.c. Настроить по фактическим потребностям проекта. */
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 24U * 1024U ) )

#define configUSE_MALLOC_FAILED_HOOK            1

/*-----------------------------------------------------------
 * Синхронизация/очереди/таймеры.
 *----------------------------------------------------------*/

#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1

#define configQUEUE_REGISTRY_SIZE               8

#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               2
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            ( ( uint16_t ) 256 )

/*-----------------------------------------------------------
 * Диагностика.
 *----------------------------------------------------------*/

#define configCHECK_FOR_STACK_OVERFLOW          2

#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

/*-----------------------------------------------------------
 * Cortex-M прерывания.
 *
 * Важно: Любой ISR, который вызывает FreeRTOS API *из ISR* (…FromISR),
 * должен иметь приоритет НИЖЕ (численно БОЛЬШЕ или равный), чем
 * configMAX_SYSCALL_INTERRUPT_PRIORITY.
 *----------------------------------------------------------*/

#define configPRIO_BITS                         __NVIC_PRIO_BITS

/* 0..15 для 4-битной приоритетности (STM32G4). 15 = самый низкий приоритет. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY     5

#define configKERNEL_INTERRUPT_PRIORITY         ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8U - configPRIO_BITS ) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8U - configPRIO_BITS ) )

/*-----------------------------------------------------------
 * Включение/исключение API (минимальный разумный набор).
 *----------------------------------------------------------*/

#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1

/*-----------------------------------------------------------
 * Assert.
 *----------------------------------------------------------*/

void vAssertCalled(const char *file, int line);

#define configASSERT(x)                         do { if ((x) == 0) { vAssertCalled(__FILE__, __LINE__); } } while (0)

#endif /* FREERTOS_CONFIG_H */
