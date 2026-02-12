# Red-team review v2: timing_epoch guard в PSRAM QSPI (commit `66e27ed`)

## Объект ревью

Ревью выполнено по изменениям коммита `66e27ed`:
- `Fw/bsp/qspi_port.h`
- `Fw/drivers/psram_aps6404l.h`
- `Fw/drivers/psram_aps6404l.c`
- `tests/unit/psram_aps6404l_tests.c`

Контекст безопасности/RT: `docs/CONTEXT_SNAPSHOT.md` (safe state, наблюдаемость, on-target доказательства).

## Найденные риски (red-team)

Формат: **Симптом → Причина → Как воспроизвести → Как доказать → Минимальная правка**.

1. **TOCTOU между проверкой epoch и фактическим transfer**
   - Симптом: `psram_read()/psram_write()` проходят pre-check epoch, но порт может сменить timing до входа в low-level транзакцию.
   - Причина: epoch проверяется до захвата lock и до вызова портовых callback чтения/записи; в chunk-цикле re-check отсутствует.
   - Как воспроизвести: fault-injection — в mock/BSP переключать epoch после `psram_is_timing_epoch_valid()`, но до первого `read/write` callback.
   - Как доказать: GPIO на вход API + лог epoch до/после каждого chunk; фиксировать mismatch между snapshot и фактическим epoch в момент I/O.
   - Минимальная правка: повторная epoch-проверка после lock и перед каждым chunk, либо гарантированный BSP-барьер «retime only when no active clients».

2. **Lock не защищён от preemptive multi-core/ISR контекста (non-atomic)**
   - Симптом: редкие двойные владельцы/конфликт состояний при одновременных вызовах из разных контекстов.
   - Причина: `lock_active/owner_task_id` читаются/пишутся без критической секции/atomic операций.
   - Как воспроизвести: stress со вложенными вызовами из task + test-hook ISR эмуляции.
   - Как доказать: trace захвата/освобождения lock с id вызывающего контекста, поиск пересечений.
   - Минимальная правка: документально запретить ISR вызовы + защитить lock критической секцией/atomic CAS.

3. **`is_idle == NULL` трактуется как idle по умолчанию (optimistic default)**
   - Симптом: recovery завершает `READY`, хотя low-level может ещё иметь незавершённый DMA/transfer.
   - Причина: fallback в `psram_is_port_idle()` возвращает `true`, если callback не задан.
   - Как воспроизвести: BSP без `is_idle`, recovery при искусственно удерживаемом busy в HAL.
   - Как доказать: лог HAL busy-flag при `psram_recover()==OK`.
   - Минимальная правка: для safety-сборок требовать `is_idle != NULL` (или compile-time профиль strict).

4. **Нет верификации `is_idle` в `psram_init()`**
   - Симптом: init может вернуть `READY`, даже если порт не дошёл до стабильного idle после init.
   - Причина: idle-check добавлен только в recover-path.
   - Как воспроизвести: mock init, который возвращает OK, но оставляет `is_idle=false` короткое время.
   - Как доказать: GPIO/лог таймстампов `init done` vs `first transfer fail`.
   - Минимальная правка: после успешного `init` выполнять `is_idle`-barrier (симметрично recover).

5. **Недифференцированная реакция на non-idle в recover (`PSRAM_ERR_BUS`)**
   - Симптом: телеметрия путает «порт занят» с реальной bus-ошибкой.
   - Причина: non-idle мапится в `PSRAM_ERR_BUS`.
   - Как воспроизвести: recover при `is_idle=false` и отдельно реальный BUS fault.
   - Как доказать: сравнить журналы ошибок — одинаковый код при разных первопричинах.
   - Минимальная правка: ввести `PSRAM_ERR_PORT_BUSY_RECOVER` или расширить reason-code.

6. **`last_not_ready_reason` может быть stale после части ошибок/переходов**
   - Симптом: статус сообщает старую причину неготовности, не связанную с текущим отказом.
   - Причина: поле обновляется только в некоторых ветках, reset — только на успешных путях.
   - Как воспроизвести: сценарий TIMING_CHANGED → recover fail (BUS) → повторные вызовы NOT_READY.
   - Как доказать: последовательный лог `state/last_error/last_not_ready_reason` по шагам сценария.
   - Минимальная правка: централизовать обновление reason при каждом выходе с `NOT_READY`/FAULT.

