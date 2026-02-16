# Протокол обмена с ТК по EtherCAT PDO (Источник ↔ Технологический комплекс)

Этот документ — единственный источник правды по формату PDO, таймингам, кодам статуса и политике ошибок **для профиля EtherCAT**.
Целевое решение по интерфейсу: см. `docs/decisions/ADR-004_TK_Interface_EtherCAT_COMX_FMC_and_UART_PDO_Emu.md`.

Ключевое правило (важно):
- **Перечень данных и их семантика** берутся из `docs/protocols/PROTOCOL_TK.md` (исторически CAN-профиль).
- Для EtherCAT этот набор данных переносится в PDO (RxPDO/TxPDO) **без изменения смыслов полей**.
- Dev-эмуляция “как PDO” по USB‑UART (PCcom4) должна быть согласована с этим документом, а не наоборот (см. `docs/protocols/PCCOM4.02_PROJECT.md` / 3.2).

Артефакты для интеграции/декодирования (TBD):
- ESI (EtherCAT Slave Information) XML для COMX 100CA-RE (Vendor ID / Product Code / PDO mapping).

Примечание по терминам: далее «Источник» (контроллер) может также обозначаться как «ИСТ», «УСПФ» или «плата источника сварочного тока» (см. `docs/GLOSSARY.md`).

---

## 1) Транспорт и общие параметры
- Полевая сеть: **EtherCAT** (циклический обмен process data).
- Роль источника: **EtherCAT Slave** (реализован в COMX 100CA-RE).
- Подключение к MCU: **COMX 100CA-RE ↔ FMC** (MCU читает/пишет process image; ISR — минимальный, основная обработка в task-контексте).
- Циклические объекты (PDO):
  - `RxPDO_CMD_WELD` — ТК → Источник (циклическая команда)
  - `TxPDO_FB_STATUS` — Источник → ТК (циклический статус/измерения)
  - (опционально) `TxPDO_FAULT` — Источник → ТК (событийное уведомление о критической аварии; см. 4.3)
- Модель обмена:
  - [x] периодическая телеметрия + команды (PDO)
  - [ ] запрос-ответ (mailbox/CoE) (не входит в профиль этого документа)
  - [ ] смешанная

Тайминги (Draft 0.2.3):
- Период команд ТК: **250 мкс** (4 кГц)
- Временные требования:
  - Максимальная задержка “команда доступна → команда принята/защёлкнута (APPLY)” в MCU_APP: **≤ 250 мкс** (цель)
  - Максимальная задержка “команда принята/защёлкнута → команда учтена fast loop” (эффект на уставке в PWM-домене): **≤ 1 период PWM** (цель)
  - Таймаут отсутствия валидных команд:
    - soft-timeout: **5 мс** (Draft) → controlled stop / спад `I_ref_used`
    - hard-timeout: **20 мс** (Draft) → запрет сварки + FAULT (latch по политике)
    - примечание: при 4 кГц это соответствует 20 и 80 циклам обмена соответственно

Примечание (отладка):
- При стендовой отладке без реального EtherCAT master взаимодействие может имитироваться с ПК по USB‑UART (PCcom4) (см. `docs/protocols/PCCOM4.02_PROJECT.md` / 3.2). Тайминги 250 мкс в этом режиме не гарантируются.

---

## 1.1) Единицы измерения и типы данных (Draft 0.2)

Переносится из `docs/protocols/PROTOCOL_TK.md` / 1.1:
- Все значения тока в протоколе — **`int32_t` в mA** (signed).
- Максимальный ток по системе: **50 000 А** (50 000 000 mA).

---

## 1.2) Spec-lock: сериализация и валидаторы (Draft 0.2.2)

Переносится из `docs/protocols/PROTOCOL_TK.md` / 1.2 с адаптацией “CAN → PDO”.

### 1.2.1 Длины PDO и endianness
- Длины PDO фиксированы:
  - `RxPDO_CMD_WELD` — **16 байт**
  - `TxPDO_FB_STATUS` — **48 байт**
  - (опционально) `TxPDO_FAULT` — **16 байт**
- Endianness: все multi-byte поля (`u16/u32/i32`) кодируются как **Little-Endian (LE)**, **без исключений**, включая счётчики.

### 1.2.2 Reserved policy
- Любые `reserved` байты/биты при передаче: **MUST=0**.
- При приёме команд (`RxPDO_CMD_WELD`):
  - любое `reserved != 0` ⇒ **REJECT** + `cnt_cmd_reject++`.
  - Поля `flags`/`crc` в текущей версии MUST=0 (см. 3.4).

