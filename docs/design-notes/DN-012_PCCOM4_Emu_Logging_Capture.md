# DN-012_PCCOM4_Emu_Logging_Capture — PDO emu + Logging + RAW Capture по FT232H/PCcom4

## 1) Статус
- **Статус:** Draft
- **Область:** сервисный интерфейс USB-UART (FT232H, PCcom4), observability/capture
- **Связанные источники правды:** `docs/PROJECT_CONTEXT.md`, `docs/GLOSSARY.md`, `docs/ENGINEERING_CONTRACT.md`, `docs/ARCHITECTURE.md`, `docs/SAFETY.md`, `docs/TEST_PLAN.md`, `docs/protocols/PCCOM4.02.md`, `docs/protocols/PCCOM4.02_PROJECT.md`, `docs/protocols/PROTOCOL_TK_ETHERCAT.md`

## 2) Контекст и проблема
Нужно обеспечить через один физический канал FT232H + PCcom4:
1. Временную эмуляцию обмена EtherCAT PDO (`TkPdo.Emu.*`) для управления/обратной связи, пока нет полноценного EtherCAT со стороны ТК.
2. Выгрузку внутренних переменных контура управления для настройки/диагностики.
3. Выгрузку RAW данных АЦП (прежде всего AD7380) с захватом окна pre/during/post по trigger.

Ограничения:
- fast-домен (PWM 1–4 кГц) — детерминированный, без RTOS;
- slow-домен (FreeRTOS) — протокол/логирование/сервис;
- нельзя ухудшить путь безопасного отключения (BKIN/BKIN2) и нельзя вносить недетерминизм в fast loop.

## 3) Цели и non-goals

### 3.1) Цели
- Реализовать устойчивый и наблюдаемый канал `TkPdo.Emu.*` поверх PCcom4.
- Добавить управляемый поток внутренних контурных переменных.
- Добавить trigger-based RAW capture с выгрузкой чанками.
- Формализовать поведение при перегрузе UART (degrade observability, не degrade control/safety).

### 3.2) Non-goals
- Не заменять EtherCAT production-обмен сервисным каналом.
- Не делать постоянный непрерывный RAW стрим на максимальной частоте как обязательный режим.
- Не выполнять UART/QSPI/CRC/сериализацию в fast-домене.
- Не изменять аппаратный shutdown-path и политику latch/recovery.

## 4) Decision

### 4.1) Трёхканальная модель
- **Канал A (PDO emu):** `Node=0x03` (`TkPdo.Emu.CmdWeld/FbStatus/Fault/Stats`).
- **Канал B (Control vars stream):** расширение `Node=0x06` (decimated stream внутренних переменных).
- **Канал C (RAW capture):** расширение `Node=0x06` (trigger-window + chunked readout).

### 4.2) Приоритеты транспортного планировщика (обязательно)
- **P0 (highest):** `PDO emu` (`TkPdo.Emu.CmdWeld/FbStatus/Fault`) и критичные служебные ответы, влияющие на актуальность команд.
- **P1:** поток контурных переменных (B).
- **P2:** выгрузка RAW capture чанками (C).

Правило обслуживания очередей:
- пока есть кадры P0, кадры P1/P2 не вытесняют P0;
- при дефиците полосы/буферов сначала деградирует P2, затем P1;
- деградация observability допустима, деградация command path недопустима.

### 4.3) Базовый режим capture
- Принят режим **Hybrid pre/post**:
  - pre-window из кольцевого буфера;
  - post-window после trigger;
  - выгрузка по запросу чанками;
  - live RAW stream допускается только с decimation и backpressure.
- Окна по умолчанию: `pre = 5 мс` (фиксировано), `post = 200 мс` (минимум).
- Буфер SRAM по умолчанию: **20 КБ**.
- Если окно не помещается: уменьшаем `post` до вместимости; если не хватает даже на `pre` → `capture_arm` отклоняется (`NO_SPACE`, счётчик).
- Повторные trigger в `CAPTURING` **игнорируются**.

### 4.4) Единая метка времени (SvcTs v1)
- Для всех Device→Host сообщений узлов `0x03` и `0x06` добавляется заголовок **SvcTs v1**:
  - `ts_us` (u32, 1 МГц, монотонно; wrap-around допустим);
  - `pwm_period_cnt` (u32, счётчик периодов ШИМ; wrap-around допустим).
- Источник `ts_us`: **TIM5**, настроенный на 1 МГц.

## 5) Rationale (почему так)
- Один канал FT232H ограничен по полосе и чувствителен к burst-нагрузке.
- Для safety/RT `CmdWeld/FbStatus` должны обрабатываться раньше любой телеметрии.
- Trigger-window даёт высокую диагностическую ценность без постоянной перегрузки канала.
- Разделение fast/slow доменов соответствует `ENGINEERING_CONTRACT` (E3/E4/E5) и снижает риск нарушения детерминизма.

## 6) Влияние на интерфейсы и данные

