# DN-021_pccom4_dma_zero_copy_rx — RX DMA circular zero-copy for PCcom4

Статус: draft  
Дата: 2026-02-18  
Владелец: TBD  
Связано: TBD  

---

## 1) Context / Problem
- Нужно убрать копирование данных в DMA ISR и перейти на RX DMA circular zero‑copy с чтением по `NDTR`.
- Контекст исполнения: DMA ISR + app task 1 мс; временной домен async + 1 мс.
- Протокол PCcom4 и формат кадров менять нельзя (см. `docs/protocols/PCCOM4.02.md`, `docs/protocols/PCCOM4.02_PROJECT.md`).

## 2) Goal / Non-goals
### Goal
- Перенести RX на DMA circular zero‑copy и убрать `memcpy` из ISR.
- Сохранить протокол и поведение PCcom4.
- Обеспечить устойчивую работу на 10 МБод с измеримыми критериями.

### Non-goals
- Изменение протокола PCcom4 или формата данных.
- Рефакторинг вне перечисленных файлов.
- Изменения в fast loop или управлении сваркой.

## 3) Decision (что делаем)
- Используем DMA circular buffer для RX и вычисляем write‑индекс по `NDTR`.
- ISR выполняет только минимальные флаги/счётчики, парсинг и тяжёлая логика выполняются в app task.
- Политика переполнения фиксируется и отражается в статусе/счётчиках.

## 4) Rationale (почему так)
- Убираем тяжёлую работу из ISR, снижая джиттер.
- Zero‑copy уменьшает нагрузку на CPU и риск overruns.
- Сохраняем совместимость с PCcom4.

## 5) Interfaces / Data / Timing impact
- Добавляется/изменяется API чтения RX‑буфера в `uart_service`.
- Временные характеристики ISR должны улучшиться или остаться не хуже базовой прошивки.
- Протокол PCcom4 без изменений.

## 6) Risks / Edge cases
- Ошибка расчёта индексов при wrap‑around.
- Переполнение RX при burst‑нагрузке.
- Ошибки UART (framing/noise) в середине кадра.

## 7) Test plan / Proof / Rollback
### Test plan / Proof
- Unit/host: парсер и индексирование кольцевого буфера.
- SIL: моделирование потоков/ошибок, resync.
- On-target измерения (GPIO/осциллограф/trace): длительность ISR, джиттер 1 мс, стресс 10 МБод.
- HIL/bench: опционально, если нужен шумоустойчивый сценарий UART.

### Rollback
- Возврат на предыдущую реализацию RX (DMA + memcpy) через откат коммита, без изменения протокола.

## 8) Status / Implementation links
- Status: draft
- Links: TBD
