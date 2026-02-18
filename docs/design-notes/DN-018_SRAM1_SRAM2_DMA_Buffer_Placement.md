# DN-018_SRAM1_SRAM2_DMA_Buffer_Placement — Размещение DMA и лог-буферов по SRAM1/2 (draft)

Статус: deferred / parking lot  
Дата: 2026-02-18  
Владелец: TBD  
Связано: `docs/PROJECT_CONTEXT.md`, `docs/TEST_PLAN.md`, `docs/design-notes/DN-017_CCMRAM_FastLoop_Placement.md`

---

## 1) Context / Problem
- Fast loop (PWM) использует DMA измерений; лог/PCcom4 работают в slow task и используют UART DMA.
- Сейчас RAM задан как единый регион; это создаёт конкуренцию по шине между DMA измерений и логированием.
- Нужно зафиксировать разнесение буферов по SRAM1/2 без изменения поведения/таймингов.

## 2) Goal / Non-goals
### Goal
- Использовать только если измерения показывают влияние логирования на fast loop.
- Разместить DMA буферы измерений в SRAM1, а UART/лог-буферы в SRAM2.
- Подтвердить размещение по link map и измерениями L3.
- Не ухудшить jitter/latency fast loop и safe state.

### Non-goals
- Не менять алгоритмы fast loop/slow loop, параметры регулятора и протоколы.
- Не переносить DMA-буферы в CCM.
- Не менять политику аварий/восстановления.

## 3) Decision (что делаем)
- В линкере выделить регионы SRAM1 и SRAM2 и секцию `.sram2` (NOLOAD).
- UART RX ring, UART TX queue и capture SRAM buffer разместить в `.sram2`.
- DMA-буферы АЦП оставить в SRAM1 (обычные `.bss/.data`).
- Подтверждать корректность по `.map` и `nm`.

## 4) Rationale (почему так)
- SRAM1 обслуживает DMA измерений; перенос лог-буферов в SRAM2 снижает конкуренцию по шине.
- DMA не работает с CCM, поэтому лог-буферы остаются в SRAM1/2.
- Минимальные изменения и прозрачные доказательства (map + L3 измерения).
- Ожидаем улучшение worst-case при contention; если contention нет — эффект может быть нулевой.
- Триггер к реализации — измеренная корреляция jitter с логированием, а не предположение.

## 5) Interfaces / Data / Timing impact
- Внешние интерфейсы не меняются.
- Добавляется новая секция памяти `.sram2` и правила размещения буферов.
- Требуется подтвердить отсутствие ухудшений jitter/latency по L3.

## 6) Risks / Edge cases
- Ошибка в размерах SRAM1/2 в линкере → перекрытие памяти и HardFault.
- DMA USART1 может быть недоступен к SRAM2 → DMA ошибки/потери.
- Рост буферов в SRAM2 → переполнение и корупция данных.

## 7) Test plan / Proof / Rollback
### Test plan / Proof
- Unit/host: проверка `.map`/`nm` адресов символов (SRAM1/2).
- SIL: не требуется.
- On-target измерения (GPIO/осциллограф/trace): `control_tick` jitter, `ADC_START→ADC_READY`, `PWM_APPLY`, стресс UART/логов.
- HIL/bench: `BKIN_RAW→PWM_OUT safe` без ухудшения при активном логировании.

### Rollback
- Вернуть линковку к единому RAM и удалить `.sram2` атрибуты.
- Прогнать L3 базовые измерения на регресс.

## 8) Status / Implementation links
- Status: draft
- Links: TBD