7. **Проверка epoch не атомарна относительно callback `get_timing_epoch`**
   - Симптом: возможны редкие ложные совпадения/расхождения при небезопасной реализации BSP callback.
   - Причина: контракт `get_timing_epoch` не требует atomic/read barrier semantics.
   - Как воспроизвести: в BSP обновлять epoch из другого контекста без memory barrier.
   - Как доказать: stress + trace epoch reads с sequence tags.
   - Минимальная правка: прописать в `qspi_port.h` жёсткий контракт atomic read для `get_timing_epoch`.

8. **Не покрыт тест «timing changed во время chunk-loop»**
   - Симптом: при длинных операциях поведение может быть неконсистентным (часть chunks на старом timing, часть на новом).
   - Причина: unit-тесты проверяют mismatch до операции, но не mid-transfer.
   - Как воспроизвести: mock меняет epoch на N-м вызове read/write.
   - Как доказать: unit-тест на многоблочный transfer + проверка ошибок/состояния/счётчиков.
   - Минимальная правка: добавить test-case с инъекцией epoch-switch в середине transfer.

9. **Нет теста приоритета ошибок LOCKED vs TIMING_CHANGED**
   - Симптом: возможен спорный API-контракт в гонке (что возвращать первым).
   - Причина: порядок проверок фиксирован, но не зафиксирован тестом как контракт.
   - Как воспроизвести: активный lock + внешний bump epoch и конкурентный вызов.
   - Как доказать: unit matrix по порядку проверок для read/write.
   - Минимальная правка: добавить contract-tests на детерминированный приоритет возврата.

10. **ABA/переполнение epoch остаётся без защиты**
   - Симптом: при wrap-around сравнение может пропустить реальное изменение конфигурации.
   - Причина: только equality check 32-bit snapshot vs current.
   - Как воспроизвести: ускоренный fuzz со значениями около `UINT32_MAX`.
   - Как доказать: журнал epoch-переходов и результаты проверок valid/invalid.
   - Минимальная правка: 64-bit epoch или подпись конфигурации (`epoch + timing_crc`).

11. **Возможная неконсистентность статуса при отказе recover**
   - Симптом: после `recover` fail часть полей статуса сброшена/обновлена несимметрично.
   - Причина: разные ветки fail-path обновляют `state/last_error`, но не всегда reason/counters согласованы.
   - Как воспроизвести: init_status fail и non-idle fail в одном тест-прогоне.
   - Как доказать: сравнительный снимок `psram_status_t` после каждой fail-ветки.
   - Минимальная правка: единая helper-функция `psram_set_fault_status(error, reason)`.

12. **API-ломающее изменение порт-контракта без миграционного слоя**
   - Симптом: существующие BSP-адаптеры с прежним `timing_epoch` полем перестают компилироваться.
   - Причина: переход на callbacks без backward-compat shim.
   - Как воспроизвести: пересобрать legacy BSP порт.
   - Как доказать: CI job на старом BSP профиле.
   - Минимальная правка: временный shim/macro-профиль или отдельный migration note + CI guard.

## Что обязательно измерить на железе (acceptance, before merge в safety-контур)

1. Время `epoch change -> first API reject` для read/write (min/max/99.9%).
2. Латентность и jitter `psram_read()/psram_write()` до/после patch (GPIO на вход/выход API).
3. Детализация `recover start -> READY` и `recover start -> FAULT` (две ветки отдельно).
4. Проверка «no transfer while non-idle» на реальном DMA/QSPI при recover.
5. Поведение при epoch-switch в середине длинного буфера (chunked transfer).
6. Корреляция `last_error` и `last_not_ready_reason` с реальными событиями HAL.
7. Нагрузка на 250 мкс сервисный цикл: worst-case влияние PSRAM API + recovery.
8. Одновременная осциллограмма `BKIN_RAW`, `PWM_OUT`, debug GPIO маркеров PSRAM (исключить side-effects на safety path).
9. Soak-тест (длительный): ложные `TIMING_CHANGED`/`BUS` на стабильном timing.
10. Fault-injection минимум 5 сценариев: epoch bump, stuck non-idle, timeout storm, BUS storm, recovery race.

## Краткий вывод

Патч двигает дизайн в правильную сторону (явный epoch callback, reason-code, idle-check в recover), но остаются критичные зоны доказательства для RT/safety: TOCTOU вокруг epoch, strictness idle-контракта, mid-transfer epoch-switch и формальный контракт приоритета ошибок.
