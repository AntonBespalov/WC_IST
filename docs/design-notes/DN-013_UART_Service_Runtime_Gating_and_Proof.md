# DN-013_UART_Service_Runtime_Gating_and_Proof — Реализация ADR-008: runtime-модель сервисного UART

Статус: draft  
Дата: 2026-02-16  
Владелец: TBD  
Связано: `docs/decisions/ADR-008_UART_Service_Interface_Runtime_Model.md` (разделы 2 и 7), `docs/CONTEXT_SNAPSHOT.md`, `docs/PROJECT_CONTEXT.md`, `docs/ARCHITECTURE.md`, `docs/protocols/PCCOM4.02.md`, `docs/protocols/PCCOM4.02_PROJECT.md`, `docs/protocols/PROTOCOL_TK_ETHERCAT.md`, `docs/TEST_PLAN.md`  

---

## 1) Context / Problem
- Требуется перейти от “описательной” модели сервиса UART к реализуемому контракту на уровне прошивки: какие команды разрешены всегда, какие требуют сервисной сессии и safety-gating, и как система деградирует при перегрузке канала.
- Основа решения уже принята в ADR-008 (вариант A): `always-on RX`, явная `service session`, классификация операций и backpressure-политика TX.
- Контекст исполнения: приём UART в ISR/DMA, разбор/валидация/dispatch в task-контексте (домен 1 мс), fast loop PWM и async fault-path остаются независимыми и не должны деградировать.
- Критично доказать, что при активном сервисном трафике сохраняются safety-инварианты (аппаратный shutdown-path, запрет опасных операций вне gating) и timing-инварианты (без роста overruns fast loop, контроль задержек в домене 250 мкс EtherCAT под нагрузкой UART).

## 2) Goal / Non-goals
### Goal
- Реализовать ADR-008 / раздел 2 (Decision) и раздел 7 (Interfaces / Data / Timing impact) как проверяемый набор требований к коду:
  - `always-on RX + stream parser` в безопасном контуре исполнения;
  - `service_session_active` и матрица допуска команд (`read-only`, `service-control`, `energy-affecting`, `flash-affecting`);
  - reject-коды по причинам gating;
  - watchdog/timeout для сервисных энерговлияющих режимов;
  - backpressure-политика TX (drop/decimation + счётчики) без блокировок fast loop.
- Зафиксировать измеримые критерии приёмки и обязательный план доказательств (unit/SIL/on-target/HIL/fault-injection).
- Подготовить минимальный и детерминированный code plan с разбивкой на коммиты.

### Non-goals
- Не менять алгоритмы fast loop управления током и измерительного тракта.
- Не менять аппаратный shutdown-path (`BKIN/BKIN2`, `DRV_EN/INH`) и логику latched-off.
- Не вводить новый транспорт или альтернативный протокол вместо PCcom4.
- Не добавлять аутентификацию/ролевую модель доступа (отдельное решение/ADR).
- Не изменять EtherCAT PDO контракт как основной канал ТК.

## 3) Decision (что делаем)
- Вводим runtime-контракт сервиса UART в три слоя:
  - `Ingress`: UART ISR/DMA пишет байты в RX ring; ISR не выполняет CRC/валидацию payload/бизнес-логику.
  - `Service task`: stream parser PCcom4, валидация frame/length/CRC, классификация операции и проверка gating.
  - `Dispatch + TX`: выполнение разрешённых операций, формирование ответов/событий, backpressure и счётчики качества.
- Фиксируем state machine сервиса:
  - `SERVICE_LOCKED` (по умолчанию после reset): разрешены только `read-only` и `service-control`;
  - `SERVICE_ACTIVE`: разрешены сервисные операции согласно матрице допуска;
  - выход из `SERVICE_ACTIVE`: `Service.Disable`, reset, либо аварийный переход по safety-политике.
- Фиксируем матрицу допуска (минимум):
  - `read-only`: всегда;
  - `service-control`: всегда;
  - `energy-affecting`: только `SERVICE_ACTIVE` + `IDLE` + подтверждённый `PWM OFF` + нет latched fault;
  - `flash-affecting`: только `SERVICE_ACTIVE` + `IDLE` + `PWM OFF` + нет latched fault + завершённые валидаторы параметров.
- Фиксируем поведение при перегрузке:
  - при RX/TX overflow — дроп с инкрементом счётчиков и диагностикой;
  - блокирующие ожидания в fast loop/ISR запрещены;
  - при потере сервис-связи в активном энерговлияющем режиме — safe stop по timeout политике.

## 4) Rationale (почему так)
- Такой разрез обязанностей повторяет архитектурный инвариант “тяжёлое только в task, ISR минимальный”, снижая риск нарушения RT в PWM-домене.
- Явная service-session + матрица допуска исключают случайное применение команд, способных менять энергию/flash, в запрещённых состояниях.
- Отдельные метрики (overflow/drop/reject/watchdog_trip/age) делают поведение наблюдаемым и пригодным для доказательств, а не “чёрным ящиком”.
- Отклонённый подход “всё всегда активно без сессии” хуже по safety/доказуемости; подход “debug-only UART” хуже по эксплуатационной пригодности и создаёт расхождение debug/prod.