### 6.1) Что не меняется
- Базовый фрейминг PCcom4 (`PREAMBLE=0xFF`, `Length`, `CRC16`).
- Семантика payload `TkPdo.Emu.*` (привязка к `PROTOCOL_TK_ETHERCAT`).

### 6.2) Что добавляется/уточняется в `PCCOM4.02_PROJECT`
- Для `Node=0x06` фиксируются операции управления сервисным потоком/захватом:
  - `Scope.StreamControl`
  - `Scope.SignalMask`
  - `Scope.Decimation`
  - `Scope.TriggerConfig`
  - `Scope.CaptureArm`
  - `Scope.CaptureRead`
  - `Scope.CaptureAbort`
  - `Scope.CaptureMeta`
  - `Scope.Stats`
- Для `Node=0x03` явно фиксируется приоритет P0 и правила при перегрузе.
- Вводится единый заголовок **SvcTs v1** для Device→Host сообщений `Node=0x03/0x06`.
- RAW v1 минимальный; расширенные метаданные выдаются отдельной командой `Scope.CaptureMeta`.
- Добавляются счётчики наблюдаемости и деградации:
  - `cnt_p1_drop`, `cnt_p2_drop`
  - `q0/q1/q2_highwater`
  - `capture_truncated_count`
  - `parser_resync_count`
  - `rx_crc_err`, `rx_out_of_order`, `watchdog_trip`

## 7) Влияние на тайминги/RT/safety
- Fast loop не выполняет тяжёлые операции и работает только с O(1) push/snapshot в SRAM.
- ISR остаются минимальными (notify/flag), heavy processing в task-контексте.
- Safety-path (BKIN/BKIN2, запрет драйверов, latch policy) не изменяется.
- Любой overrun/overflow сервиса должен быть наблюдаемым и не должен разрешать энергетику.

## 8) Риски и edge cases

### R1) Saturation UART/FT232H
- Риск: рост latency на сервисном канале.
- Митигатор: strict priority P0>P1>P2, backpressure, decimation, drop counters.

### R2) Parser desync / CRC burst
- Риск: частичная потеря управления сервисными командами.
- Митигатор: robust stream parser (resync по `0xFF` + `Length` + CRC), диагностические счётчики.

### R3) Overflow pre/post buffers
- Риск: неполное capture-окно.
- Митигатор: статус `truncated`, bounded ring, корректная метаинформация окна.

### R4) Starvation нижних потоков
- Риск: недовыгрузка observability.
- Митигатор: ограничение нагрузки, адаптивный decimation, сигнализация о деградации.

### R5) Wrap-around SvcTs
- Риск: переполнение `ts_us`/`pwm_period_cnt` и неверные расчёты на ПК.
- Митигатор: документированный wrap-around + расчёт разностей по модулю.

### R6) Недостаточный буфер SRAM
- Риск: невозможность гарантировать окно capture.
- Митигатор: уменьшение `post`, либо отказ `capture_arm` при нехватке под `pre`.

## 9) Test plan / Proof

### 9.1) Host unit (L1)
- Парсер PCcom4: `Length` диапазон, CRC mismatch, partial frame, burst `0xFF`, resync.
- Политика `seq`: first/repeat/backward/gap/wrap-around.
- Валидация `Node=0x06` конфигурации: mask/decimation/trigger/pre/post/ranges.
- Capture FSM: `IDLE -> ARMED -> CAPTURING -> FROZEN -> READOUT`, abort-path.
- Scheduler policy: при конкуренции трафика P0 всегда обслуживается первым; P1/P2 корректно деградируют.
- `SvcTs v1`: монотонность и wrap-around для `ts_us`/`pwm_period_cnt`.
- `Scope.CaptureMeta`: согласованность `total/pre/post` с фактическим окном, корректный `post`-reduce.

### 9.2) On-target (L3)
- Измерения на `DBG0/DBG1` и обязательных точках:
  - `CTRL_TICK`, `ADC_START`, `ADC_READY`, `PWM_APPLY`, `FAULT_ENTRY`
  - `BKIN_RAW`, `PWM_OUT`
- Сравнение baseline (`service OFF`) vs (`service ON`):
  - `control_tick` jitter/worst-case;
  - `cnt_ctrl_overrun`;
  - latency по `CmdWeld` в slow loop.

### 9.3) HIL/fault-injection (L4, минимум 5)
1. CRC burst ошибок в входном PCcom4 потоке.
2. Потеря/пауза `CmdWeld` до soft/hard timeout.
3. `seq` повтор/назад/пропуск.
4. Trigger storm + queue pressure.
5. Переполнение буфера capture/readout.
6. Ошибка backend storage (timeout/fault) во время выгрузки.
7. Некорректная конфигурация capture (out-of-range).
8. Обрыв сервис-канала в середине readout.
9. Недостаточный буфер SRAM → `post` уменьшается и отражается в `CaptureMeta`.

