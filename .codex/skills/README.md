# Skills (проектные правила для Codex/LLM)

Эта папка содержит “skills” — короткие инструкции (про правила), которые задают стиль мышления и обязательные акценты при разработке и ревью.
Skills версионируются вместе с проектом.

Формат хранения: каждый skill — это папка `.codex/skills/<skill-name>/` с файлом `.codex/skills/<skill-name>/SKILL.md`.

Примечание по терминам: “Spec” в этом репозитории — это шаг процесса (формулирование требований и критериев приёмки), а не обязательно отдельный файл.
Фиксация результата Spec обычно делается как DN в `docs/design-notes/` (см. `docs/HOW_TO_USE_RU.md` / раздел “Процесс изменения” и `docs/PROMPT_TEMPLATES.md`).

## Источники правды
Перед использованием skills ориентируйся на:
- `docs/PROJECT_CONTEXT.md` — железо, тайминги, интерфейсы, политика safe state (Ground Truth)
- `docs/GLOSSARY.md` — единые определения терминов
- `docs/ARCHITECTURE.md` — модульные границы и тестируемость (host/SIL/HIL/on-target)
- `docs/SAFETY.md` — классы аварий, latch/recovery, watchdog, shutdown-path
- `docs/protocols/PROTOCOL_TK.md` — протокол обмена с ТК (CAN)
- `docs/protocols/PCCOM4.02.md` — протокол плата ↔ ПК (USB-UART, PCcom4)
- `docs/protocols/PCCOM4.02_PROJECT.md` — профиль проекта поверх PCcom4 (узлы/операции/форматы)
- `docs/TEST_PLAN.md` — уровни тестирования и регрессия

Если skill противоречит контексту/словарю/архитектуре/safety — приоритет у документов из `docs/`, либо предложи правку.

---

## Как использовать
В запросе к Codex указывай явно:
- **“Используй skill `<name>`”** (или несколько skills)
- цель задачи и границы изменений (“разрешённые файлы”, “не трогать лишнее”)

Для крупных задач обычно достаточно 1–2 skills одновременно. Для критичных изменений — допускается 2–3 (workflow + test + patch).

---

## Когда создавать ADR vs DN (порядок)
Это определяется **вручную**, но по простому правилу (и это уже отражено в `when_to_use` внутри конкретных skills и в `docs/HOW_TO_USE_RU.md`):

1) **Есть “решение” с вариантами?** (2+ правдоподобных подхода, компромиссы, выбор влияет на интерфейсы/тайминги/safety/протокол)
   → делаем **ADR** через `adr-writer` (почему + варианты + критерии + риски + proof/rollback), затем продолжаем Spec-шагом.

2) **Нужно зафиксировать результат Spec/обсуждения в репозитории перед кодом/ревью?**
   → делаем **DN** через `dn-writer` (Goal/Non-goals, Decision/Rationale, impact, risks, Test plan/Proof/Rollback).

3) **Если выбора нет** (правка очевидна/локальна, без архитектурных развилок)
   → ADR можно пропустить и идти **Gate → Spec → DN → Tests/Proof → Code** (см. `workflow-spec-tests-code` и `docs/PROMPT_TEMPLATES.md`).

Практическое правило: **ADR нужен, когда через месяц будет вопрос “почему выбрали именно так?”**. DN нужен, когда будет вопрос “что именно сделали/проверили и как откатить?”.

## Актуальные skills

### 0) `ask-questions-embedded-stm32-freertos`
**Файл:** `.codex/skills/ask-questions-embedded-stm32-freertos/SKILL.md`  
**Когда применять:** если задача недоописана (Gate M1..M5) или затрагивает safety/тайминги/ISR/DMA/обмен.  
**Что делает:** задаёт минимальные must-have вопросы и не позволяет начинать реализацию без ответов или явных допущений.

### 1) `workflow-spec-tests-code`
**Файл:** `.codex/skills/workflow-spec-tests-code/SKILL.md`  
**Когда применять:** новая фича/изменение поведения (особенно control/measurement/safety/comms).  
**Что делает:** принуждает к процессу **Spec → Tests/Proof → Code** (код — только по явному запросу).

