#include "logging_tx_scheduler.h"
#include "logging_platform.h"
#include <stddef.h>

logging_result_t logging_tx_scheduler_init(logging_tx_scheduler_t *sched,
                                           uint32_t log_budget_step,
                                           uint32_t log_budget_max)
{
  if (sched == NULL)
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  sched->log_budget_step = log_budget_step;
  sched->log_budget_max = log_budget_max;
  sched->log_budget_bytes = 0u;
  return LOGGING_RESULT_OK;
}

void logging_tx_scheduler_on_tick(logging_tx_scheduler_t *sched)
{
  if (sched == NULL)
  {
    return;
  }
  const logging_critical_state_t irq_state = LOGGING_CRITICAL_ENTER();
  uint32_t next = sched->log_budget_bytes + sched->log_budget_step;
  if (next > sched->log_budget_max)
  {
    next = sched->log_budget_max;
  }
  sched->log_budget_bytes = next;
  LOGGING_CRITICAL_EXIT(irq_state);
}

logging_result_t logging_tx_scheduler_next(logging_tx_scheduler_t *sched,
                                           const logging_tx_queue_if_t *iface,
                                           uint8_t *out,
                                           uint32_t max_len,
                                           logging_tx_class_t *out_class,
                                           uint32_t *out_len)
{
  if ((sched == NULL) || (iface == NULL) || (out == NULL) || (out_class == NULL) || (out_len == NULL))
  {
    return LOGGING_RESULT_INVALID_ARG;
  }
  *out_len = 0u;
  *out_class = LOGGING_TX_CLASS_NONE;

  if ((iface->has_pdo != NULL) && (iface->pop_pdo != NULL) && iface->has_pdo(iface->ctx))
  {
    const uint32_t len = iface->pop_pdo(iface->ctx, out, max_len);
    if (len > 0u)
    {
      *out_len = len;
      *out_class = LOGGING_TX_CLASS_PDO;
      return LOGGING_RESULT_OK;
    }
  }

  if ((iface->has_log != NULL) && (iface->pop_log != NULL) && iface->has_log(iface->ctx))
  {
    uint32_t budget = 0u;
    const logging_critical_state_t irq_state = LOGGING_CRITICAL_ENTER();
    budget = sched->log_budget_bytes;
    LOGGING_CRITICAL_EXIT(irq_state);

    if (budget == 0u)
    {
      return LOGGING_RESULT_NOT_READY;
    }
    const uint32_t allowed = (budget < max_len) ? budget : max_len;
    const uint32_t len = iface->pop_log(iface->ctx, out, allowed);
    if (len > 0u)
    {
      if (len > allowed)
      {
        return LOGGING_RESULT_INVALID_ARG;
      }
      const logging_critical_state_t irq_state_sub = LOGGING_CRITICAL_ENTER();
      sched->log_budget_bytes -= len;
      LOGGING_CRITICAL_EXIT(irq_state_sub);
      *out_len = len;
      *out_class = LOGGING_TX_CLASS_LOG;
      return LOGGING_RESULT_OK;
    }
  }

  return LOGGING_RESULT_NOT_READY;
}