### 9.4) Критерии приёмки
- `PDO emu` реально обслуживается первым под нагрузкой.
- Нет регрессии shutdown latency `BKIN_RAW -> PWM_OUT`.
- Нет ухудшения fast-loop детерминизма сверх зафиксированного порога.
- Все деградации observability отражаются счётчиками/флагами.
- `SvcTs v1` присутствует во всех Device→Host сообщениях `Node=0x03/0x06`.

## 10) Rollback
- Feature flags:
  - `TKPDO_EMU_EN`
  - `SCOPE_STREAM_EN`
  - `RAW_CAPTURE_EN`
- Порядок rollback:
1. Выключить `RAW_CAPTURE_EN`.
2. Выключить `SCOPE_STREAM_EN`.
3. Оставить только `TKPDO_EMU_EN`.
4. При необходимости вернуть профиль к `Node=0x03` без расширений `Node=0x06`.

Критерий успешного rollback:
- service возвращён к режиму `PDO emu only`;
- safety/RT метрики не ухудшены относительно baseline.

## 11) План внедрения
1. Обновить `docs/protocols/PCCOM4.02_PROJECT.md` (Node=0x03/0x06, приоритеты, counters).
2. Обновить `docs/TEST_PLAN.md` (сценарии A/B/C и критерии измерений).
3. Реализовать protocol/logging/capture core в `Fw/*` (без `Core/*` на этом этапе).
4. Добавить host unit tests для parser/seq/scheduler/capture.
5. Выполнить on-target измерения и зафиксировать evidence.

## 12) Открытые вопросы (TBD)
- Авто-определение свободной SRAM и динамический выбор размера буфера.
- Политика saturating vs wrap-around для отдельных статистических счётчиков.

## 13) Доп. детализация Logging/Capture Core

### 13.1) Компоненты
1. **`capture_frontend_fast` (fast context):**
- вход: готовые выборки/события из fast pipeline;
- действие: O(1) запись в SPSC ring в SRAM;
- запреты: UART/QSPI/CRC/форматирование/блокировки.

2. **`trace_stream_core` (slow context):**
- выбор сигналов по `signal_mask`;
- прореживание по `decimation`;
- упаковка records для `Scope.Data`.

3. **`capture_core` (slow context):**
- FSM захвата (`IDLE`, `ARMED`, `CAPTURING`, `FROZEN`, `READOUT`, `ABORTED`);
- управление pre/post окнами;
- выдача чанков `capture_id + offset + len`.

4. **`capture_storage_if`:**
- backend-абстракция SRAM/PSRAM;
- API: `reserve/write/read_chunk/get_meta`;
- ошибки: `NO_SPACE`, `TIMEOUT`, `BACKEND_FAULT`.

5. **`service_tx_scheduler`:**
- три очереди `Q0(P0)`, `Q1(P1)`, `Q2(P2)`;
- детерминированный budget на тик;
- backpressure и обновление счётчиков.

### 13.2) Контракты API (черновые)
- `capture_arm(cfg)` — валидация конфигурации + переход в `ARMED`.
- `capture_on_trigger(trigger_ts)` — фиксация trigger и старт post-window.
- `capture_poll(now_ts)` — завершение окна и перевод в `FROZEN`.
- `capture_read(capture_id, offset, req_len, out_chunk)` — чтение чанка.
- `stream_set_mask(mask)` / `stream_set_decimation(n)` / `stream_enable(on)`.

### 13.3) Модель памяти
- **SRAM ring:** быстрый pre-buffer и staging; дефолтный объём 20 КБ; `post` может уменьшаться для вмещения.
- **Storage backend (опционально PSRAM):** хранение завершённых capture-окон.
- **Meta block:** отдельный заголовок и выдача метаданных через `Scope.CaptureMeta`.

### 13.4) Политика backpressure
- рост `Q2` -> уменьшение chunk размера/частоты, затем дропы `Q2` (`cnt_p2_drop++`);
- рост `Q1` -> auto-decimation вверх, затем дропы `Q1` (`cnt_p1_drop++`);
- `Q0` обслуживается first и не должен страдать из-за `Q1/Q2`.

### 13.5) Trigger policy
- Источники trigger: `manual`, `weld_start`, `fault_entry`.
- Приоритет trigger: `fault_entry > weld_start > manual`.
- Повторные trigger в `CAPTURING`:
  - игнор.

### 13.6) Диагностические метрики (минимум)
- парсер: `rx_ok`, `rx_crc_err`, `parser_resync_count`;
- scheduler: `q0/q1/q2_highwater`, `cnt_p1_drop`, `cnt_p2_drop`;
- capture: `capture_count`, `capture_truncated_count`, `capture_backend_fault_count`;
- comms freshness: `pdo_age_max_us`, `watchdog_trip`.

### 13.7) Инварианты корректности
- Любая ошибка logging/capture не влияет на разрешение сварки.
- Ошибки backend/readout приводят к деградации observability, но не к блокировке slow loop.
- Сервисная подсистема не изменяет shutdown-path и не обходит safety gating.
