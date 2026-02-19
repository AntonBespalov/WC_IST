# Руководство по механизму логирования/захвата (PCcom4 Scope)

Этот документ описывает **как устроен и как использовать** механизм логирования/захвата, реализованный в прошивке.
Решения и терминология основаны на `DN-011_Service_Logging_Capture_Framework.md` и
`DN-012_PCCOM4_Emu_Logging_Capture.md` (см. `docs/design-notes/`).

Ключевой контекст по RT/Safety: см. `docs/PROJECT_CONTEXT.md` / разделы 1, 2, 5  
Термины: см. `docs/GLOSSARY.md` / разделы 1–3

---

## 1) Область применения и границы
- Механизм предназначен для **slow‑домена** (FreeRTOS task), не для fast‑контуров управления (см. `PROJECT_CONTEXT.md` / раздел 1).
- Логирование не должно влиять на PWM/силовую часть; любой риск влияния → измерять джиттер и время ISR/тика.
- Безопасное состояние и аппаратный shutdown‑path остаются независимыми от логирования (см. `PROJECT_CONTEXT.md` / раздел 2 и 4).

---

## 2) Архитектура: модули и роли

**Core‑логика (платформо‑независимая):**
- `Fw/logging/capture_core.*` — FSM захвата (pre/post, trigger, freeze, readout), расчёт окна, метаданные.
- `Fw/logging/capture_storage_sram.*` — кольцевое хранилище SRAM для RAW.
- `Fw/logging/trace_stream_core.*` — поток Control vars (enable/mask/decimation/seq).
- `Fw/logging/svc_ts.*` — метка времени SvcTs v1 (ts_us + pwm_period_cnt).
- `Fw/protocol/pccom4/*` — parser/encoder/service PCcom4 для команд и выдачи потоков.
- `Fw/protocol/service_tx_scheduler.*` — очереди P0/P1/P2 + бюджет логов (байт/тик).

**Port‑слой (HAL/RTOS):**
- `Fw/port/uart_service.*` — DMA RX/TX, ring/queue, HAL callbacks.
- `Fw/port/app_task.*` — task‑loop (1 мс), интеграция сервисов.
- `Core/Src/main.c` — только init + старт задач.

### 2.1. Приоритеты P0/P1/P2 (кадр → приоритет)
| Приоритет | Кадры/потоки | Узел/операция |
|---|---|---|
| P0 | `TkPdo.Emu.CmdWeld/FbStatus/Fault` | `Node=0x03` |
| P1 | Control vars stream | `Node=0x06`, `MESSAGE op=0x10+dataset_id` (`dataset_id=0x01`) |
| P1 | EtherCAT‑лог‑стрим | `Node=0x06`, `MESSAGE op=0x10+dataset_id` (`dataset_id=0x03`) |
| P2 | RAW capture chunk | `Node=0x06`, `MESSAGE op=0x10+dataset_id` (`dataset_id=0x02`) |

---

## 3) Потоки данных (кратко)

**RAW capture:**
1) `capture_core_on_sample()` получает сэмплы (fast‑домен).
2) `capture_core_on_trigger()` фиксирует момент trigger и запускает post‑окно.
3) После завершения post‑окна `capture_core` замораживает буфер.
4) `pccom4_service_emit_raw_chunk()` читает чанки и отдаёт по PCcom4.

**Control vars stream:**
1) `trace_stream_core_poll()` решает, выдавать ли кадр (decimation/seq).
2) `pccom4_service_emit_trace_stream()` формирует payload и отправляет.

**PCcom4:**
1) RX байты → `pccom4_parser` → команды `pccom4_service`.
2) Ответы/стримы → `service_tx_scheduler` → UART TX.

---

## 4) Временная метка SvcTs v1

Формат (8 байт, little‑endian):
- `ts_us` — монотонное время от 1 МГц таймбазы, [мкс].
- `pwm_period_cnt` — счётчик периодов PWM, [периоды].

Источники времени:
- `svc_ts_set_sources(read_us, read_pwm)` задаёт функции чтения.
- Если `read_pwm == NULL`, используется внутренний счётчик `svc_ts_pwm_period_tick()`.

Примечание: `ts_us` — 32‑битный, переполнение неизбежно; хост обязан учитывать wrap‑around.

