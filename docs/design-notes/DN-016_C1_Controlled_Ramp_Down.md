# DN-016_C1_Controlled_Ramp_Down — Controlled ramp down для C1

Статус: draft  
Дата: 2026-02-17  
Владелец: TBD  
Связано: TBD  

---

## 1) Context / Problem
- Для части причин класса C1 (перегрев, **soft-timeout** по командам ТК, невалидные измерения уровня “quality bad”) требуется **controlled ramp down** перед программным OFF, чтобы снизить ударные переходные процессы.
- При этом существуют C1-события, для которых ramp недопустим/бессмысленен (нарушение детерминизма fast-loop, **hard-timeout**, “measurement pipeline dead”) — там нужен **немедленный OFF + latch**.
- Контекст исполнения: принятие решения/причины — `safety_supervisor` + `state_machine` (slow-домен), применение ramp — fast-loop (PWM-домен).
- Временные домены: PWM (детерминированный тик) + async fault/timeout.

## 2) Goal / Non-goals
### Goal
- Ввести режим `C1_RAMP` длительностью `T_ramp_ms = 10–20 мс` (дефолт 10 мс) перед программным OFF **для разрешённых причин C1**.
- Зафиксировать политику отмены/восстановления:
  - отмена ramp после старта **не допускается**;
  - recovery только после достижения safe state и **явного recovery** (команда/процедура).
- Зафиксировать поведение по timeouts: **soft-timeout → ramp**, **hard-timeout → немедленный OFF + latch**.

### Non-goals
- Не менять C0 path (BKIN/BKIN2), его latency и latch-политику.
- Не вводить авто-рестарт после аварий.
- Не менять пороги измерений/температур/классы C0/C2.

## 3) Decision (что делаем)
### 3.1 Политика timeouts (loss of comms)
Вводятся два порога потери команд ТК (значения — TBD):
- **soft-timeout**: отсутствуют валидные команды > `T_soft_ms` → причина C1 → `C1_RAMP` → OFF + latch.
- **hard-timeout**: отсутствуют валидные команды > `T_hard_ms` (где `T_hard_ms > T_soft_ms`) → **немедленный OFF + latch** (без ramp).

> Примечание: в этом DN “потеря связи” в контексте ramp означает именно soft-timeout. Hard-timeout — отдельная эскалация.

### 3.2 Условия входа в `C1_RAMP`
`C1_RAMP` стартует только для “разрешённых причин” C1:
- overtemp (не C0),
- soft-timeout,
- invalid measurements уровня “quality bad” (stuck/sat/out-of-range и т.п.), **если measurement pipeline жив** (см. 3.4).

### 3.3 Алгоритм ramp (fast-loop, 1 раз на период PWM)
- При старте `C1_RAMP` фиксируется снапшот:
  - `I_ref_start = I_ref_used_at_ramp_start` (снапшот/атомарно, clamp ≥ 0),
  - `T_ramp_ms` (политика).
- Число шагов определяется частотой PWM:
  - `T_pwm_ms = 1000 / f_pwm_hz`
  - `N_steps = ceil(T_ramp_ms / T_pwm_ms)` (минимум 1)
- Профиль линейный, обновление строго по тикам PWM:
  - для шага `k = 0..N_steps-1`:
    - `I_ref_used(k+1) = max(0, I_ref_start - (k+1) * I_ref_start / N_steps)`
- Во время `C1_RAMP` **новые команды ТК не влияют** на текущий ramp (никакой “неявной отмены” через команды).
- По завершении ramp (`I_ref_used == 0` и `k == N_steps`) выполняется “safe OFF sequence” (см. 3.5), затем переход в `OFF_LATCHED`.

### 3.4 Эскалации/исключения (без ramp)
- **C0 во время ramp** → аппаратный OFF через BKIN/BKIN2 (C0 path без изменений).
- **Overrun fast-loop** (нарушен контракт “1 шаг на период PWM”; детект: новый тик при активном шаге) → **немедленный OFF + latch**.
- **Hard-timeout** → **немедленный OFF + latch**.
- **Measurement pipeline dead** (timeout внешних АЦП, отсутствие данных/зависание пайплайна) → **немедленный OFF + latch**.
  - Пояснение: ramp-профиль не использует измерения, но дальнейшая коммутация силовой части при “pipeline dead” недопустима.

