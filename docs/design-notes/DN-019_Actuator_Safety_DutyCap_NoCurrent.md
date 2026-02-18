# DN-019_Actuator_Safety_DutyCap_NoCurrent — Duty cap и NO_CURRENT для безопасности актуатора

Статус: **draft (spec-complete)**  
Дата: 2026-02-18  
Владелец: TBD  
Связано: `docs/PROJECT_CONTEXT.md`, `docs/ENGINEERING_CONTRACT.md`, `docs/TEST_PLAN.md`, `docs/CONTEXT_SNAPSHOT.md`

---

## 1) Context / Problem
- Источник не знает, сомкнуты ли клещи; при большом `I_ref` регулятор может увести `duty` к 100%, что опасно для вторички/электродов.
- В `control_core` нет:
  - явного верхнего ограничения `duty` (safety cap),
  - детектора “нет тока при попытке сварки” (NO_CURRENT) с корректным latch/recovery.
- Изменение касается fast loop (PWM-домен), а реакция (останов) проходит через `safety_supervisor/state_machine` (без изменения аппаратного shutdown-path).
- Временной домен: период PWM 1–4 кГц; требуется time-based окно детекта (в миллисекундах), а не “N периодов”.

## 2) Goal / Non-goals
### Goal
- Ограничить `duty` сверху (cap) независимо от запроса ТК и режима формирования `duty`.
- Ввести детектор NO_CURRENT по признакам “высокий `duty_used`” и `I_per ≈ 0` на заданном временном окне.
- Обеспечить телеметрию:
  - `limit_word.LIMIT_DUTY`,
  - `fault_word.NO_CURRENT`,
  - `duty_used_permille`,
  - и корректный latch/recovery.
- Зафиксировать fail-safe контракт по ошибкам конфигурации (Variant A): некорректные параметры не могут “тихо” выключить защиту.

### Non-goals
- Не менять политику C0/BKIN и аппаратный shutdown-path.
- Не менять частоту PWM/инварианты измерений (`I_per` как усреднение по периоду PWM).
- Не менять протоколы/формат обмена с ТК.
- Не вносить блокирующие операции/форматирование логов в fast loop.

---

## 3) Decision (что делаем)

### 3.1 Duty Cap (ограничение duty по безопасности)
- Вводим параметр `duty_cap_permille` (0…1000).
- Ограничение применяется **на выходе актуатора** (после всех режимов формирования `duty_cmd`, перед записью в TIM1):
  - `duty_used = clamp(duty_cmd, 0, duty_cap_permille)`
- Если `duty_cmd > duty_cap_permille`:
  - выставляем `limit_word.LIMIT_DUTY = 1`,
  - в телеметрии `duty_used_permille` отражаем **фактически применённое** `duty_used`.

**Контракт anti-windup (если применимо):**
- Насыщение по cap должно считаться сатурацией выхода; интегратор не должен “усугублять” сатурацию по cap (аналогично `u_max/u_min`).

### 3.2 Детектор NO_CURRENT (попытка сварки при токе ≈ 0)
Назначение: обнаружить режим “варим в воздухе/клещи не замкнуты”, когда система подаёт значимое `duty`, а ток остаётся около нуля.

**Условия детектора (все одновременно):**
1) Разрешение сварки активно: `enable/allow == 1`.
2) Измерения валидны: `meas_valid == 1`.
3) “Есть попытка сварки”:
   - `duty_used >= duty_no_current_duty_thr_permille`
   - (опционально, если в проекте есть смысловой гейт по уставке) `I_ref_used >= i_try_thr_A`
4) Ток “около нуля”:
   - `abs(I_per) <= i_zero_thr_A`,
   где `I_per` — усреднение по периоду PWM (проектный инвариант).

**Blanking (защита от ложных срабатываний при старте):**
- После фронта `enable/allow` (или входа в `WELD_ACTIVE`) детектор блокируется на время `t_blank_ms`.
- В течение `t_blank_ms` накопление NO_CURRENT запрещено.

**Окно по времени (ms), а не по периодам:**
- Параметр `t_no_current_ms` задаётся в миллисекундах.
- Реализация в fast loop накапливает “прошедшее время” по фактическому `T_pwm_ms`:
  - `accum_ms += T_pwm_ms`, пока условия выполняются;
  - при нарушении любого условия `accum_ms = 0`.
