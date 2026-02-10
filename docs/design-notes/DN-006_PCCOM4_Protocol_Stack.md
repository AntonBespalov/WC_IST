# DN-006_PCCOM4_Protocol_Stack — стек протокола PCcom 4.02 (плата ↔ ПК, USB-UART)
Статус: draft  
Дата: 2026-02-10  
Владелец: TBD  
Связано: `docs/protocols/PCCOM4.02.md`, `docs/protocols/PCCOM4.02_PROJECT.md`, `docs/TEST_PLAN.md` / 3.1, `docs/ARCHITECTURE.md` / protocol_core, `docs/decisions/ADR-20260209-tk-ethercat-pdo-emu-over-udp-batching.md`, `docs/decisions/ADR-20260210-tk-interface-ethercat-comx-fmc-and-uart-pdo-emu.md`

---

## 1) Context / Problem
Нужно реализовать совместимый стек PCcom 4.02 для сервисного обмена “плата ↔ ПК” по USB‑UART (FT232H): настройка параметров, диагностика, осциллографирование и сервисные режимы (в т.ч. `ManualDuty`, `SelfTest`, а также проектные узлы).

Ограничения:
- протокол должен быть устойчивым к шуму/мусору в потоке UART (preamble `0xFF` не уникален);
- реализация не должна ухудшать детерминизм fast loop (PWM‑домен) и не должна вмешиваться в аппаратный shutdown‑path (TIM1 BKIN/BKIN2, DRV_EN/INH), см. `docs/PROJECT_CONTEXT.md` и `docs/SAFETY.md`.

Контекст исполнения (целевой):
- RX: ISR/DMA → ring buffer;
- парсинг и диспетчеризация команд: FreeRTOS task (slow loop);
- TX: неблокирующая очередь/передача (DMA/IRQ).

Временной домен: RT‑чувствительный (обмен идёт параллельно с PWM‑контуром), поэтому запрещены блокировки/долгие операции в критичных путях.

---

## 2) Goal / Non-goals
### Goal
- Реализовать:
  - фрейминг/декодирование PCcom 4.02 + CRC16 Modbus RTU по `docs/protocols/PCCOM4.02.md`;
  - проектные узлы/операции по `docs/protocols/PCCOM4.02_PROJECT.md` (перечень будет расширяться);
  - единый слой safety‑gating для “опасных” сервисных команд (только `IDLE` + подтверждённый `PWM OFF`, таймауты, валидация диапазонов, запрет flash‑операций вне safe state).
- Обеспечить доказуемость: host unit + on‑target замеры + fault‑injection (см. `docs/TEST_PLAN.md` / 3.1).

### Non-goals
- Не определяем формат payload для узлов, где в профиле проекта указано “TBD” (например, эмуляция CAN/осциллографические наборы данных) — обеспечиваем только транспорт/контейнер и контроль длины/CRC.
- Не переносим протокол на новый физический интерфейс (Ethernet/UDP) в этом DN; но закладываем совместимую архитектуру парсеров для будущего UDP‑профиля.

---

## 3) Decision (что делаем)
### 3.1 Слои реализации
Делим реализацию на независимые слои:
1) `pccom4_crc16`: CRC16 (Modbus RTU).
2) `pccom4_frame`: pack/unpack полей `Length/Dst/Src/Type/Node/Op/Data/CRC`.
3) `pccom4_stream_parser`: потоковый парсер UART (resync по `0xFF`, валидация `Length`, CRC).
4) `pccom4_dispatch`: диспетчеризация `Node/Op` в таблицу обработчиков + единый формат ошибок.
5) `pccom4_project_handlers`: обработчики профиля WC_IST (узлы/операции).
6) `pccom4_transport_uart1`: glue к HAL/FreeRTOS (DMA RX ring + task notify; неблокирующий TX).

### 3.2 Политики и инварианты (safety)
- Любая команда, способная менять энергию/ШИМ/режимы/flash, проходит через единый gate:
  - `state == IDLE` и `PWM OFF` подтверждён;
  - нет активного latched fault (recovery только по политике проекта);
  - значения валидированы (диапазоны, запрет NaN/Inf для float);
  - запись во flash — только в safe state (см. `docs/PROJECT_CONTEXT.md`).
- `ManualDuty` и `SelfTest` реализуются строго по условиям допуска из `docs/protocols/PCCOM4.02_PROJECT.md` и `docs/design-notes/DN-002_MFDC_ManualDuty_Service_Mode.md`.

