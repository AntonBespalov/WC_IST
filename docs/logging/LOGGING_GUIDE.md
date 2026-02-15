# Fw/logging — схема модулей, потоки данных и порядок вызовов (Stage‑1, SRAM capture)

Этот документ — конспект договорённостей и объяснений по подсистеме `Fw/logging` (логгирование/«осциллограф»), чтобы новым участникам было понятно:
- как модули связаны,
- что вызывать и в каком порядке,
- как устроены pre/post trigger,
- как правильно подключать несколько источников данных (fast_loop + отдельная measurement task),
- как решить проблему «pretrigger начинается в середине записи».

> Контекст проекта: STM32 (fast‑контур без RTOS, slow‑контур на FreeRTOS). Fast‑контур должен оставаться детерминированным и не блокироваться.

---

## 1. Ментальная модель подсистемы

`Fw/logging` — это каркас отладочного захвата данных:
- **fast‑контур** делает **только snapshot** и публикует его в очередь (без упаковки/CRC/транспорта/записи в SRAM‑окно);
- **slow‑контур** потребляет snapshot’ы, **упаковывает payload** (профиль полей), **опционально считает CRC32**, пишет **Record** в SRAM capture‑окно (pre/post trigger);
- по запросу ПК окно читается **чанками** и отправляется в транспорт (USB‑UART/PCcom4 и т.п.);
- при совместном использовании UART для PDO и логов применяется `logging_tx_scheduler`, чтобы **логи не “задушили” управление**.

---

## 2. Основные сущности: Record, Capture session, Snapshot

### 2.1 Record (лог‑запись)
Базовая единица данных в SRAM‑окне:

```
[RecordHeader (16 bytes, LE)] [payload (payload_len bytes)]
```

- Заголовок фиксированный: содержит `type`, `source_id`, `payload_len`, `seq`, `timestamp`, `flags` и т.п.
- Payload переменной длины: сформирован packer’ом из snapshot по выбранному профилю (списку полей).
- CRC32 (если включено) обычно дописывается **в конец payload**, а в `flags` выставляется `HAS_CRC32`.
- Порядок полей заголовка (LE, 16 байт):  
  `magic (u16, 0xA55A)` → `type (u8)` → `flags (u8)` → `source_id (u16)` → `payload_len (u16)` →  
  `seq (u16)` → `pwm_period_count (u32)` → `pwm_subtick (u16)`.
- Примечание: `domain_id` присутствует в `logging_timestamp_t`, но в заголовок Record **не сериализуется** (Stage‑1).

### 2.2 Capture session (сессия захвата)
Сессия управляет окном pre/post trigger в SRAM:

Состояния:
- `IDLE → ARMED → TRIGGERED → STOPPED`

Смысл:
- **ARMED**: собираем **pretrigger** в кольцевой буфер (храним «последние N байт»).
- **TRIGGERED**: фиксируем начало окна и пишем **posttrigger** до `posttrigger_bytes`, затем автоматом `STOPPED`.
- **STOPPED**: можно читать окно чанками (`read_chunk`) и отдавать на ПК.

### 2.3 Snapshot
Snapshot — структура фиксированного размера, которую fast‑контур формирует 1 раз на период PWM (или по иной детерминированной точке) и публикует в очередь.

---

## 3. Модули `Fw/logging` и их роль

Ниже — “карта” модулей и кому они нужны.

### 3.1 Fast→Slow мост
- **`logging_spsc.*`** — lock‑free очередь SPSC (single‑producer/single‑consumer) для snapshots.
- **`logging_pipeline.h`** — удобные inline‑обёртки уровня “fast_publish / slow_consume / slow_pack”.

### 3.2 Упаковка данных (payload)
- **`logging_packer.*`** — упаковывает payload по профилю полей (offset+size), всё в **little‑endian**.
- **`logging_crc32.*`** — (опционально) вычисляет/дописывает CRC32 к payload.

### 3.3 Формат заголовка
- **`logging_types.h`** — типы/enum/структуры заголовков, статусы, конфиг сессии.
- **`logging_record.*`** — pack/unpack заголовка Record (16 байт, LE).