### 1.2.3 Политика `seq` (u16)
Политика `seq` переносится из `docs/protocols/PROTOCOL_TK.md` / 1.2.3.

`seq` сравнивается по модулю 65536 (wrap-around 65535 → 0) по “half-range” правилу:
- `delta = (seq - last_seq) mod 65536`
- первый валидный кадр после старта/выхода из FAULT: APPLY, `last_seq = seq`
- повтор (`delta==0`) ⇒ REJECT + `cnt_cmd_reject++`
- движение вперёд:
  - `delta==1` ⇒ нормально
  - `delta>1 && delta<=0x7FFF` ⇒ APPLY, но `SEQ_GAP_DETECTED=1` и `cnt_seq_gap++`
- скачок назад (`delta>0x7FFF`) ⇒ REJECT + `cnt_cmd_reject++`

#### Уточнение для cmd-rate > PWM-rate (4 кГц vs 1–4 кГц)
Чтобы избежать ложных `SEQ_GAP_DETECTED` при применении уставки раз в период PWM:
- В этом профиле **APPLY** означает: “команда валидирована и опубликована как *последняя валидная* (command latch)”.
- Fast loop использует **последнюю валидную** команду, защёлкнутую на границе периода PWM (см. `docs/PROJECT_CONTEXT.md` / тайминги).
- `SEQ_GAP_DETECTED` и `cnt_seq_gap` относятся **к приёму валидных `CMD_WELD`** (gap между двумя подряд принятыми валидными командами), а не к “частоте использования уставки” внутри fast loop.

### 1.2.4 Валидность команды и “не варим вслепую”
Команда `CMD_WELD` (RxPDO) считается валидной только если:
- длина PDO соответствует профилю,
- `reserved` поля равны 0,
- `mode/enable/I_ref_cmd/max_slew_rate_A_ms/fault_reset` проходят проектные валидаторы,
- `seq` проходит политику 1.2.3.

При сомнении поведение должно быть консервативным: REJECT и/или safe stop, но не включение энергии.

---

## 2) Состояния устройства (state machine)

Переносится из `docs/protocols/PROTOCOL_TK.md` / 2 (без привязки к транспорту):
- `IDLE` — безопасное состояние, сварка запрещена, PWM OFF.
- `ARMED` — подготовка/разрешение, ожидание старта.
- `WELD` — активная сварка.
- `FAULT` — авария (для критических fault — latch до recovery по политике).

Переходы, latch/recovery и условия допуска — по `docs/SAFETY.md` и реализации state machine; данный документ фиксирует поля и правила валидности/таймаутов.

---

## 3) Команда ТК → Источник (`CMD_WELD`, RxPDO)

### 3.1 Поля
Переносится из `docs/protocols/PROTOCOL_TK.md` / 3.1–3.3:
- `seq`: номер командного кадра (см. 1.2.3); используется для детекта пропусков/повторов.
- `mode`: запрошенный режим state machine (IDLE/ARMED/WELD). Фактическое состояние отражается в `FB_STATUS.state`.
- `enable`:
  - `1` — разрешение на сварку при выполнении условий safety/gating (см. `docs/SAFETY.md` / раздел 6).
  - `0` — запрет сварки; PWM OFF не позже чем через **2 периода PWM**. Controlled stop (спад `I_ref_used`) допускается, но всегда ограничен `soft-timeout/hard-timeout` политикой.
- `I_ref_cmd`: командная уставка тока. Единица mA, диапазон `0…I_ref_max_mA`.
  - Если в течение одного периода PWM пришло несколько валидных `CMD_WELD`, действующей считается **последняя валидная**; изменение уставки учитывается fast loop на **следующей** границе периода PWM.
- `max_slew_rate_A_ms`: ограничитель скорости изменения уставки (dI/dt) в **A/мс**; при значении `0` применяется default `max_slew_rate_default_A_ms`.
- `fault_reset`: запрос на снятие latch/восстановление (применимо только в `state=FAULT` и только при выполнении условий recovery; см. `docs/SAFETY.md` / раздел 5).
- `flags/crc/reserved*`: MUST=0; любое ненулевое значение трактуется как несовместимость/ошибка формирования кадра и ведёт к REJECT.

### 3.2 Примечание про целостность
В EtherCAT отдельный payload CRC не используется; целостность обеспечивает EtherCAT на уровне транспорта. Поле `crc` в кадре `CMD_WELD` в текущей версии зарезервировано и MUST=0.

