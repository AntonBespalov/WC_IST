#ifndef MFDC_SERVICE_TX_SCHEDULER_H
#define MFDC_SERVICE_TX_SCHEDULER_H

#include "logging_cfg.h"
#include "pccom4_parser.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Приоритет очереди отправки.
 */
typedef enum
{
  SVC_TX_PRIO_P0 = 0, /**< PDO emu. */
  SVC_TX_PRIO_P1 = 1, /**< Control vars stream. */
  SVC_TX_PRIO_P2 = 2  /**< RAW capture. */
} svc_tx_prio_t;

/**
 * @brief Статистика очередей отправки.
 */
typedef struct
{
  /** @brief Дропы очереди P1, [шт]. */
  uint32_t cnt_p1_drop;
  /** @brief Дропы очереди P2, [шт]. */
  uint32_t cnt_p2_drop;
  /** @brief Максимальная заполненность Q0, [кадров]. */
  uint16_t q0_highwater;
  /** @brief Максимальная заполненность Q1, [кадров]. */
  uint16_t q1_highwater;
  /** @brief Максимальная заполненность Q2, [кадров]. */
  uint16_t q2_highwater;
} service_tx_stats_t;

/**
 * @brief Элемент очереди отправки.
 */
typedef struct
{
  /** @brief Длина кадра, [байт]. */
  uint16_t len;
  /** @brief Кадр PCcom4 (с PREAMBLE), [байт]. */
  uint8_t bytes[PCCOM4_MAX_FRAME_BYTES];
} service_tx_item_t;

/**
 * @brief Контекст планировщика отправки.
 */
typedef struct
{
  /** @brief Очередь P0. */
  service_tx_item_t q0[MFDC_SVC_TX_Q0_CAPACITY];
  /** @brief Очередь P1. */
  service_tx_item_t q1[MFDC_SVC_TX_Q1_CAPACITY];
  /** @brief Очередь P2. */
  service_tx_item_t q2[MFDC_SVC_TX_Q2_CAPACITY];
  /** @brief Индекс чтения Q0, [индекс]. */
  uint16_t q0_rd;
  /** @brief Индекс записи Q0, [индекс]. */
  uint16_t q0_wr;
  /** @brief Кол-во элементов Q0, [кадров]. */
  uint16_t q0_count;
  /** @brief Индекс чтения Q1, [индекс]. */
  uint16_t q1_rd;
  /** @brief Индекс записи Q1, [индекс]. */
  uint16_t q1_wr;
  /** @brief Кол-во элементов Q1, [кадров]. */
  uint16_t q1_count;
  /** @brief Индекс чтения Q2, [индекс]. */
  uint16_t q2_rd;
  /** @brief Индекс записи Q2, [индекс]. */
  uint16_t q2_wr;
  /** @brief Кол-во элементов Q2, [кадров]. */
  uint16_t q2_count;
  /** @brief Лог-бюджет на тик, [байт]. */
  uint32_t log_budget_per_tick;
  /** @brief Текущий лог-бюджет, [байт]. */
  uint32_t log_budget;
  /** @brief Признак использования бюджета. */
  bool budget_enabled;
  /** @brief Статистика. */
  service_tx_stats_t stats;
} service_tx_scheduler_t;

/**
 * @brief Инициализировать планировщик.
 * @param sched Указатель на контекст.
 * @return None.
 */
void service_tx_scheduler_init(service_tx_scheduler_t *sched);

/**
 * @brief Задать бюджет логов на тик.
 * @param sched Указатель на контекст.
 * @param budget_bytes Бюджет на тик, [байт].
 * @return None.
 */
void service_tx_scheduler_set_budget(service_tx_scheduler_t *sched, uint32_t budget_bytes);

/**
 * @brief Начислить бюджет (вызывать раз в тик).
 * @param sched Указатель на контекст.
 * @return None.
 */
void service_tx_scheduler_on_tick(service_tx_scheduler_t *sched);

/**
 * @brief Поместить кадр в очередь.
 * @param sched Указатель на контекст.
 * @param prio Приоритет очереди.
 * @param frame Кадр (с PREAMBLE).
 * @param frame_len Длина кадра, [байт].
 * @return true, если кадр принят.
 */
bool service_tx_scheduler_enqueue(service_tx_scheduler_t *sched,
                                  svc_tx_prio_t prio,
                                  const uint8_t *frame,
                                  uint16_t frame_len);

/**
 * @brief Извлечь следующий кадр для отправки.
 * @param sched Указатель на контекст.
 * @param out_frame Буфер для кадра.
 * @param out_len Длина кадра, [байт].
 * @return true, если кадр извлечён.
 */
bool service_tx_scheduler_dequeue(service_tx_scheduler_t *sched,
                                  uint8_t *out_frame,
                                  uint16_t *out_len);

/**
 * @brief Получить статистику очередей.
 * @param sched Указатель на контекст.
 * @param out_stats Выходная структура.
 * @return None.
 */
void service_tx_scheduler_get_stats(const service_tx_scheduler_t *sched, service_tx_stats_t *out_stats);

#endif /* MFDC_SERVICE_TX_SCHEDULER_H */