### 3.4 Сессии и SRAM‑окно
- **`logging_capture_sram.*`** — SRAM‑реализация capture‑окна с логикой pre/post trigger (в байтах).
- **`logging_core.*`** — “ядро”: arm/trigger/stop/clear, запись Record, чтение чанками, статус.

### 3.5 Арбитраж отправки (если общий UART)
- **`logging_tx_scheduler.*`** — планировщик: PDO всегда приоритетнее, логи — по бюджету (token bucket).
- **`logging_platform.h`** — хуки critical section (на таргете нужно корректно реализовать).

---

## 4. Схема связей модулей (граф потоков)

### 4.1 Основной поток данных
```
FAST (ISR / 1 раз на период PWM)
  snapshot = collect_signals()
    |
    v
  logging_spsc_push(snapshot)          // через logging_fast_publish()
    |
    v
SLOW (FreeRTOS logging task)
  while (logging_spsc_pop(snapshot)):  // через logging_slow_consume()
     payload = logging_packer_pack(snapshot, profile)
     payload = crc32_append(payload)   // опц.
     logging_core_write_record(header+payload)
        |
        v
     logging_record_pack_header(16B) + logging_capture_sram_write()
        |
        v
  SRAM capture window (pre/post trigger)
```

### 4.2 Readout (выгрузка на ПК)
```
PC requests read chunk
  logging_core_read_chunk(offset, out, max_len) -> bytes
    |
    v
transport_send(bytes)
```

### 4.3 Tx arbitration (если PDO и LOG делят UART)
```
tick (например 250us): logging_tx_scheduler_on_tick()
when UART ready:
  logging_tx_scheduler_next() -> PDO first; else LOG if budget
```

---

## 5. Порядок вызовов: что и где вызывать

Ниже — “канонический” порядок интеграции в проекте.

### 5.1 Инициализация (1 раз на старте)
1) Подготовить SRAM‑буфер capture:
- `uint8_t capture_buf[CAPTURE_BYTES];`

2) Инициализировать core/capture:
- `logging_core_init(&core, capture_buf, CAPTURE_BYTES, format_version, ...)`

3) Инициализировать SPSC очередь(и) snapshots:
- `logging_spsc_init(&q_fast, ...)`
- `logging_spsc_init(&q_meas, ...)` (если есть второй producer, см. раздел 7)

4) (опционально) инициализировать tx scheduler:
- `logging_tx_scheduler_init(&sched, cfg)`

5) Обязательно: корректно определить critical sections в `logging_platform.h` (на таргете).

### 5.2 Управление сессией (обычно из протокола/ПК)
- **ARM:** `logging_core_arm(&core, pre_bytes, post_bytes, session_id, ...)`
- **ARM:**  
  `logging_session_cfg_t cfg = { pre_bytes, post_bytes };`  
  `logging_core_arm(&core, &cfg, session_id);`
- **TRIGGER:** `logging_core_trigger(&core)`  
  (alignment‑версия возможна как future‑улучшение; см. раздел 6)
- **STOP (опционально):** `logging_core_stop(&core)`
- **CLEAR:** `logging_core_clear(&core)`

### 5.3 Fast‑контур (ISR/таймер, 1 раз на период PWM)
В fast‑контуре делаем только:
1) собрать snapshot (фикс.структура);
2) `logging_spsc_push(&q_fast, &snapshot)` (без блокировок).

Рекомендуемый fast‑оптимизатор: если сессия не `ARMED/TRIGGERED`, не публиковать snapshot’ы.

### 5.4 Slow‑контур (logging task)
В logging task:
1) обслужить команды (ARM/TRIGGER/STOP/READ);
2) “дренить” очередь(и) snapshots:
   - pop snapshot
   - pack payload (профиль)
   - (опц.) CRC32
   - write Record в core/capture

### 5.5 Readout (по запросу ПК)
Только после `STOPPED`:
- `logging_core_get_status(&core, &st)` (state/window_len/dropped)
- `logging_core_read_chunk(&core, offset, out, cap, &out_len)`  
  offset увеличивать на `out_len`, пока `out_len == 0`.