### 3.3 Запрет tuning по полевому интерфейсу
Настройка коэффициентов регулятора тока (и связанных tuning-параметров) по EtherCAT **запрещена** в рамках PDO-профиля:
- по EtherCAT PDO допускаются только циклические команды управления и статус,
- запись/изменение tuning выполняется только через сервисное ПО по UART (PCcom), см. `docs/protocols/PCCOM4.02_PROJECT.md` / 2.1.

### 3.4 Байтовая раскладка payload (`CMD_WELD`, 16 байт, LE)
Переносится из `docs/protocols/PROTOCOL_TK.md` / 3.4.

| Bytes | Field | Type | Units |
|---|---|---|---|
| 0..1 | `seq` | u16 | - |
| 2 | `mode` | u8 | - |
| 3 | `enable` | u8 | - |
| 4..7 | `I_ref_cmd` | i32 | mA |
| 8..9 | `max_slew_rate_A_ms` | u16 | A/мс |
| 10 | `fault_reset` | u8 | - |
| 11 | `flags` | u8 | MUST=0 |
| 12 | `crc` | u8 | MUST=0 |
| 13 | `reserved0` | u8 | MUST=0 |
| 14..15 | `reserved1` | u16 | MUST=0 |

---

## 4) Ответ Источник → ТК (`FB_STATUS`, TxPDO)

### 4.1 Поля
Переносится из `docs/protocols/PROTOCOL_TK.md` / 4.1 (без привязки к CAN).

- `seq_applied` (подтверждение seq): последнее принятое и защёлкнутое (APPLY) `seq` для “последней валидной” команды (command latch)
- `state` (текущее состояние): IDLE/ARMED/WELD/FAULT
- `status_word` (битовое слово): **u16** (Draft 0.2)
  - bit0 `READY`
  - bit1 `CMD_REJECTED`
  - bit2 `COMMS_SOFT_TIMEOUT_ACTIVE`
  - bit3 `COMMS_HARD_TIMEOUT_ACTIVE`
  - bit4 `BUS_OFF_ACTIVE` (CAN-specific; для EtherCAT должно оставаться 0, пока не введён отдельный флаг link-down)
  - bit5 `ADC_INVALID`
  - bit6 `CTRL_OVERRUN`
  - bit7 `MANUAL_DUTY_ACTIVE`
  - bit8 `SEQ_GAP_DETECTED`
- `fault_word` (битовое слово): **u16** (Draft 0.2, только faults)
  - bit0 `DRIVER_FAULT`
  - bit1 `HW_TRIP`
  - bit2 `ADC_FAULT`
  - bit3 `COMMS_TIMEOUT_HARD`
  - bit4 `CTRL_OVERRUN`
  - bit5 `OVERTEMP`
- `limit_word` (битовое слово): **u16** (Draft 0.2, только limits)
  - bit0 `LIMIT_DUTY`
  - bit1 `LIMIT_DI_DT`
  - bit2 `LIMIT_BY_VS` (volt-seconds)
  - bit3 `SATURATION_SUSPECTED`
- `fault_code` (u16): enum “последней причины” (для логов/тестов), `0=NONE` (см. раздел 6)

Инварианты интерпретации (для ТК), перенос из `docs/protocols/PROTOCOL_TK.md`:
- `fault_word != 0` ⇒ `READY` MUST=0 и сварка запрещена (даже если `enable=1`).
- `COMMS_HARD_TIMEOUT_ACTIVE=1` ⇒ сварка запрещена; как минимум `fault_word.COMMS_TIMEOUT_HARD` должен отражать hard-timeout по политике.
- `MANUAL_DUTY_ACTIVE=1` ⇒ сварка штатным контуром запрещена.