## 5) Interfaces / Data / Timing impact
- Интерфейсы/данные (уровень контракта):
  - внутренние признаки: `service_session_active`, `service_mode_state`, `gating_reject_reason`;
  - счётчики: `rx_crc_err`, `rx_overflow`, `parser_resync_count`, `cmd_reject_count`, `tx_drop_count`, `watchdog_trip`, `cmd_age_max_us`;
  - классификатор операций PCcom4: `read-only` / `service-control` / `energy-affecting` / `flash-affecting`.
- Протокольные эффекты (для профиля проекта):
  - операция включения/выключения сессии сервиса (`Service.Enable/Disable`) и коды reject для отклонённых операций;
  - требования к совместимости с существующими узлами `ManualDuty.*`, `TkPdo.Emu.*`, `Scope.*` без изменения базового frame-формата PCcom4.
- Тайминги и домены:
  - PWM-домен (1–4 кГц): без функциональных изменений;
  - сервисный домен (1 мс): parser + gating + dispatch;
  - EtherCAT домен (250 мкс): проверка отсутствия деградации при активном UART flood;
  - async fault-домен: без изменений, приоритет аппаратного stop сохраняется.
- Обратная совместимость:
  - базовая PCcom4 рамка сохраняется;
  - для ПК-инструментов добавляется обязательный шаг управления сервисной сессией и обработка reject-кодов.

## 6) Risks / Edge cases
- Риск 1: “тихий” рост задержек из-за parser/dispatch под burst трафиком.
  - Детект: `service_task_wcet`, `cnt_ctrl_overrun`, `cmd_age_us` под нагрузкой.
- Риск 2: обход gating из-за неполной матрицы допуска или ошибочной классификации операции.
  - Детект: unit-тесты матрицы допуска + fault-injection “опасная команда в ARMED/WELD”.
- Риск 3: деградация/голодание TX при активном Scope/логировании.
  - Детект: `tx_drop_count`, watermark буферов, проверка отсутствия блокировок.
- Риск 4: потеря сервис-связи в активных режимах `ManualDuty`/`TkPdo.Emu`.
  - Детект: `watchdog_trip`, переход в safe state, подтверждение на GPIO/статусе.
- Риск 5: регресс аппаратного shutdown-path при интеграции сервиса.
  - Детект: on-target измерение `BKIN_RAW -> PWM_OUT safe` в baseline и под UART stress.

## 7) Test plan / Proof / Rollback
### Test plan / Proof
- Unit/host:
  - parser stream: частичные кадры, мусор, бурст `0xFF`, CRC/length negative cases;
  - матрица допуска по состояниям: `IDLE/ARMED/WELD/FAULT`, `PWM OFF/ON`, `service_session_active`;
  - reject-коды: детерминированная причина отказа для каждой запрещённой операции;
  - watchdog/timeout для `ManualDuty` и `TkPdo.Emu.*`;
  - backpressure policy: drop/overflow счётчики и отсутствие блокировок.
- SIL:
  - сценарии “burst UART + штатный цикл управления” с проверкой, что fast path и state machine корректны;
  - сценарии последовательностей команд `Service.Enable -> mutating op -> Service.Disable` с проверкой переходов состояния.
- On-target измерения (GPIO/осциллограф/trace):
  - измерить `fast_loop_wcet` и `fast_loop_jitter`: baseline vs UART flood;
  - измерить `COMX_IRQ -> CMD_LATCHED` в EtherCAT-домене под одновременным UART flood;
  - измерить `service_task_wcet` и корреляцию с `rx_overflow/tx_drop_count`;
  - подтвердить `BKIN_RAW -> PWM_OUT safe` при активном сервисе (не хуже baseline).
- HIL/bench:
  - fault-injection (минимум 6):
    1) отключение USB-UART/пауза host 50–200 мс при активном `ManualDuty`,
    2) поток битых CRC/мусора в stream,
    3) burst валидных кадров выше throughput обработчика,
    4) попытка `flash-affecting` команды в `ARMED/WELD`,
    5) replay/out-of-order `seq` для `TkPdo.Emu.CmdWeld`,
    6) TX-backpressure (host перестал читать порт) при активном `Scope.Stream`.
  - ожидаемое: safe stop по timeout где применимо, корректные reject-коды/счётчики, отсутствие влияния на аппаратный shutdown-path.

### Rollback
- Быстрый rollback: отключить mutating-команды сервиса feature-flag’ом, оставить только `read-only` + `service-control`.
- Частичный rollback: временно выключить `ManualDuty` и `TkPdo.Emu.*`, оставив EtherCAT единственным источником команд.
- Безопасный rollback: при любом сомнении по RT/safety форсировать `SERVICE_LOCKED` по умолчанию и запрет энерговлияющих операций до исправления.

## 8) Status / Implementation links
- Status: draft
- Links: TBD (issue/PR/commit)