---

## 6. Pretrigger «в середине записи» — выбранное решение и опциональное улучшение

### 6.1 Почему так происходит
`logging_capture_sram` оперирует **байтами**, а не “целыми Record”.  
Pretrigger хранит “последние N байт”, поэтому при trigger окно может начаться:
- в середине payload,
- или даже в середине заголовка Record (если окно очень маленькое).

Pretrigger при этом **существует**, просто первый Record в окне может быть “обрезан”.

### 6.2 Выбранное решение: ресинхронизация на ПК + MAGIC (Stage-1)

Идея: на стороне ПК/парсера окно читается как поток байтов и выполняется **ресинхронизация** — поиск первого валидного `RecordHeader`. Всё до найденной синхронизации отбрасывается как “хвост обрезанной записи”, дальше парсинг идёт строго:
`header(16) + payload(payload_len)`.

Критерии «валидного заголовка» (минимальный набор):
- `magic == 0xA55A` — основной якорь синхронизации,
- `payload_len <= MAX_PAYLOAD` (разумный верхний предел),
- `type` в допустимом диапазоне/enum,
- `flags` не содержит неизвестных битов (если применимо),
- (опционально) если `flags` указывает CRC — проверка CRC по payload.

**MAGIC в заголовке (без изменения размера):**
- Поле `magic` в `RecordHeader` (первые 2 байта заголовка) используется как sync‑константа `0xA55A`.
- Значение выставляется при формировании заголовка (`logging_record_header_init()`).
- `logging_record_unpack_header()` возвращает ошибку, если `magic` не совпал.

Плюсы:
- Нулевые изменения в логике `capture_sram`/trigger (быстро и дёшево),
- Высокая устойчивость к обрезанным записям и частичным данным,
- Существенно меньше ложных совпадений благодаря MAGIC (и/или CRC).

Ограничение:
- Самый первый Record в окне может быть потерян, если окно началось в его середине. Это нормальная плата за байтовый pretrigger на Stage-1.

### 6.3 Опциональное улучшение (не реализовано в рамках Stage-1): aligned pretrigger на стороне прошивки
Если в будущем понадобится, чтобы окно **всегда** начиналось с начала Record (без потери первого Record), можно реализовать “aligned trigger” в прошивке:
- `logging_core` ведёт индекс стартов записей (в абсолютных байтах от ARM),
- при trigger выбирается ближайший старт записи, попадающий в доступную часть pretrigger,
- `window_start` выставляется по границе Record.

Это усложняет `logging_core`, но упрощает парсер и делает pretrigger «красивым».


---

## 7. Несколько источников данных: fast_loop + measurement_core task

### 7.1 Поддерживается ли несколько источников в одном capture?
Да. В одной сессии можно хранить записи от разных источников и различать их по:
- `type` (например, FAST_SNAPSHOT vs MEAS_SNAPSHOT),
- `source_id` (например, SRC_FAST vs SRC_MEAS).

### 7.2 Важно: `logging_spsc` — строго SPSC
`logging_spsc` поддерживает **только один producer** и **один consumer** на экземпляр очереди.

Если у тебя два независимых producer-контекста:
- `fast_loop` (ISR/таймер),
- `measurement_core` (отдельная FreeRTOS‑таска),

то пушить обоим в **одну** SPSC нельзя.

### 7.3 Рекомендованный паттерн: “много producers → много SPSC → один writer”
Схема:
- `q_fast`: producer = fast ISR, consumer = logging task
- `q_meas`: producer = measurement task, consumer = logging task

Да, это означает **2× `logging_spsc_init()`**.

Почему так лучше:
- `logging_core/capture` имеют **одного** писателя (logging task) → минимум гонок, стабильный `seq`, меньше критических секций.
- measurement task не блокирует/не замедляет writer и не создаёт гонок в SRAM‑окне.

