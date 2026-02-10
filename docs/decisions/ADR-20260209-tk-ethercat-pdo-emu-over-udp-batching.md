# ADR-20260209 — Имитация EtherCAT PDO: PCcom4 по USB-UART (stream)

Статус: draft  
Дата: 2026-02-09  
Владелец: Bespalov  
Связано: `docs/decisions/ADR-20260210-tk-interface-ethercat-comx-fmc-and-uart-pdo-emu.md`, TBD (issue/PR)  

---

## 1) Context / Problem
- Нужно сделать сервисный интерфейс по **USB-UART (PCcom4)** для:
  - имитации циклического управления “как EtherCAT PDO” (best effort + watchdog) с ПК под Windows.
- Контекст проекта: см. `docs/CONTEXT_SNAPSHOT.md` (двухдоменная архитектура; обмен с ТК 1 мс как целевой на штатном интерфейсе; наблюдаемость DBG0/DBG1).
- Ограничения/инварианты (нельзя сломать):
  - аппаратный safe state и shutdown-path (TIM1 BKIN/BKIN2) остаются неизменными (см. `docs/PROJECT_CONTEXT.md`, `docs/ARCHITECTURE.md`).
  - потеря/невалидность “команд управления” по сервисному каналу ⇒ безопасный останов/запрет сварки по watchdog; без авто-рестарта.
  - сервисный обмен не должен блокировать fast loop/критичные ISR; при перегрузке допустим дроп/флаг overflow, но не деградация управления “вслепую”.
  - протокольная модель PCcom4 (Node/Op/CRC16) сохраняется; расширения фиксируются в проектном профиле `docs/protocols/PCCOM4.02_PROJECT.md`.
- Триггеры пересмотра:
  - если потребуется жёсткая периодика ~1 мс на Windows (best effort перестаёт устраивать),
  - если потребуется строгая синхронизация по типу EtherCAT DC/SYNC,
  - если объёмы телеметрии потребуют иной схемы буферизации/транспорта (например, не через USB-UART).

## 2) Decision (что выбрали)
- Выбранный вариант: **Вариант A** — “EtherCAT PDO-like” как **PCcom4 по USB-UART (stream)**, при этом:
  - имитация обмена с ТК/“PDO” делается через узел проекта `Node = 0x03`, но **все упоминания CAN в профиле/кодовых именах должны быть заменены на EtherCAT PDO** (например: “Имитация обмена с ТК по EtherCAT PDO”).
- Scope: фиксируем транспорт (PCcom4 stream по USB-UART) и привязку payload к “канонике” EtherCAT: `docs/protocols/PROTOCOL_TK_ETHERCAT.md` (RxPDO `CMD_WELD` 16 байт, TxPDO `FB_STATUS` 48 байт, (опц.) `FAULT` 16 байт). Расширения/счётчики фиксируются в `docs/protocols/PCCOM4.02_PROJECT.md` / 3.2.

## 3) Options considered (варианты)
### Вариант A
- Идея:
  - PCcom4 как контейнер, USB-UART как транспорт (stream).
  - `Node = 0x03` используется как “псевдо-PDO” (имитация EtherCAT PDO):
    - Host→Device: `TkPdo.Emu.CmdWeld` (Type=0x02, DataLen=16),
    - Device→Host: `TkPdo.Emu.FbStatus` (Type=0x02, DataLen=48),
    - (опционально) Device→Host: `TkPdo.Emu.Fault` (Type=0x02, DataLen=16).
- Плюсы:
  - Максимальная совместимость со стеком `protocol_core`/профилем PCcom4 проекта: не плодим новый “мини-протокол”.
  - Форматы легко маппятся в реальный EtherCAT PDO в будущем (те же поля/структуры).
- Минусы/риски:
  - UART stream = возможны потери/шум/ресинхронизация; требуется валидатор, watchdog и метрики качества.

### Вариант B
- Идея:
  - Отдельный “лёгкий бинарный протокол” поверх UART для псевдо-PDO/телеметрии (без PCcom4-кадров).
- Плюсы:
  - Минимальный overhead, максимально прямые структуры.
- Минусы/риски:
  - Дублирование командной модели/CRC/версий, выше риск расхождения с протокольным слоем проекта и инструментами.

## 4) Decision criteria (критерии выбора)
- Совместимость с существующей моделью PCcom4 проекта (Node/Op/CRC16, профили, tooling).
- Явная управляемость потерь/джиттера (seq + метрики + watchdog).
- Safety: потеря команд ⇒ предсказуемый safe stop.
- Простота верификации (измеримость на железе и воспроизводимость на ПК).

## 5) Rationale (почему так)
- Выбран Вариант A, потому что он:
  - использует уже существующий узел `Node = 0x03` как контейнер для псевдо-PDO (имитации EtherCAT PDO) и требует лишь привести наименования/кодовые имена в соответствие EtherCAT PDO, убрав упоминания CAN;
  - остаётся в рамках правил `docs/protocols/PCCOM4.02.md` (stream parser, CRC16, валидаторы длины) и не вводит второй параллельный протокол.
- Вариант B отвергнут, потому что создаёт второй параллельный протокол и ухудшает сопровождаемость/совместимость.

## 6) Consequences / Risks
- Позитивные последствия:
  - Один “семейный” протокол PCcom4 для сервиса (управление/статус/метрики/осциллограф) по USB-UART.
- Негативные последствия/долг:
  - Нужен устойчивый stream parser (ресинхронизация, таймауты, защита от мусора в потоке).
  - Нужна строгая политика перегрузки (drop + counters), чтобы сервисный обмен не влиял на управление.