### 2) `adr-writer`
**Файл:** `.codex/skills/adr-writer/SKILL.md`  
**Когда применять:** когда нужно принять архитектурное решение или выбрать вариант реализации (2+ правдоподобных подхода, важны критерии выбора).  
**Что делает:** оформляет ADR строго по `docs/decisions/ADR-TEMPLATE.md` (контекст, варианты, критерии, риски, tests/proof, rollback).

### 3) `dn-writer`
**Файл:** `.codex/skills/dn-writer/SKILL.md`  
**Когда применять:** после обсуждения требований/архитектуры, чтобы зафиксировать решение в `docs/design-notes/` перед реализацией/ревью.  
**Что делает:** оформляет DN строго по `docs/design-notes/DN-TEMPLATE.md` (Goal/Non-goals, Decision/Rationale, impact, risks, Test plan/Proof/Rollback).

### 4) `test-verification-welding-short`
**Файл:** `.codex/skills/test-verification-welding-short/SKILL.md`  
**Когда применять:** любые изменения в управлении током, измерениях, авариях, таймингах, обмене.  
**Что делает:** требует измеримых критериев, плана доказательств, fault-injection и инструментирования таймингов (GPIO/trace).

### 5) `safety-invariants-welding`
**Файл:** `.codex/skills/safety-invariants-welding/SKILL.md`  
**Когда применять:** всё, что затрагивает safe state / BKIN / DRV_EN / recovery / watchdog / разрешение сварки.  
**Что делает:** фиксирует “неприкасаемые” инварианты (hardware shutdown-path, no auto-restart, no blind welding) и требует доказательств.

### 6) `patch-discipline-small-diffs`
**Файл:** `.codex/skills/patch-discipline-small-diffs/SKILL.md`  
**Когда применять:** любая кодогенерация/правки кода.  
**Что делает:** удерживает изменения маленькими, запрещает “скрытые рефакторинги”, задаёт план коммитов.

### 7) `red-team-review-welding`
**Файл:** `.codex/skills/red-team-review-welding/SKILL.md`  
**Когда применять:** после патча/PR — особенно для критичных модулей.  
**Что делает:** “враждебное” ревью: гонки, тайминги, отказные сценарии, нарушения safety-инвариантов. Не переписывает код, а ищет проблемы и требует проверки.

### 8) `strict-audit`
**Файл:** `.codex/skills/strict-audit/SKILL.md`  
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

- `.codex/skills/test-verification-welding-long/SKILL.md` — расширенная версия верификации (HIL/SIL/DFT/record-replay), когда нужен “полный” evidence plan.
- `.codex/skills/debug-scientific-embedded/SKILL.md` — научный режим отладки (наблюдение → гипотеза → инструментирование → вывод).
- `.codex/skills/complexity-analysis/SKILL.md` — аудит RT/сложности (ISR/stack/blocking).
- `.codex/skills/docs-maintainer/SKILL.md` — подсказка, какие `docs/*` обновить после изменения кода/архитектуры.
- `.codex/skills/system-design-interrogation/SKILL.md` — интерактивное прояснение требований (вопросы по одному, drafting Spec секциями; код — только после “Spec утвержден”).

---

## Шаблон для добавления нового skill
1) Имя skill: `<kebab-case>`
2) Папка: `.codex/skills/<kebab-case>/`
3) Файл: `.codex/skills/<kebab-case>/SKILL.md`
4) YAML-шапка: `name`, `version`, `description`, `tags`, (опц.) `project_context`, `glossary`, `when_to_use`, `outputs`
5) Skill должен быть коротким (≤ 1–2 страниц) и содержать:
   - миссию
   - MUST/SHOULD
   - контракт вывода (что модель обязана выдать)
   - “нельзя”
6) При изменениях:
   - повышай `version`
   - в PR укажи причину (ссылка на проблему/инцидент/решение)

---

## Принципы поддержки skills
- Skills — это правила работы, а не документация по железу.
- Данные и “источники правды” живут в `docs/`.
- Любое изменение политики аварий/таймингов должно обновлять и код, и `docs/PROJECT_CONTEXT.md` (+ при необходимости `docs/SAFETY.md`).