---

## 5) Конфигурация

Глобальные дефолты — `Fw/logging/logging_cfg.h`:
- `MFDC_SVC_TS_HZ` — частота таймбазы (по умолчанию 1 МГц).
- `MFDC_CAPTURE_*_DEFAULT` — pre/post, sample_rate, sample_bytes, mask, buffer_bytes.
- `MFDC_SVC_TX_Q*` — ёмкости очередей P0/P1/P2.
- `MFDC_UART_*_FRAMES_PER_TICK_DEFAULT` — лимиты кадров за тик (0 = без ограничений).

Runtime‑настройка:
- `app_task_set_uart_limits(rx, tx)` — задать лимиты кадров на тик (например 1/3).
- `pccom4_service_set_tx_budget()` — лимит бюджета логов (P1/P2) в байтах на тик.

### 5.1. Что такое «бюджет логов» и зачем он нужен
Бюджет — это **лимит байт на тик** для очередей логирования `P1/P2`
(Control vars и RAW). Он **не** ограничивает `P0` (ответы на команды).

Как работает:
- если бюджет выключен (`budget_bytes = 0`) — выдача из `P1/P2` без ограничений;
- если бюджет включён — кадр `P1/P2` выдаётся только когда в бюджете хватает `len`;
  после выдачи бюджет уменьшается на `len`;
- каждый тик `service_tx_scheduler_on_tick()` добавляет `budget_bytes` к текущему бюджету;
  переполнение saturates до `UINT32_MAX`.

Зачем:
- ограничить долю канала, занимаемую логами, чтобы **не душить** ответы `P0`;
- сделать выдачу логов более предсказуемой по времени (не «заливать» канал RAW).

### 5.2. Пример расчёта бюджета
Допустим, UART 115200 бод, 8N1 ⇒ ~10 бит на байт, эффективная скорость ~11 520 байт/с.
При тике 1 мс это ≈ **11–12 байт/тик**.

Для высоких скоростей (8N1, 1 мс тик):
- **921 600 бод** → ~92 160 байт/с → **~92 байта/тик**
- **3 Мбод** → ~300 000 байт/с → **~300 байт/тик**
- **5 Мбод** → ~500 000 байт/с → **~500 байт/тик**
- **10 Мбод** → ~1 000 000 байт/с → **~1000 байт/тик**

Если хотим отдать логам не более ~30% канала:
- бюджет ≈ `0.3 * 11` ≈ **3 байта/тик** (округлить до 3–4).

Если хотим отдать логам 50%:
- бюджет ≈ **5–6 байт/тик**.

Реально имеет смысл брать больше (например 16–32 байт/тик), если:
- частота тика выше 1 мс, либо
- физический линк быстрее (например 230400 бод), либо
- лог‑данные небольшие и допускают burst‑выдачу.

---

## 6) Протокол PCcom4 (Scope/Logging) — текущая реализация

### 6.1. Общие правила
- Узел Scope: `node = 0x06`.
- Адреса `device/host` берутся из последнего валидного запроса; без запроса стримы не отправляются.
- CRC: CRC16 Modbus (IBM), little‑endian.

### 6.2. Управление стримами (Scope.StreamControl)
Команда `READ/WRITE`, `op = dataset_id`:
- `dataset_id = 0x01` — Control vars stream
- `dataset_id = 0x02` — RAW capture stream
- `dataset_id = 0x03` — EtherCAT‑лог‑стрим

READ:
- `data_len = 0`
- ответ: `READ_OK` с 1 байтом состояния (0/1)

WRITE:
- `data_len = 1`, `data[0] != 0` → enable
- для RAW при enable вызывается `capture_core_arm()`; при ошибке → `WRITE_ERR`

### 6.3. Метаданные захвата (Scope.CaptureMeta)
Команда `READ`, `op = 0x20`, `data_len = 0`.  
Ответ: 36 байт, соответствует `capture_meta_t`:
- `raw_fmt_ver` (u16), `state` (u16), `capture_id` (u32),
  `sample_rate_hz` (u32), `total_samples` (u32), `pre_samples` (u32),
  `post_samples` (u32), `decimation` (u16), `sample_bytes` (u16),
  `channels_mask` (u16), `flags` (u16), `buffer_bytes` (u32).