- Срабатывание: если `accum_ms >= t_no_current_ms` → фиксируем событие NO_CURRENT.

### 3.3 Реакция на NO_CURRENT и восстановление (latch/recovery)
- При срабатывании:
  - выставить `fault_word.NO_CURRENT = 1`,
  - выполнить controlled stop через SM/safety_supervisor (например, `DRV_EN=0` / запрет PWM) **не позже следующего периода PWM**.
- Latch / recovery (контракт):
  - `NO_CURRENT` защёлкивается до выполнения обоих условий:
    1) `enable/allow` стал `0`,
    2) начат новый цикл сварки (повторное `enable/allow==1`; опционально — также “новый seq”, если это часть общей политики).
- Аппаратный shutdown-path (BKIN/C0) не меняется и остаётся независимым.

### 3.4 Параметры (единицы, disable)
- `duty_cap_permille` (0…1000) — cap по duty (permille).
- `duty_no_current_duty_thr_permille` (0…1000) — порог “высокого duty” для NO_CURRENT.
- `i_zero_thr_A` (A) — порог “ток около нуля” по `I_per`.
- `i_try_thr_A` (A, optional) — порог “существенная уставка тока” (если используется).
- `t_blank_ms` (ms) — blanking после старта сварки.
- `t_no_current_ms` (ms) — окно подтверждения NO_CURRENT.
- Disable NO_CURRENT: `t_no_current_ms == 0` (детектор выключен).

### 3.5 Mode gating (ManualDuty / сервис)
**Общее правило**
- `duty_cap_permille` действует **во всех режимах** формирования duty (PI, ManualDuty, сервисные).

**NO_CURRENT по умолчанию**
- NO_CURRENT активен **во всех режимах**, включая `ManualDuty`, если выполняются условия детектора и прошёл `t_blank_ms`.

**Единственное исключение — Service override**
- Отключение NO_CURRENT в `ManualDuty` допускается **только** при активном `service_override==1` (явное разрешение наладки/сервиса).
- При активном `service_override`:
  - NO_CURRENT не накапливается и не срабатывает,
  - duty cap **не отключается**,
  - состояние `SERVICE_OVERRIDE_ACTIVE` отражается в диагностике/телеметрии.

**Ограничения на активацию `service_override`**
- `service_override` разрешается активировать **только при `enable/allow == 0`** (до старта сварки).
- Попытка `service_override: 0→1` при `enable/allow == 1` → **CFG_INVALID + controlled stop**.

### 3.6 Validation rules (fail-safe Variant A)
**Hard rules (нарушение = CFG_INVALID)**
- `0 <= duty_cap_permille <= 1000`
- `0 <= duty_no_current_duty_thr_permille <= duty_cap_permille`
- `i_zero_thr_A >= 0`
- если используется `i_try_thr_A`: `i_try_thr_A >= 0`
- `t_blank_ms >= 0`
- `t_no_current_ms >= 0`
- `service_override` допускается только в `ManualDuty`; попытка активировать override вне `ManualDuty` → CFG_INVALID.
- Если механизм `service_override` в данной сборке не предусмотрен, любая попытка установить `service_override==1` → CFG_INVALID.

**Fail-safe реакция (Variant A)**
- При нарушении любого Hard rule:
  - конфигурация отклоняется/не применяется,
  - сварка ингибируется (или, если активна — controlled stop),
  - защёлкивается диагностический признак `CFG_INVALID`,
  - восстановление возможно только после `enable/allow==0` и применения валидной конфигурации.

### 3.7 Детерминизм
- Fast loop остаётся O(1): сравнения + накопление `accum_ms` + clamp.
- Никаких блокировок/аллокаций/лог-форматирования в fast loop.

---

## 4) Rationale (почему так)
- Duty cap предотвращает уход в 100% при разомкнутых клещах независимо от запроса ТК.
- NO_CURRENT устраняет опасный режим “подаём мощность, тока нет” и делает поведение диагностируемым.
- Решение не затрагивает аппаратный shutdown-path (BKIN/C0) и не ломает safety-инварианты.
- Time-based окно (ms) обеспечивает одинаковое поведение на 1–4 кГц.

