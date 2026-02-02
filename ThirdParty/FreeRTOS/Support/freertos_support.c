/*
 * FreeRTOS интеграция (хуки/ассерты) для WC_IST.
 *
 * Важно: политика safe state/аварий описана в docs/PROJECT_CONTEXT.md.
 * Здесь оставлен минимальный безопасный по умолчанию вариант: останов.
 */

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"

void vApplicationMallocFailedHook(void)
{
  Error_Handler();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  (void)pcTaskName;
  Error_Handler();
}

void vAssertCalled(const char *file, int line)
{
  (void)file;
  (void)line;
  Error_Handler();
}