### 6.4. Data‑стримы (MESSAGE)
Тип кадра: `MESSAGE (0x02)`, `op = 0x10 + dataset_id`.

**Control vars stream** (`op = 0x11`):
- Payload: `SvcTs v1 (8)` + `dataset_id (1)`  
  Сейчас дополнительных полей нет — это задел для расширения.

**EtherCAT‑лог‑стрим** (`op = 0x13`):
- Payload: `SvcTs v1 (8)` + `Rx PDO (16)` + `Tx PDO (48)`

**RAW chunk** (`op = 0x12`):
- Payload:
  - `SvcTs v1 (8)`
  - `dataset_id (1)`
  - RAW header (14):
    - `raw_fmt_ver` (u16)
    - `capture_id` (u32)
    - `sample_rate_hz` (u32)
    - `offset_samples` (u32)
  - Далее — массив сэмплов.

RAW v1 (текущая реализация):
- один сэмпл = `I_weld` (int16 LE) + `U_weld` (int16 LE).

---

## 7) Поведение FSM захвата
- `capture_core_arm()` рассчитывает pre/post в сэмплах и проверяет ёмкость.
  - если `pre` не помещается → `NO_SPACE` и `ABORTED`
  - если окно превышает ёмкость → `POST_REDUCED`
- `capture_core_on_sample()` пишет в SRAM‑кольцо, пока не заморожено.
  - при несоответствии `bytes_per_sample` текущему формату — `TRUNCATED`.

---

## 8) Инициализация и базовый запуск

### 8.1. Что и где инициализируется
Инициализация выполняется в `app_task_start()`/`app_main_task()`:
- запуск таймбазы (`TIM5`), затем `svc_ts_init()` и `svc_ts_set_sources(read_us, read_pwm)`;
- `uart_service_init()` — старт RX DMA и подготовка буферов;
- `pccom4_service_init()` + `pccom4_service_set_tx_budget()`;
- запуск task‑цикла (1 мс), в котором вызываются `pccom4_service_on_tick()`,
  приём байт из UART и отправка кадров.

Если нужен корректный `pwm_period_cnt` в SvcTs:
- либо передать `read_pwm` в `svc_ts_set_sources()`,
- либо вызывать `svc_ts_pwm_period_tick()` **раз в период PWM** (обычно в ISR события периода).

### 8.2. Лимиты (опционально)
   - `app_task_set_uart_limits(1u, 3u)` — 1 кадр RX и 3 кадра TX на тик.

### 8.3. Подключение хоста
   - Отправить любой валидный PCcom4 запрос, чтобы зафиксировать адреса.

### 8.4. Включить стримы
   - WRITE `node=0x06, op=0x01` → Control vars
   - WRITE `node=0x06, op=0x02` → RAW capture

### 8.5. Trigger
   - Trigger формируется на стороне прошивки (`capture_core_on_trigger()`).
   - Хост получает RAW чанки после freeze.

### 8.6. Метаданные
   - READ `node=0x06, op=0x20` → `CaptureMeta`.

### 8.7. Быстрый старт (примеры PCcom4 в HEX)
Примеры ниже — **только для примера адресов** (`dst=0x10` устройство, `src=0x20` хост).
CRC рассчитан по Modbus (IBM), little‑endian.

- READ StreamControl (Control vars, `op=0x01`):  
  `FF 08 10 20 01 06 01 3B 84`
- WRITE StreamControl ON (Control vars):  
  `FF 09 10 20 03 06 01 01 98 61`
- READ CaptureMeta (`op=0x20`):  
  `FF 08 10 20 01 06 20 6B 8E`
- WRITE RAW ON (`op=0x02`):  
  `FF 09 10 20 03 06 02 01 98 25`

---

## 9) Как использовать Control vars stream из control_fast_loop

Сейчас Control vars stream **не содержит данных**, кроме `SvcTs v1` + `dataset_id`.
Если нужно отдавать значения из fast‑домена:
1) В fast‑loop обновлять **снэпшот** структуры (atomic/volatile, без тяжёлых операций).
2) В `pccom4_service_emit_trace_stream()` копировать снэпшот и дописывать payload.
3) Обновить формат payload на хосте (длина/декодер).