### 3.3 Расширяемость перечня команд
Перечень операций `Node=0x04` (“Настройка параметров”) будет расширяться.
Добавление нового параметра должно быть “табличной” операцией:
- добавили запись (Node/Op → описание, длина Data, права доступа, persist‑policy, валидатор);
- добавили тесты валидатора и round‑trip кодека;
- обновили `docs/protocols/PCCOM4.02_PROJECT.md`.

---

## 4) Rationale (почему так)
- Парсер UART должен быть устойчивым к потоку без уникального SOF (preamble `0xFF` встречается в данных) → нужен явный resync‑алгоритм по `docs/protocols/PCCOM4.02.md` / 8.
- Разделение на слои позволяет:
  - тестировать core‑логику на ПК без HAL/FreeRTOS;
  - удержать быстрые пути ISR/DMA минимальными (копирование в ring + сигнал таске);
  - подготовить повторное использование `pccom4_frame/dispatch` для UDP‑транспорта (см. ADR про batching).

---

## 5) Interfaces / Data / Timing impact
### 5.1 UART конфигурация (текущая база)
Используем текущую конфигурацию USART1: `115200, 8N1, RTS/CTS` (см. `Core/Src/main.c`).

### 5.2 CRC/endianness
- CRC16 в кадре: `[CRC_LO][CRC_HI]` (Modbus RTU), см. `docs/protocols/PCCOM4.02.md` / 4.3.
- Проектное кодирование многобайтных значений в `Data`: little‑endian (см. `docs/protocols/PCCOM4.02_PROJECT.md` / 1.3).

Открытый пункт для фиксации:
- в `docs/protocols/PCCOM4.02_PROJECT.md` встречаются поля с пометкой “MSB first” для `u16` в `SelfTest.*`.
- требуется выбрать единый вариант для проекта (рекомендуемый: привести к little‑endian везде) и синхронизировать документ/реализацию/tooling.

### 5.3 Влияние на тайминги
- В fast loop (PWM‑домен) запрещены любые операции PCcom (ни парсинга, ни ответов, ни flash).
- On‑target доказать “не влияет на fast loop” измерениями jitter и worst‑case длительности шага управления (см. `docs/TEST_PLAN.md` / 3.1).

---

## 6) Risks / Edge cases
- RX мусор/шум/бурст `0xFF` → потеря синхронизации парсера; лечится resync + таймаут межбайтового разрыва + тесты.
- Переполнение RX/TX буферов при бурсте сообщений (особенно `Scope.Data`) → требуются backpressure/дроп + счётчики; система должна оставаться управляемой и безопасной.
- Ошибки в gating (например, разрешили `ManualDuty` в `WELD`) → риск неконтролируемой энергии; ловится негативными on‑target тестами и fault‑injection.
- Несогласованная endianness в проекте → “тихие” ошибки диагностики/настроек; лечится фиксацией в профиле + unit тестами на раскладку байт.

---

## 7) Test plan / Proof / Rollback
### Test plan / Proof
См. `docs/TEST_PLAN.md` / 3.1 как минимальный обязательный набор. Конкретизация:

Unit/host (L1):
- CRC16 (golden vectors), проверка порядка байтов CRC.
- Парсер стрима: частичные кадры, ресинхронизация, `Length` 8…255, битые CRC, шум.
- Dispatch: неизвестные `Node/Op` → `Type=0x00` для read/write; на `Type=0x02` не отвечать.

On-target измерения:
- сравнение “PCcom OFF vs ON”: jitter `CTRL_TICK` по `DBG0/DBG1`, worst‑case время шага управления.
- стресс RX (бурст кадров) + проверка отсутствия блокировок/overrun.

Fault-injection (минимум 5):
1) бурст битых CRC + мусор в потоке;
2) переполнение RX ring buffer;
3) попытка `ManualDuty.Enable=ON` в `ARMED/WELD` (должно быть отвергнуто);
4) таймаут сервис‑связи `ManualDuty` > 20 мс → safe state (см. DN‑002);
5) попытка `SelfTest.Run` не в `IDLE` (должно быть отвергнуто);
6) попытка записи параметров/flash‑операций вне `IDLE` + `PWM OFF` (должно быть отвергнуто).

Evidence:
- логи/счётчики (CRC errors, parse errors, drops, overruns);
- осциллограммы jitter/latency (условия измерений фиксировать: baudrate/поток).

### Rollback
- Сборочный флаг/feature‑toggle, отключающий transport/task PCcom4, сохраняя остальную систему.
- Runtime‑rollback для “опасных” команд: временно отключить обработчики `ManualDuty.*`/tuning/selftest, оставив чтение статуса/версии/диагностики.
- Любой rollback не должен менять аппаратный shutdown‑path и политику latched faults.

---

## 8) Status / Implementation links
- Status: draft
- Links: TBD (PR/commit/issue)