### 4.1.1 Пояснения к битам (семантика)
Переносится из `docs/protocols/PROTOCOL_TK.md` / 4.1.1:
- bit0 `READY`: 1, если сварка **может** быть разрешена при `enable=1` и отсутствии faults/таймаутов (не “сварим прямо сейчас”).
- bit1 `CMD_REJECTED`: 1, если **последний** принятый на вход `CMD_WELD` был отвергнут валидатором. Сбрасывается при **следующем** успешно принятом/защёлкнутом (APPLY) `CMD_WELD`.
- bit2 `COMMS_SOFT_TIMEOUT_ACTIVE`: 1, если активен soft-timeout (нет валидных `CMD_WELD` дольше `soft-timeout`). Сбрасывается при получении валидного `CMD_WELD`.
- bit3 `COMMS_HARD_TIMEOUT_ACTIVE`: 1, если активен hard-timeout (нет валидных `CMD_WELD` дольше `hard-timeout`). Сбрасывается только после восстановления связи и выполнения политики выхода из hard-timeout.
- bit5 `ADC_INVALID`: 1, если измерения объявлены невалидными диагностикой и сварка должна быть запрещена политикой.
- bit6 `CTRL_OVERRUN`: 1, если был overrun критического цикла управления в текущем отчётном окне.
- bit7 `MANUAL_DUTY_ACTIVE`: 1, если активен сервисный режим `ManualDuty` (см. раздел 8).
- bit8 `SEQ_GAP_DETECTED`: 1, если **последний принятый/защёлкнутый (APPLY)** `CMD_WELD` имел gap по `seq` (пропуск вперёд). Сбрасывается при APPLY кадра с `delta==1`.

### 4.1.2 Байтовая раскладка payload (`FB_STATUS`, 48 байт, LE)
Переносится из `docs/protocols/PROTOCOL_TK.md` / 4.1.2.

| Bytes | Field | Type | Units |
|---|---|---|---|
| 0..1 | `seq_applied` | u16 | - |
| 2 | `state` | u8 | - |
| 3 | `reserved0` | u8 | MUST=0 |
| 4..5 | `status_word` | u16 | - |
| 6..7 | `fault_word` | u16 | - |
| 8..9 | `limit_word` | u16 | - |
| 10..11 | `fault_code` | u16 | - |
| 12..15 | `I_ref_used` | i32 | mA |
| 16..17 | `duty_used_permille` | u16 | permille |
| 18..21 | `I_per` | i32 | mA |
| 22..23 | `U_per` | u16 | 0.1 В |
| 24..25 | `reserved_power` | u16 | MUST=0 |
| 26..27 | `cnt_cmd_reject` | u16 | - |
| 28..29 | `cnt_seq_gap` | u16 | - |
| 30..31 | `cnt_adc_fault` | u16 | - |
| 32..33 | `cnt_comms_fault` | u16 | - |
| 34..35 | `cnt_ctrl_overrun` | u16 | - |
| 36..37 | `cnt_log_overrun` | u16 | - |
| 38..47 | `reserved_tail` | u8[10] | MUST=0 |

### 4.2 Правила формирования
Переносится из `docs/protocols/PROTOCOL_TK.md` / 4.2:
- Частота ответа: **4 кГц** (250 мкс, синхронно с командным доменом)
- В FAULT: `state=FAULT`, актуальные fault/limit слова, диагностические счётчики, последняя причина.
- В статусе отдавать агрегированные по периоду значения (`I_per`, `U_per`). При PWM < 4 кГц допускается повторение `I_per/U_per` в нескольких подряд статусах (обновление 1 раз на период PWM).

### 4.3 Сообщение `FAULT` (опционально, `TxPDO_FAULT`, 16 байт, LE)
Переносится из `docs/protocols/PROTOCOL_TK.md` / 4.3 с адаптацией “CAN-message → опциональный PDO”.

Назначение: немедленное уведомление о критичной аварии (в дополнение к `FB_STATUS` 4 кГц).

Поля:
- `seq_applied` (u16)
- `state` (u8, типично `FAULT`)
- `reserved0` (u8, MUST=0)
- `fault_word` (u16)
- `fault_code` (u16)
- `fault_time_ms` (u32, ms since boot; `0` если нет timebase)
- `fault_context` (u32, Draft 0.2.2 = 0)

Байтовая раскладка payload (`FAULT`, 16 байт, LE):
| Bytes | Field | Type | Units |
|---|---|---|---|
| 0..1 | `seq_applied` | u16 | - |
| 2 | `state` | u8 | - |
| 3 | `reserved0` | u8 | MUST=0 |
| 4..5 | `fault_word` | u16 | - |
| 6..7 | `fault_code` | u16 | - |
| 8..11 | `fault_time_ms` | u32 | ms |
| 12..15 | `fault_context` | u32 | - |

Примечание (интеграция):
- В EtherCAT нет “событийных кадров” как в CAN; `TxPDO_FAULT` — опциональный второй PDO и должен быть согласован в ESI/mapping.
- Если `TxPDO_FAULT` не используется, уведомление о fault опирается на `FB_STATUS.fault_word/state` и изменение `fault_code`.

---