### 7.4 Drain policy (чтобы fast не голодал)
В logging task:
- выкачивать `q_fast` с приоритетом (например, до `Nfast_max` элементов за итерацию),
- затем `q_meas` (до `Nmeas_max`),
- повторять.

Так measurement‑поток не “заливает” систему и не вызывает потери fast‑данных из‑за задержек.

---

## 8. `logging_tx_scheduler`: как не убить PDO логами

Если у тебя один UART/USB‑UART для:
- регулярного обмена (PDO/команды),
- потоковой выгрузки логов,

то нужен арбитраж.

`logging_tx_scheduler` реализует:
- **PDO всегда приоритетнее**
- LOG отправляется только если есть **бюджет** (token bucket), пополняемый на каждом тике

Интеграция:
- `logging_tx_scheduler_on_tick(&sched)` вызывать регулярно (удобно 250 мкс, совпадает с PWM периодом).
- `logging_tx_scheduler_next(&sched, iface, out, out_cap, &class, &len)` вызывать когда транспорт готов принять следующий блок.

`iface` — адаптер, который умеет:
- проверить наличие PDO/LOG в очередях,
- извлечь следующий кадр/чанк.

---

## 9. Практические грабли и рекомендации

1) **Pretrigger в байтах** ⇒ начало окна может быть в середине Record.  
   Решение: (а) ресинхронизация на ПК + MAGIC, (б) trigger‑aligned в прошивке.

2) **SPSC не MPSC**.  
   Если больше одного producer — больше одной очереди.

3) **Единый writer**.  
   Лучше держать запись в SRAM‑окно (через `logging_core_write_record*`) в одном месте (logging task), иначе придётся вводить глобальные locks.

4) **Размеры pre/post в байтах**.  
   При подборе `pretrigger_bytes` думай в терминах “примерный размер записи × сколько записей нужно назад”.

5) **CRC32 как диагностический якорь**.  
   Если включено, это помогает парсеру и снижает риск ложной синхронизации.

---

## 10. Референсная архитектура интеграции (коротко)

- `app_logging_init()`:
  - init core/capture
  - init `q_fast`, init `q_meas`
  - init scheduler (если общий UART)

- `pwm_tick_isr()` (fast):
  - collect snapshot
  - push to `q_fast`

- `measurement_task()`:
  - collect meas snapshot
  - push to `q_meas`

- `logging_task()`:
  - handle arm/trigger/stop/read requests
  - drain `q_fast` (priority)
  - drain `q_meas`
  - pack + (crc) + write record

- `protocol_read_chunk()`:
  - `read_chunk(offset)` → send

- `tx_tick_250us()`:
  - `scheduler_on_tick()`

- `tx_pump()`:
  - `scheduler_next()` → send PDO/LOG

---

## 11. Мини‑глоссарий

- **Snapshot** — фиксированная структура “состояния” (быстро собрать, быстро передать).
- **SPSC** — очередь single‑producer/single‑consumer (один push‑контекст, один pop‑контекст).
- **Record** — сериализованная запись: header + payload (и, опционально, CRC32).
- **Pretrigger** — история “до события”, реализована как кольцевой буфер последних байт.
- **Posttrigger** — данные “после события” до лимита.
- **Resync (ПК)** — поиск валидного заголовка в байтовом потоке.
- **Aligned trigger** — выбор start окна по границе Record.

---

## 12. Текущее состояние и дальнейшие шаги

Сделано (реализовано в прошивке):
1) **MAGIC в заголовке**: поле `magic` (значение `0xA55A`) для надёжной синхронизации при ресинхронизации на ПК.
3) **Мульти-источники**: при наличии fast ISR + отдельной measurement task используются **две SPSC-очереди** (`q_fast`, `q_meas`) и **один writer** (logging task), который дренит обе очереди.

Требуется на стороне ПК:
- **PC-ресинхронизация**: парсер должен уметь найти первый валидный `RecordHeader` в окне и продолжать парсинг с границы записи.

Опционально (на будущее, если понадобится “идеальный” pretrigger):
- **aligned trigger** в прошивке (окно начинается строго с начала Record), см. п.6.3.