Это минимальный и безопасный путь: fast‑loop только пишет значения, slow‑loop читает и формирует кадр.

---

## 10) PSRAM backend (план расширения)

Цель: увеличить окно захвата и объём данных, **не влияя на fast‑контур** и
не выполняя QSPI/PSRAM операции в ISR/fast‑loop.

### 10.1. Предлагаемая архитектура
- **Storage‑backend** как абстракция (SRAM/PSRAM) — единый интерфейс.
- **Буфер fast→slow** в SRAM (кольцо/двойной буфер).
- **Flush‑задача** в slow‑домeне: выкачивает блоки из SRAM‑буфера в PSRAM (QSPI/DMA).
- `capture_core` остаётся владельцем FSM; меняется только backend хранения.

### 10.2. Спецификация backend‑интерфейса (без кода)
Интерфейс хранилища должен обеспечивать:
- `init(buffer, size_bytes)` — инициализация и доступный объём.
- `capacity_samples(bytes_per_sample)` — ёмкость в сэмплах.
- `push(sample_bytes, bytes_per_sample)` — запись сэмпла (fast‑домен).
- `read(start_sample, count, bytes_per_sample, out)` — чтение чанка (slow‑домен).
- `set_frozen(bool)` — запрет записи при freeze/readout.

Требования:
- `push()` **без блокировок** и без тяжёлых операций (fast‑loop).
- `read()` допускает медленные операции (slow‑loop).
- Чёткое разделение доменов доступа (fast: write, slow: read/flush).

### 10.3. План миграции (минимальный дифф)
1) Ввести абстракцию `capture_storage_iface` (vtable) и адаптировать `capture_core`
   к работе через интерфейс.
2) Оставить текущий `capture_storage_sram` как реализацию по умолчанию.
3) Добавить `capture_storage_psram` (QSPI/PSRAM), используя SRAM‑буфер для fast‑push.
4) Ввести выбор backend через конфиг `capture_core_cfg_t` (тип/указатель на iface).
5) Добавить флаг/индикатор fallback при недоступности PSRAM (см. 10.4).
6) Измерить влияние на тайминги (GPIO/осциллограф).

### 10.4. Fallback‑флаг в CaptureMeta (предлагается)
Рекомендуется зарезервировать новый бит в `CaptureMeta.flags`, например:
- `CAPTURE_META_FLAG_STORAGE_FALLBACK = (1u << 3)` — запись ушла в SRAM,
  т.к. PSRAM недоступна/ошибка инициализации.

**Важно:** это **планируемое расширение**, сейчас флаг не используется.
Если добавлять — обновить `PCCOM4.02_PROJECT.md` и описание метаданных.

### 10.5. Что измерять на железе
- Время ISR/fast‑loop до/после включения PSRAM.
- Джиттер PWM и влияние на шаг управления (см. `PROJECT_CONTEXT.md` / раздел 2).
- Пропускная способность QSPI (устойчивость flush‑задачи при max sample_rate).

---

## 11) Расширение/модификация

**Добавление новых каналов RAW:**
1) Расширить `capture_sample_t` и формат RAW v1 → v2.
2) Обновить `capture_core_on_sample()` для упаковки новых каналов.
3) Обновить хост‑парсер под новый `raw_fmt_ver`.

**Расширение Control vars stream:**
1) Добавить полезную нагрузку после `dataset_id`.
2) Обновить длину кадра и хост‑декодер.

**Изменение протокола:**
1) Обновить `pccom4_service` (op‑codes/handlers).
2) Зафиксировать изменения в `docs/protocols/PCCOM4.02_PROJECT.md`.

**RT/безопасность:**
- Любые изменения в fast‑домене — только с измерением джиттера и временем ISR.
- Аппаратный shutdown‑path должен оставаться независимым от логирования.

---

## 12) Верификация (минимум)
- Unit: `tests/unit/pccom4_logging_tests.c` (CRC, PCcom4, capture).
- On‑target: измерить длительность ISR RX и время тика `AppMainTask` (GPIO/логика).
- См. `docs/TEST_PLAN.md` и `docs/PROJECT_CONTEXT.md` / раздел 2 для требований по таймингу.