- Риски отказа/edge cases и как их обнаруживаем:
  - Потеря/задержка кадров: счётчики `rx_missed`, `rx_out_of_order`, `watchdog_trip`.
  - Перегрузка scope: `scope_dropped_samples`, `scope_tx_overflow`, флаг overflow в статусе.
  - “Подвисание” ПК/пауза приложения: watchdog с переходом в safe stop; лог/счётчик срабатываний.

## 7) Interfaces / Data / Timing impact
- Транспорт: USB-UART (FT232H), поток байт по правилам PCcom4 (см. `docs/protocols/PCCOM4.02.md` / раздел 8 “Требования к реализации парсера (stream)”).
  - `PREAMBLE=0xFF` не уникален ⇒ требуется ресинхронизация по `Length`+CRC.
  - Любые “хвосты”/битые CRC учитываются в счётчиках качества связи.
- Адреса PCcom4: как в `docs/protocols/PCCOM4.02_PROJECT.md` (ПК=`0x01`, УСПФ=`0x03`).
- Имитация “PDO” через узел `Node = 0x03` (переименованный на “Имитация обмена с ТК по EtherCAT PDO”):
  - `TkPdo.Emu.CmdWeld` (`Op=0x01`, `Type=0x02`, `DataLen=16`) — Host→Device:
    - payload `CMD_WELD` как в `docs/protocols/PROTOCOL_TK_ETHERCAT.md` / 3.4.
  - `TkPdo.Emu.FbStatus` (`Op=0x02`, `Type=0x02`, `DataLen=48`) — Device→Host:
    - payload `FB_STATUS` как в `docs/protocols/PROTOCOL_TK_ETHERCAT.md` / 4.1.2.
  - (опционально) `TkPdo.Emu.Fault` (`Op=0x03`, `Type=0x02`, `DataLen=16`) — Device→Host:
    - payload `FAULT` как в `docs/protocols/PROTOCOL_TK_ETHERCAT.md` / 4.3.
- Watchdog / таймаут:
  - Когда `CMD_WELD.enable=1`, отсутствие валидного `CMD_WELD` дольше `soft/hard-timeout` из `docs/protocols/PROTOCOL_TK_ETHERCAT.md` ⇒ controlled stop / запрет сварки + отражение в `FB_STATUS` (`COMMS_*_TIMEOUT_ACTIVE`, `fault_word.COMMS_TIMEOUT_HARD` по политике).
  - Для dev-режима допускается параметр `TkPdo.Emu.LinkTimeoutMs` (см. `docs/protocols/PCCOM4.02_PROJECT.md` / 3.3, `TkPdo.Emu.LinkTimeoutMs`) как настройка hard-timeout (Draft).
- Метрики качества связи (проектное расширение узла `Node=0x03`):
  - `TkPdo.Emu.Stats` (`Op=0x10`, чтение): счётчики `rx_ok`, `rx_crc_err`, `rx_out_of_order`, `rx_missed`, `watchdog_trip`, `pdo_age_max_us`, `last_seq`, `last_rtt_us` (формат Data фиксируется проектом).
- Осциллограф:
  - Использовать уже описанный узел `Node = 0x06` (`Scope.*`) поверх того же PCcom4 по USB-UART.
  - При перегрузке USB-UART: допускается дроп/децимация + счётчики, но не блокировка fast loop.

## 8) Tests / Proof / Evidence / Rollback
### Tests / Proof / Evidence
- Unit/host:
  - Stream parser PCcom4 по UART: частичные кадры, ресинхронизация по `0xFF`, валидация `Length`, CRC16 Modbus RTU (как в `docs/protocols/PCCOM4.02.md`).
  - Валидатор фиксированных `DataLen` для `TkPdo.Emu.*`.
- SIL:
  - Модель “приход `CMD_WELD` с `seq` → обновление команды → генерация `FB_STATUS` с `seq_applied`”, плюс watchdog при пропуске/невалидности.
- On-target измерения (GPIO/осциллограф/trace):
  - GPIO: `UART_RX_ACTIVITY`, `EMU_CMD_VALID`, `EMU_CMD_APPLIED`, `WATCHDOG_TRIP`, `SCOPE_TX_BUSY`.
  - Измерить:
    - распределение периода прихода `EMU_CMD_VALID` (джиттер Windows),
    - задержку `EMU_CMD_VALID → EMU_CMD_APPLIED` (в пределах slow loop),
    - максимальный “возраст команды” `pdo_age_max_us`.
- HIL/bench:
  - Fault-injection (минимум 5):
    1) отключение/обрыв USB-UART,
    2) пауза PC приложения на 50–200 мс,
    3) out-of-order seq,
    4) переполнение RX (бурст кадров),
    5) битые CRC кадра / мусор в потоке.
  - Ожидаемое: safe stop по таймауту, корректные счётчики/флаги, отсутствие влияния на fast loop и аппаратный shutdown-path.

### Rollback
- Отключить `TkPdo.Emu.*` по USB-UART как функцию (feature flag) и вернуться к штатному интерфейсу управления (EtherCAT PDO по целевому плану) без изменения safety-политики и fast loop.

## 9) Status / Follow-ups
- Status: draft | accepted | implemented | obsolete
- Follow-ups: TODO список задач (если есть)
  - Синхронизировать `docs/protocols/PCCOM4.02_PROJECT.md` / 3.2 с `docs/protocols/PROTOCOL_TK_ETHERCAT.md` (payload `CMD_WELD` / `FB_STATUS` / `FAULT`).
  - Уточнить/зафиксировать проектные константы валидатора (например `I_ref_max_mA`, default `max_slew_rate_default_A_ms`) и правила “валидная команда” (если потребуется — править `docs/protocols/PROTOCOL_TK_ETHERCAT.md`).
  - Зафиксировать целевой период для Windows (например 5 мс) и первичное значение watchdog (например 20 мс) на основе измерений.
- Links: PR/commit/issue
