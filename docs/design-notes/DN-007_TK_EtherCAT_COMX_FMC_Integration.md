# DN-007_TK_EtherCAT_COMX_FMC_Integration — интерфейс ТК: EtherCAT (COMX↔FMC) + PDO-эмуляция по USB-UART (PCcom4)

Статус: draft  
Дата: 2026-02-10  
Владелец: Bespalov  
Связано: `docs/decisions/ADR-004_TK_Interface_EtherCAT_COMX_FMC_and_UART_PDO_Emu.md`, `docs/decisions/ADR-003_TK_EtherCAT_PDO_Emu_Over_UDP_Batching.md`, `docs/protocols/PCCOM4.02_PROJECT.md`, `docs/protocols/obsolete/PROTOCOL_TK.md` *(устар.)*  

---

## 1) Context / Problem
- Нужно реализовать принятое решение: заменить источник команд ТК **CAN FD → EtherCAT PDO** через модуль **COMX 100CA-RE**, подключённый к MCU по **FMC** (см. ADR).
- Для разработки/отладки требуется сохранить “PDO-like” обмен с ПК под Windows, но через **USB-UART (PCcom4 stream)**, узел `Node=0x03` (`TkPdo.Emu.*`).
- Контекст исполнения:
  - обработка EtherCAT/COMX и PCcom4 — в **slow loop / FreeRTOS task**;
  - ISR от COMX/UART (если есть) — **минимальная**: только сигнал/notify/флаг;
  - fast loop (PWM-домен) не зависит от сетевых/сервисных стеков (см. `docs/CONTEXT_SNAPSHOT.md`).
- Временной домен:
  - целевой период команд ТК: **250 мкс (4 кГц)** (best-effort; измерять фактический джиттер/возраст команды);
  - watchdog по отсутствию/невалидности команд: временный дефолт `LinkTimeoutMs = 20` (TBD по измерениям).

RT-механика (Draft):
- COMX_IRQ → EXTI ISR: только `notify`/флаг (без чтения FMC/process image).
- Обработка RxPDO/TxPDO: отдельная FreeRTOS task (высокий приоритет среди APP), читает process image в снапшот, валидирует и публикует “последнюю валидную” команду.
- При вызове FreeRTOS API из ISR — NVIC priority ISR должен быть **ниже** `configMAX_SYSCALL_INTERRUPT_PRIORITY` (см. `ThirdParty/FreeRTOS/Config/FreeRTOSConfig.h`).
- Бюджеты/критерии PASS: см. `docs/TEST_PLAN.md` и `docs/protocols/PROTOCOL_TK_ETHERCAT.md`.

## 2) Goal / Non-goals
### Goal
- Добавить “канал команд ТК” на EtherCAT PDO (COMX↔FMC), который:
  - выдаёт в систему **консервативно-валидированную** команду (`enable/уставка/режим/seq/флаги`) со счётчиками качества и метрикой “возраста команды”;
  - при потере/устаревании/невалидности команд гарантирует **safe stop / запрет сварки** по watchdog (без авто-рестарта после критических fault).
- Реализовать режим `TkPdo.Emu.*` по USB-UART (PCcom4 stream) как dev-источник команд/статуса с теми же полями, что и PDO (см. `docs/protocols/PCCOM4.02_PROJECT.md` / 3.2).
- Обеспечить наблюдаемость и доказуемость:
  - GPIO/trace для измерения задержек и джиттера,
  - счётчики/статистика `TkPdo.Emu.Stats`,
  - план fault-injection (минимум 5 сценариев).

### Non-goals
- Не менять fast loop, TIM1/PWM, BKIN/BKIN2, MOE и аппаратный shutdown-path (см. `docs/CONTEXT_SNAPSHOT.md`).
- Не реализовывать EtherCAT stack на MCU (mailbox/CoE/DC/SYNC): это зона ответственности COMX и системной интеграции.
- Не “улучшать” логику управления током/измерений/аварий ради интеграции интерфейса (изменения только по необходимости и отдельными решениями).
- Не обещать детерминизм “как EtherCAT DC” без отдельного решения и измерений.

## 3) Decision (что делаем)
- Делаем поэтапно (минимальный риск):
  1) Документация: синхронизировать профиль PCcom4 (`TkPdo.Emu.*`), пометки “CAN vs EtherCAT”, список missing specs (закрыть противоречия).
  2) Ввести абстракцию “источник команд ТК” (CAN/EtherCAT/Emu) с единым контрактом данных и валидаторами.
  3) COMX↔FMC интеграция:
     - task читает PDO (process image) в снапшот, валидирует и публикует команду;
     - ISR (если используется) только будит task.
  4) Watchdog/метрики:
     - таймаут по отсутствию валидной команды (`LinkTimeoutMs`) приводит к safe stop/запрету сварки;
     - ведём счётчики: ok/CRC/seq/out-of-order/missed/timeout + `cmd_age_max_us`.
  5) Dev-режим: обработчики `TkPdo.Emu.*` по USB-UART (PCcom4) как альтернативный источник команд и канал статуса (под feature flag).