### 3.5 Safe OFF sequence (порядок действий)
По завершении ramp или при немедленном программном OFF выполняется порядок отключения:
1) Перевести PWM в безопасное состояние (выходы в idle/forced inactive) и затем отключить выдачу (например: MOE=0 / disable outputs — конкретика реализации TBD, но семантика фиксируется как “PWM_OUT safe+off”).
2) Отключить драйверы силовой части через `DRV_EN/INH OFF` (если доступно на плате/ревизии).
3) Зафиксировать в статусе:
   - `fault_class=C1`, `fault_cause`, `ramp_was_active`, `seq_applied`,
   - установка latch-флага.

> Пояснение по порядку: сначала “успокаиваем” PWM, затем запрещаем драйверы, чтобы избежать непредсказуемых фронтов на линиях управления.

### 3.6 Latch-политика (C1)
- Любая обработанная причина C1 (как с ramp, так и без) приводит к состоянию `OFF_LATCHED`.
- Выход из `OFF_LATCHED` возможен только через **явный recovery** при выполнении условий безопасности (TBD в state_machine).

### 3.7 Переходы state machine (минимальная формализация)
- `WELDING → C1_RAMP → OFF_LATCHED`
- `WELDING → OFF_LATCHED` (hard-timeout / overrun / pipeline-dead)
- `C1_RAMP + C0_event → C0_HW_OFF_LATCHED`
- `OFF_LATCHED + explicit_recovery + conditions_ok → READY`

## 4) Rationale (почему так)
- Controlled ramp снижает нагрузку на силовую часть и риск небезопасных переходных процессов.
- Линейный профиль детерминирован, прост для верификации и **не опирается** на измерения (но допускает эскалацию при “pipeline dead”).
- Разделение soft/hard timeout убирает двусмысленность: ramp полезен при кратковременной потере команд, но при длительной — нужен быстрый stop.
- Сохранение C0 path неизменным удерживает safety-инварианты (BKIN как hardware shutdown).

## 5) Interfaces / Data / Timing impact
- Параметры политики:
  - `T_ramp_ms` (10–20 мс, дефолт 10 мс)
  - `T_soft_ms` / `T_hard_ms` (TBD, `T_hard_ms > T_soft_ms`)
- Состояния/флаги:
  - `state=C1_RAMP`, `ramp_active`, `ramp_steps_total`, `ramp_step_idx`, `I_ref_start`
  - `fault_class=C1`, `fault_cause`, `latch=1`
- Тайминги:
  - ramp-шаг применяется строго по периоду PWM, O(1), без блокировок.
- Синхронизация slow→fast:
  - атомарный снапшот (seq counter/double-buffer), ramp стартует только по консистентным данным.

## 6) Risks / Edge cases
- Hard-timeout наступает во время ramp → немедленный OFF + latch.
- Невалидные измерения “quality bad” во время ramp:
  - если pipeline жив — продолжаем ramp (профиль не зависит от измерений);
  - если pipeline dead — немедленный OFF + latch.
- C0 (BKIN) возникает во время ramp → аппаратный OFF (C0 path).
- Рваный снапшот slow→fast → неверные параметры ramp → требуется seq-guard.
- Команды ТК приходят во время ramp → не влияют на ramp (иначе получится неявная отмена).

## 7) Test plan / Proof / Rollback
### Test plan / Proof
- Unit/host:
  - Формулы: `N_steps = ceil(T_ramp_ms / T_pwm_ms)`, корректное квантование на 1 кГц и 4 кГц.
  - `I_ref_start` фиксируется на `RAMP_START` (приход новых `I_ref_cmd` не меняет ramp).
  - Монотонность `I_ref_used` и достижение нуля ровно за `N_steps`.
  - Эскалации: hard-timeout / overrun / pipeline-dead → немедленный OFF + latch (без ramp).
- SIL:
  - Трассы “soft-timeout/перегрев/quality-bad” → корректный ramp и OFF_LATCHED.
  - Трасса “hard-timeout во время ramp” → немедленный OFF + latch.
- On-target измерения (GPIO/осциллограф/trace):
  - `RAMP_START → I_ref_used=0` (t_ramp) и `PWM_OUT safe+off`.
  - `PWM_OUT safe+off → DRV_EN/INH OFF` (порядок и задержка — если предусмотрена).
  - `BKIN_RAW → PWM_OUT safe` при C0 (без ухудшения).
- HIL/bench:
  - Потеря связи: `T_soft_ms` → ramp; `T_hard_ms` → OFF + latch.
  - “quality-bad” измерений при живом pipeline: ramp → OFF + latch.
  - “pipeline dead”: немедленный OFF + latch.
  - C0 во время ramp: аппаратный OFF.

### Rollback
- Вернуться к немедленному программному OFF для C1 (без ramp), C0 path не менять.

## 8) Status / Implementation links
- Status: draft
- Links: TBD
