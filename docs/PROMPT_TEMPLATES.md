# Шаблоны запросов к Codex/LLM (под этот проект)

Используй `docs/CONTEXT_SNAPSHOT.md` как короткую вставку контекста.
Если задача safety/тайминги/измерения/обмен — подключай соответствующие skills.

---

## Мини-шапка запроса (MUST) — копипаста в начало любого промпта
Заполни 5 строк. Если не можешь заполнить хотя бы одну — сначала запускай ворота.

- Цель (и что НЕ менять):
- Разрешённые файлы/модули:
- Контекст исполнения (task/ISR/DMA):
- Временной домен (PWM / 1 мс / async fault / другое):
- Done/Proof (как проверяем и что доказываем):

Если любая строка пустая или “TBD”:
→ сначала используй skill `ask-questions-embedded-stm32-freertos` и ответь в формате `defaults` или `0a 1b ...`.

---

## Шаблон 0 — “Ленивая работа с контекстом” (вставка в любой запрос)
> Используй `docs/PROJECT_CONTEXT.md` и `docs/GLOSSARY.md` как источники правды.  
> Работай “лениво”: **не перепечатывай** документы целиком; ссылайся на секции/названия.  
> Если информации не хватает — перечисли минимальные вопросы или явные допущения.

> Если mini-шапка MUST не заполнена (есть пустые/TBD) — сначала запусти `ask-questions-embedded-stm32-freertos`, затем возвращайся к шаблону.


---

## Шаблон 1 — Spec → DN → Tests/Proof → Code (надёжный режим)
**Skills:** `workflow-spec-tests-code`, (если код) `patch-discipline-small-diffs`, (если safety) `safety-invariants-welding`

> Используй skills: `workflow-spec-tests-code` и `test-verification-welding-short`.  
> Контекст (кратко): [вставь `docs/CONTEXT_SNAPSHOT.md`].  
> Задача: <что хотим изменить>.  
> Ограничения: <что нельзя менять / какие файлы трогать можно>.  
> Выход:  
> 1) Spec (требования, критерии приёмки, инварианты),  
> 2) DN draft (выжимка “что/как/почему + test plan/rollback” для `docs/design-notes/`),  
> 3) Tests/Proof (unit/on-target/HIL/fault-injection + что мерить осциллографом),  
> 4) Code plan (файлы + план коммитов).  
> Код НЕ пиши, пока я не попрошу.

> Mini-шапка MUST заполнена выше; если нет — сначала `ask-questions-embedded-stm32-freertos`.



---

## Шаблон 2 — Реализация минимальным патчем
**Skills:** `patch-discipline-small-diffs`, `test-verification-welding-short` (+ `safety-invariants-welding` при необходимости)

> Используй skills: `patch-discipline-small-diffs` и `test-verification-welding-short`.  
> Контекст: [вставь `docs/CONTEXT_SNAPSHOT.md`].  
> Реализуй: <конкретная фича/фикс>.  
> Разрешённые файлы: <список>.  
> Запрещено: рефакторинг/переименования/форматирование вне нужного.  
> Выход:  
> - кратко: какие файлы изменил и почему,  
> - патч/код,  
> - какие тесты добавить/обновить + критерии приёмки.

> Mini-шапка MUST заполнена выше; если нет — сначала `ask-questions-embedded-stm32-freertos`.

---

## Шаблон 3 — Red-team review готового PR/патча
**Skills:** `red-team-review-welding` (+ `safety-invariants-welding`)

> Используй skill: `red-team-review-welding`.  
> Контекст: [вставь `docs/CONTEXT_SNAPSHOT.md`].  
> Вот дифф/PR: <вставь изменения>.  
> Задача: найти проблемы (тайминги/гонки/отказы/нарушение safety-инвариантов).  
> Выход:  
> - 10+ рисков: симптом, причина, как воспроизвести (fault-injection/тест), как доказать (GPIO/лог), минимальная правка.  
> - отдельным списком: что обязательно измерить на железе (BKIN_RAW→PWM_OUT, jitter, ADC latency и т.п.).

> Mini-шапка MUST заполнена выше; если нет — сначала `ask-questions-embedded-stm32-freertos`.

---

## Шаблон 4 — План HIL для конкретного модуля/фичи
**Skills:** `test-verification-welding-short` (+ `workflow-spec-tests-code`)

> Используй skill: `test-verification-welding-short`.  
> Контекст: [вставь `docs/CONTEXT_SNAPSHOT.md`].  
> Нужен план HIL для: <что тестируем>.  
> Дай:  
> - какие сигналы эмулировать (ток/напряжение/ошибки),  
> - сценарии (норма, плохой контакт, перегрузка, stuck/sat/timeout),  
> - критерии приёмки,  
> - какие линии аварий дёргать (BKIN/DRV_EN),  
> - что логировать (record/replay).

> Mini-шапка MUST заполнена выше; если нет — сначала `ask-questions-embedded-stm32-freertos`.

---

## Шаблон 5 — Протокол ТК (дизайн/изменение)
**Skills:** `workflow-spec-tests-code`, `patch-discipline-small-diffs` (если кодогенерация)

> Используй `docs/PROTOCOL_TK.md` как источник правды (или заполни шаблон, если файла ещё нет).  
> Контекст: [вставь `docs/CONTEXT_SNAPSHOT.md`].  
> Задача: <определить/изменить поля/таймауты/коды ошибок>.  
> Выход:  
> - изменения в `docs/PROTOCOL_TK.md` (в виде предложенного текста),  
> - список тестов протокола (timeout/seq/crc/bus-off),  
> - совместимость с текущей реализацией (что сломается).

> Mini-шапка MUST заполнена выше; если нет — сначала `ask-questions-embedded-stm32-freertos`.

---

Шаблон 6 — Жесткий Аудит / Safety Gate

**Skills**: `strict-audit`, `red-team-review-welding`, `safety-invariants-welding`

> Используй skills: `strict-audit` и `red-team-review-welding`.
> Контекст: [вставь `docs/CONTEXT_SNAPSHOT.md`].
> Вводные данные: <код, архитектурное решение или PR>.
> Задача: Провести аудит с нулевой толерантностью к рискам.
> Выход:
> - Статус: [AUDIT PASS] / [AUDIT FAIL]
> - Блокирующие замечания (No-Go).
> - Требования к исправлению.

> Mini-шапка MUST заполнена выше; если нет — сначала `ask-questions-embedded-stm32-freertos`.
