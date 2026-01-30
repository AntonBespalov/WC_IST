# Skills (проектные правила для Codex/LLM)

Эта папка содержит “skills” — короткие инструкции (про правила), которые задают стиль мышления и обязательные акценты при разработке и ревью.
Skills версионируются вместе с проектом.

## Источники правды
Перед использованием skills ориентируйся на:
- `docs/PROJECT_CONTEXT.md` — железо, тайминги, интерфейсы, политика safe state (Ground Truth)
- `docs/GLOSSARY.md` — единые определения терминов
- `docs/ARCHITECTURE.md` — модульные границы и тестируемость (host/SIL/HIL/on-target)
- `docs/SAFETY.md` — классы аварий, latch/recovery, watchdog, shutdown-path
- `docs/PROTOCOL_TK.md` — протокол обмена с ТК
- `docs/TEST_PLAN.md` — уровни тестирования и регрессия

Если skill противоречит контексту/словарю/архитектуре/safety — приоритет у документов из `docs/`, либо предложи правку.

---

## Как использовать
В запросе к Codex указывай явно:
- **“Используй skill `<name>`”** (или несколько skills)
- цель задачи и границы изменений (“разрешённые файлы”, “не трогать лишнее”)

Для крупных задач обычно достаточно 1–2 skills одновременно. Для критичных изменений — допускается 2–3 (workflow + test + patch).

---

## Актуальные skills

### 0) `ask-questions-embedded-stm32-freertos`
**Файл:** `skills/ask-questions-embedded-stm32-freertos.md`  
**Когда применять:** если задача недоописана (Gate M1..M5) или затрагивает safety/тайминги/ISR/DMA/обмен.  
**Что делает:** задаёт минимальные must-have вопросы и не позволяет начинать реализацию без ответов или явных допущений.

### 1) `workflow-spec-tests-code`
**Файл:** `skills/workflow-spec-tests-code.md`  
**Когда применять:** новая фича/изменение поведения (особенно control/measurement/safety/comms).  
**Что делает:** принуждает к процессу **Spec → Tests/Proof → Code** (код — только по явному запросу).

### 2) `test-verification-welding-short`
**Файл:** `skills/test-verification-welding-short.md`  
**Когда применять:** любые изменения в управлении током, измерениях, авариях, таймингах, обмене.  
**Что делает:** требует измеримых критериев, плана доказательств, fault-injection и инструментирования таймингов (GPIO/trace).

### 3) `safety-invariants-welding`
**Файл:** `skills/safety-invariants-welding.md`  
**Когда применять:** всё, что затрагивает safe state / BKIN / DRV_EN / recovery / watchdog / разрешение сварки.  
**Что делает:** фиксирует “неприкасаемые” инварианты (hardware shutdown-path, no auto-restart, no blind welding) и требует доказательств.

### 4) `patch-discipline-small-diffs`
**Файл:** `skills/patch-discipline-small-diffs.md`  
**Когда применять:** любая кодогенерация/правки кода.  
**Что делает:** удерживает изменения маленькими, запрещает “скрытые рефакторинги”, задаёт план коммитов.

### 5) `red-team-review-welding`
**Файл:** `skills/red-team-review-welding.md`  
**Когда применять:** после патча/PR — особенно для критичных модулей.  
**Что делает:** “враждебное” ревью: гонки, тайминги, отказные сценарии, нарушения safety-инвариантов. Не переписывает код, а ищет проблемы и требует проверки.

### 6) `strict-audit`
**Файл:** `skills/strict-audit.md`  
**Когда применять:** когда нужен “No-Go” аудит (safety gate) перед стендом/силовой частью или при спорных решениях.  
**Что делает:** выдаёт [AUDIT PASS]/[AUDIT FAIL], блокирующие замечания и что измерить/доказать.

---

## Рекомендуемые связки skills (быстро и надёжно)
- **Новая фича / изменение поведения:**  
  `workflow-spec-tests-code` + `test-verification-welding-short`
- **Реализация кода минимальным патчем:**  
  `patch-discipline-small-diffs` + `test-verification-welding-short`
- **Любые изменения в shutdown/recovery/watchdog:**  
  `safety-invariants-welding` + `test-verification-welding-short` (+ `patch-discipline-small-diffs` если пишем код)
- **После реализации (ревью):**  
  `red-team-review-welding`

---

## Дополнительные (опциональные) skills
Использовать по ситуации, обычно не больше 1 вместе с “ядром”.

- `skills/_optional/test-verification-welding-long.md` — расширенная версия верификации (HIL/SIL/DFT/record-replay), когда нужен “полный” evidence plan.
- `skills/_optional/debug-scientific-embedded.md` — научный режим отладки (наблюдение → гипотеза → инструментирование → вывод).
- `skills/_optional/complexity-analysis.md` — аудит RT/сложности (ISR/stack/blocking).
- `skills/_optional/docs-maintainer.md` — подсказка, какие `docs/*` обновить после изменения кода/архитектуры.
- `skills/_optional/system-design-interrogation.md` — интерактивное прояснение требований (вопросы по одному, drafting Spec секциями).

---

## Шаблон для добавления нового skill
1) Название файла: `skills/<kebab-case>.md`
2) YAML-шапка: `name`, `version`, `description`, `tags`, (опц.) `project_context`, `glossary`, `when_to_use`, `outputs`
3) Skill должен быть коротким (≤ 1–2 страниц) и содержать:
   - миссию
   - MUST/SHOULD
   - контракт вывода (что модель обязана выдать)
   - “нельзя”
4) При изменениях:
   - повышай `version`
   - в PR укажи причину (ссылка на проблему/инцидент/решение)

---

## Принципы поддержки skills
- Skills — это правила работы, а не документация по железу.
- Данные и “источники правды” живут в `docs/`.
- Любое изменение политики аварий/таймингов должно обновлять и код, и `docs/PROJECT_CONTEXT.md` (+ при необходимости `docs/SAFETY.md`).