## 4) Rationale (почему так)
- COMX↔FMC обслуживается в task-контексте, чтобы:
  - не увеличивать длительность/джиттер критичных ISR,
  - проще доказать тайминги и корректно обработать перегрузку (drop/counters/controlled stop).
- Единый контракт команд (CAN/EtherCAT/Emu) снижает риск рассинхронизации поведения и упрощает тесты/включение feature flags.
- Watchdog по “возрасту/валидности команды” обязателен, т.к. Windows/сеть/COMX могут давать джиттер и паузы.

## 5) Interfaces / Data / Timing impact
- Внутренний контракт “команда ТК” (пример, под фиксацию в коде/спеке):
  - `proto_ver` (u8), `enable` (bool), `estop_req` (bool), `seq` (u16),
  - `mode` (u8), `setpoint_i` (i32, единицы TBD), `setpoint_u` (i32, опционально),
  - `cmd_word0/cmd_word1` (u32), `cmd_valid` (bool), `cmd_age_us` (u32),
  - метрики качества: `rx_ok/rx_crc_err/rx_out_of_order/rx_missed/watchdog_trip/cmd_age_max_us`.
- Источники/маппинг:
  - EtherCAT: RxPDO/TxPDO (TBD в отдельной спецификации) маппятся на этот контракт.
  - Emu по UART: `Node=0x03` (`TkPdo.Emu.CmdWeld/FbStatus/Fault/Stats`) по `docs/protocols/PCCOM4.02_PROJECT.md` / 3.2.
- Тайминги:
  - измерять: период обновления команды, `cmd_age_us` на момент применения, задержку “PDO available → cmd_valid → cmd_applied”.
  - `LinkTimeoutMs` (временный дефолт 20 мс) подлежит уточнению измерениями и фиксации как константа/параметр.

## 6) Risks / Edge cases
- Неконсистентный снапшот process image (гонка чтения): требуется “атомарное” чтение (seq-before/seq-after или двойной буфер) + счётчик.
- COMX завис/не инициализировался: система должна остаться в safe state, сварка запрещена, причина видна в статусе/счётчиках.
- Out-of-order/пропуски `seq`: не должны приводить к “принятию старой команды”; нужна политика REJECT/APPLY.
- Перегрузка сервиса (UART, scope): допускается дроп/overflow, но запрещено влияние на fast loop; при сомнениях — controlled stop.
- Ошибка единиц/диапазонов уставки (`setpoint_i`): риск некорректной энергии; лечится валидаторами и тестами на диапазоны.

## 7) Test plan / Proof / Rollback
### Test plan / Proof
- Unit/host:
  - валидатор команды: reserved MUST=0, диапазоны `setpoint_*`, политика `seq` (wrap-around, out-of-order, gap).
  - логика watchdog: “нет валидной команды > LinkTimeoutMs” → запрет/stop + счётчик.
  - “атомарный снимок” PDO (модель seq-before/seq-after) → детект неконсистентности.
  - PCcom4 stream parser (если затрагивается): ресинхронизация, `Length`, CRC16, бурсты.
- SIL:
  - модель “приход команды (PDO/Emu) → публикация cmd → применение в slow loop → tx status/seq_echo”, включая паузы/джиттер/пропуски.
- On-target измерения (GPIO/осциллограф/trace):
  - сигналы (пример): `COMX_IRQ`, `PDO_SNAPSHOT_OK`, `CMD_VALID`, `CMD_APPLIED`, `WATCHDOG_TRIP`.
  - измерить:
    - `COMX_IRQ → CMD_APPLIED` (worst-case),
    - `cmd_age_us` (распределение, максимум),
    - влияние на jitter fast loop (сравнение “feature OFF vs ON”).
  - наблюдать: `BKIN_RAW` и `PWM_OUT` для подтверждения аппаратного shutdown-path.
- HIL/bench (fault-injection, минимум 5):
  1) потеря EtherCAT линка / останов обновления PDO,
  2) зависание COMX (нет обновлений process image),
  3) out-of-order/скачки `seq`,
  4) пауза EtherCAT master/ПК на 50–200 мс,
  5) неконсистентный снапшот/битые поля (reserved!=0 / неверная версия),
  6) обрыв USB-UART в режиме emu (если включён).
  - Ожидаемое: safe stop/запрет сварки по политике, latch/recovery без авто-рестарта, корректные счётчики/флаги, отсутствие влияния на fast loop.

### Rollback
- Feature flags:
  - отключить EtherCAT как источник команд (оставить систему в safe state / fallback на CAN если предусмотрено),
  - отключить `TkPdo.Emu.*` по UART.
- Runtime rollback при сомнениях:
  - принудительно `allow=false`/`enable=0` + выставить диагностический fault/статус “link timeout / cmd invalid”.
- Любой rollback не меняет аппаратный shutdown-path и не ослабляет latched fault политику.

## 8) Status / Implementation links
- Status: draft | accepted | implemented | obsolete
- Links: TBD (PR/commit/issue)