## 5) Политика ошибок и таймаутов
Переносится из `docs/protocols/PROTOCOL_TK.md` / 5 с адаптацией “bus-off → link-down/нет обновлений PDO”.

- Потеря команд:
  - soft-timeout → controlled stop (спад `I_ref_used`) + status flag
  - hard-timeout → запрет сварки + FAULT (по политике latch)
- Сервисный режим `ManualDuty` (если включён): отсутствие сервис-команд/keepalive по интерфейсу активации > **20 мс** ⇒ запрет сварки + переход в safe state (см. раздел 8 и `docs/design-notes/DN-002_MFDC_ManualDuty_Service_Mode.md`)
- Невалидная команда/формат:
  - `reserved != 0`, несовместимый `mode`, `seq` назад/повтор, out-of-range ⇒ кадр не применять; счётчик + `status_word.CMD_REJECTED=1`, `fault_code=CMD_INVALID` (если применимо).
- Пропуски seq:
  - пропуск — применить новый, установить `SEQ_GAP_DETECTED` и увеличить `cnt_seq_gap`.
- Потеря EtherCAT-связи / нет обновлений process data:
  - трактовать как “потеря команд” для watchdog (soft/hard timeouts),
  - выставлять `COMMS_*_TIMEOUT_ACTIVE` и на hard-timeout формировать `fault_word.COMMS_TIMEOUT_HARD`.

---

## 6) Коды ошибок (fault_code) и битовые слова (`status_word`/`fault_word`/`limit_word`)
Переносится из `docs/protocols/PROTOCOL_TK.md` / 6.

`fault_code` — “последняя причина” для удобства логов/тестов; основная истина остаётся в `fault_word`.

Минимальная каноника SW-0 (Draft 0.2.2, u16):
| Code | Name |
|---:|---|
| 0 | `NONE` |
| 1 | `DRIVER_FAULT` |
| 2 | `HW_TRIP` |
| 3 | `ADC_SPI_TIMEOUT` |
| 4 | `ADC_RANGE` |
| 5 | `ADC_STUCK` |
| 6 | `COMMS_TIMEOUT_HARD` |
| 7 | `COMMS_TIMEOUT_SOFT` |
| 8 | `BUS_OFF` *(CAN-specific; для EtherCAT рекомендуется не использовать)* |
| 9 | `CMD_INVALID` |
| 10 | `CTRL_OVERRUN` |
| 11 | `OVERTEMP` |
| 12 | `INCOMPATIBLE_MODE` |
| 13 | `INTERNAL_ERR` |

---

## 7) Тестовые сценарии протокола (обязательный минимум)
Переносится из `docs/protocols/PROTOCOL_TK.md` / 7 с адаптацией к EtherCAT.

Unit/host:
- Валидация reserved MUST=0 (REJECT).
- Политика `seq` (повтор/скачок назад — REJECT; пропуск — APPLY + `SEQ_GAP_DETECTED`).
- Таймауты soft/hard по отсутствию валидных `CMD_WELD`.
- Диапазоны уставки и `max_slew_rate_A_ms` (out-of-range — REJECT).

On-target измерения (GPIO/осциллограф/trace):
- Измерить задержку “новый RxPDO доступен → команда применена” (slow loop).
- Измерить `pdo_age_max_us`/“возраст команды” (распределение, максимум).
- Сравнить jitter fast loop “EtherCAT OFF vs ON”.
- Подтвердить аппаратный shutdown-path: наблюдать `BKIN_RAW` и `PWM_OUT`.

HIL/bench fault-injection (минимум 5):
1) Потеря EtherCAT линка / останов обновления PDO.
2) Пауза master/ПК на 50–200 мс.
3) Скачок `seq` назад (REJECT).
4) Пропуск `seq` вперёд (APPLY + `SEQ_GAP_DETECTED`).
5) reserved != 0 или out-of-range уставка (REJECT + статус/счётчики).

---

## 8) Сервисные команды (опционально): режим `ManualDuty`
Сервисные команды (настройка параметров, осциллограф, ручные режимы) не входят в EtherCAT PDO профиль этого документа и выполняются через USB‑UART (PCcom4) по `docs/protocols/PCCOM4.02_PROJECT.md`.

Инвариант: любые сервисные команды, способные изменить энергию/ШИМ/flash, должны быть gated safety‑политикой (только `IDLE` + подтверждённый `PWM OFF`, без активного latched fault), см. `docs/PROJECT_CONTEXT.md` и профиль PCcom4.