## 5) Interfaces / Data / Timing impact
### Данные/статусы
- Статусы:
  - `limit_word.LIMIT_DUTY`,
  - `fault_word.NO_CURRENT`,
  - `CFG_INVALID` (как fault/flag проекта),
  - `duty_used_permille`.
- Детектор использует:
  - `I_per` (усреднение по периоду PWM),
  - `meas_valid`,
  - `enable/allow`,
  - `duty_used`.

### Тайминги
- Детект worst-case:
  - `t_detect <= t_blank_ms + t_no_current_ms + 1*T_pwm`
- Отключение (controlled stop) после фиксации события:
  - `t_off <= 1*T_pwm`
- Duty cap применяется в том же периоде PWM, где вычислен `duty_cmd` (без дополнительной задержки).

## 6) Risks / Edge cases
- **Ложные срабатывания из-за шумов/смещения нуля тока**: требуется корректный выбор `i_zero_thr_A`; при необходимости — гистерезис.
- **Стартовые транзиенты**: обязательный `t_blank_ms`.
- **Низкая PWM частота**: окно задаётся в ms.
- **Провалы `meas_valid`**: при `meas_valid==0` накопление сбрасывается, fault не формируется.
- **ManualDuty**: без жёсткого контракта может стать обходом защиты; поэтому NO_CURRENT включён по умолчанию, а отключение только через `service_override`.
- **Отказ датчика тока (stuck-at 0)**: допустимая safety-реакция (лучше остановиться), но важно иметь предсказуемое поведение через `meas_valid`/диагностику.

## 7) Test plan / Proof / Rollback
### Test plan / Proof

**Unit/host**
- Duty cap:
  - `duty_cmd <= cap` → `duty_used == duty_cmd`, `LIMIT_DUTY=0`;
  - `duty_cmd > cap` → `duty_used == cap`, `LIMIT_DUTY=1`.
- NO_CURRENT:
  - накопление `accum_ms` растёт только при выполнении всех гейтов;
  - при нарушении любого условия → `accum_ms=0`;
  - срабатывание на границе `accum_ms >= t_no_current_ms`.
- Проверка time-based поведения на 1 кГц и 4 кГц:
  - момент срабатывания одинаков по времени (в пределах ±1 периода PWM).
- Blanking:
  - NO_CURRENT не может сработать раньше `t_blank_ms + t_no_current_ms`.
- Latch/recovery:
  - fault держится до `enable/allow==0` и нового цикла разрешения сварки.
- Validation / CFG_INVALID:
  - `duty_thr > duty_cap` → CFG_INVALID;
  - `service_override==1` вне ManualDuty → CFG_INVALID;
  - `service_override:0→1` при `enable==1` → CFG_INVALID + stop (в модели/эмуляции).

**On-target (стенд)**
- “Клещи разомкнуты”:
  - детект NO_CURRENT по времени,
  - controlled stop не позже следующего периода PWM,
  - корректные статусы (`NO_CURRENT`, `LIMIT_DUTY` при необходимости).
- “Нормальная сварка”:
  - NO_CURRENT не срабатывает,
  - cap может быть активен без ложного fault.
- “Фликер валидности”:
  - кратковременный `meas_valid=0` → накопление сбрасывается, ложных fault нет.
- Частоты PWM 1 кГц / 4 кГц:
  - одинаковые временные свойства (с учётом квантования периодом PWM).

**HIL/bench (fault injection)**
- Stuck-at `I_per=0` при `meas_valid=1` → гарантированное срабатывание NO_CURRENT.
- Timeout/invalid (`meas_valid=0`) → NO_CURRENT не накапливается.
- Шум `I_per` около порога → отсутствие дребезга (при корректных порогах/blanking).

**Критерии приёмки**
- `t_detect <= t_blank_ms + t_no_current_ms + 1*T_pwm`
- `t_off <= 1*T_pwm` после фиксации события
- Детерминизм fast loop сохранён (нет роста overruns/джиттера относительно baseline)
- BKIN/C0 не затронуты

### Rollback
- `duty_cap_permille = 1000`
- `t_no_current_ms = 0` (NO_CURRENT disable)
- Снять `service_override` (если применялся)
- Прогнать базовые L3 измерения и регрессионные стендовые кейсы

## 8) Status / Implementation links
- Status: **draft (spec-complete)**
- Links: TBD
