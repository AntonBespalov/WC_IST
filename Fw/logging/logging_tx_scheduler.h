#ifndef LOGGING_TX_SCHEDULER_H
#define LOGGING_TX_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>
#include "logging_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file logging_tx_scheduler.h
 * @brief Планировщик отправки PDO/LOG поверх общего UART.
 */

/**
 * @brief Класс отправляемых данных.
 */
typedef enum {
  LOGGING_TX_CLASS_NONE = 0, /**< Ничего не отправлено. */
  LOGGING_TX_CLASS_PDO = 1,  /**< Управляющий обмен (PDO). */
  LOGGING_TX_CLASS_LOG = 2   /**< Логи/данные осциллографа. */
} logging_tx_class_t;

/**
 * @brief Интерфейс доступа к очередям PDO/LOG.
 */
typedef struct {
  void *ctx; /**< Пользовательский контекст, [указатель]. */
  bool (*has_pdo)(void *ctx); /**< Наличие PDO-кадров. */
  bool (*has_log)(void *ctx); /**< Наличие LOG-кадров. */
  uint32_t (*pop_pdo)(void *ctx, uint8_t *out, uint32_t max_len); /**< Выбор PDO-кадра, [байт]. */
  uint32_t (*pop_log)(void *ctx, uint8_t *out, uint32_t max_len); /**< Выбор LOG-кадра, [байт]. */
} logging_tx_queue_if_t;

/**
 * @brief Контекст планировщика TX.
 */
typedef struct {
  uint32_t log_budget_bytes;     /**< Текущий бюджет LOG, [байт]. */
  uint32_t log_budget_step;      /**< Пополнение бюджета на тик, [байт]. */
  uint32_t log_budget_max;       /**< Максимум бюджета, [байт]. */
} logging_tx_scheduler_t;

/**
 * @brief Инициализировать планировщик TX.
 * @param sched Указатель на контекст.
 * @param log_budget_step Пополнение бюджета на тик, [байт].
 * @param log_budget_max Максимум бюджета, [байт].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre sched != NULL.
 */
logging_result_t logging_tx_scheduler_init(logging_tx_scheduler_t *sched,
                                           uint32_t log_budget_step,
                                           uint32_t log_budget_max);

/**
 * @brief Тик планировщика (250 мкс, 4 кГц).
 * @param sched Указатель на контекст.
 * @return None.
 * @pre sched != NULL.
 * @note Вызывать из 250 мкс тика (ISR или task). См. logging_platform.h.
 */
void logging_tx_scheduler_on_tick(logging_tx_scheduler_t *sched);

/**
 * @brief Выбрать следующий кадр на отправку.
 * @param sched Указатель на контекст.
 * @param iface Интерфейс очередей.
 * @param out Буфер назначения.
 * @param max_len Максимальный размер, [байт].
 * @param out_class Класс выбранных данных.
 * @param out_len Фактическая длина, [байт].
 * @return LOGGING_RESULT_OK при успехе.
 * @pre sched != NULL, iface != NULL, out != NULL, out_class != NULL, out_len != NULL.
 */
logging_result_t logging_tx_scheduler_next(logging_tx_scheduler_t *sched,
                                           const logging_tx_queue_if_t *iface,
                                           uint8_t *out,
                                           uint32_t max_len,
                                           logging_tx_class_t *out_class,
                                           uint32_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_TX_SCHEDULER_H */