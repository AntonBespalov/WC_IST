# Как пользоваться docs/ + `.codex/skills/` (MFDC / STM32G474)

Этот файл — “входная точка” для команды: где правда, как искать документы, как формулировать задачи, как безопасно вносить изменения.

## Ядро (читают все)

Это минимальный набор, который должен знать каждый участник проекта (onboarding). Всё остальное — “справочник по ситуации”.

1) `docs/HOW_TO_USE_RU.md` — эта памятка + mind map.
2) `docs/PROJECT_CONTEXT.md` — Ground Truth: железо/тайминги/safe state/допущения.
3) `docs/GLOSSARY.md` — единые определения терминов.
4) `docs/ARCHITECTURE.md` — границы модулей, fast/slow домены, тестируемость.
5) `docs/SAFETY.md` — политика аварий, latch/recovery, watchdog, shutdown-path, proof obligations.
6) `docs/protocols/PROTOCOL_TK.md` — обмен с ТК: кадры/таймауты/seq/CRC/сценарии.
7) `docs/protocols/PCCOM4.02.md` — PCcom4 (плата ↔ ПК по USB-UART): настройка/логирование/отладка (в т.ч. обёртка CAN-кадров).
8) `docs/TEST_PLAN.md` — минимальная регрессия и требуемые доказательства.
9) `docs/CONTEXT_SNAPSHOT.md` — короткая вставка контекста для запросов к Codex.
10) `docs/README.md` и `docs/DOCS_INDEX.md` — навигация (что где лежит).
11) `docs/CODING_STANDARD_RU.md` — стандарт кода: язык комментариев и кодировка.

## 0) Mind map (карта системы)

```text
MFDC: ПО источника сварочного тока
├─ Ground Truth (docs/)
│  ├─ PROJECT_CONTEXT.md
│  │  ├─ железо/тайминги/пины
│  │  └─ safe state (политика)
│  ├─ GLOSSARY.md
│  ├─ ARCHITECTURE.md
│  ├─ SAFETY.md
│  ├─ protocols/PROTOCOL_TK.md
│  ├─ protocols/PCCOM4.02.md
│  ├─ TEST_PLAN.md
│  └─ DOCS_INDEX.md
│     └─ deep docs
├─ ПО (высокоуровнево)
│  ├─ Fast loop (без RTOS)
│  │  ├─ измерения (SPI+DMA, усреднение по PWM)
│  │  ├─ регулятор тока (1 шаг / период PWM)
│  │  ├─ fast protections
│  │  └─ PWM+BKIN shutdown-path
│  └─ Slow loop (FreeRTOS)
│     ├─ обмен с ТК (1 мс домен)
│     ├─ диагностика/счётчики
│     ├─ логирование (не блокировать fast loop)
│     └─ recovery/команды обслуживания
├─ Safety
│  ├─ Safe State
│  │  ├─ TIM1 BKIN/BKIN2 => PWM OFF
│  │  └─ DRV_EN/INH => запрет драйверов
│  ├─ Классы аварий
│  │  ├─ C0: аппаратный трип + latch
│  │  ├─ C1: быстрый stop (timeout/measurements/overrun)
│  │  └─ C2: warning/ограничения
│  ├─ Latch & Recovery
│  └─ Watchdog
│     ├─ внутренний
│     └─ внешний CBM706T (FAULT vs RESET)
├─ Измерения
│  ├─ AD7380 (I_weld/U_weld)
│  ├─ Инвариант: mean() по периоду PWM
│  ├─ P_per = mean(I*U)
│  └─ Диагностика: stuck/sat/timeout
├─ Протокол с ТК (CAN)
│  ├─ Команда 1 мс => Статус 1 мс
│  ├─ Seq/CRC/Timeouts
│  └─ State machine: IDLE/ARMED/WELD/FAULT
├─ Протокол плата ↔ ПК (USB-UART)
│  ├─ PCcom4 (protocols/PCCOM4.02.md)
│  └─ Настройка / осциллографирование / отладка (туннель CAN)
├─ Тестирование и доказательства
│  ├─ L0..L5 (unit/SIL/on-target/HIL/bench)
│  ├─ Минимальная регрессия R1..R10
│  └─ Инструментирование (DBG_* + BKIN_RAW + PWM_OUT)
└─ Codex/LLM (skills)
   ├─ Контекст: CONTEXT_SNAPSHOT.md
   ├─ Ворота: ask-questions-embedded-stm32-freertos
   ├─ Workflow: Spec => DN => Tests/Proof => Code
   └─ Review: red-team-review-welding / strict-audit
```

Примечание: это текстовая “карта” (без Mermaid), чтобы одинаково читалась в любом просмотрщике Markdown.

---

## 1) Куда смотреть (порядок чтения)

1) `docs/README.md` и `docs/DOCS_INDEX.md` — “что где лежит”.
2) `docs/PROJECT_CONTEXT.md` — единый Ground Truth (железо/тайминги/safe state).
3) `docs/GLOSSARY.md` — термины (чтобы одинаково понимать “safe state”, “latch”, “overrun”, “BKIN”).
4) `docs/ARCHITECTURE.md` — модульные границы и тестируемость (host/SIL/HIL/on-target).
5) `docs/SAFETY.md` и `docs/safety/*` — политика аварий, latch/recovery, shutdown-path, timing budget.
6) `docs/protocols/PROTOCOL_TK.md` — обмен с ТК: кадры/таймауты/seq/CRC/тестовые сценарии.
7) `docs/TEST_PLAN.md` — уровни тестов и “минимальная регрессия” (что обязаны доказать).

---

## Справочник (по ситуации)

Если нужен “глубокий” документ — не искать по памяти, а открывать `docs/DOCS_INDEX.md` и идти по разделу:
- `docs/safety/*` — SFAT, Safety Concept (CBM706T), FMEA-lite, DAP.
- `docs/theory/*` — подробности архитектуры/контура/модели отказов.
- `docs/verification/*` — V&V master/plan, red-team.
- `docs/reviews/*` — дизайн-ревью (где “сломается в реальности”).

### Быстрый навигатор: “вопрос → куда смотреть”
- Контур тока, границы ответственности источника vs ТК: `docs/design-notes/DN-001_MFDC_Current_Control.md`.
- Архитектура ПО (fast/slow домены, модульные границы): `docs/ARCHITECTURE.md` + `docs/theory/MFDC_Software_Architecture_STM32G474.md`.
- Измерения I/U и физически корректные величины (mean по периоду PWM, `P_per = mean(I*U)`): `docs/measurements/MEASUREMENT_ARCHITECTURE_RU.md`.
- Fault-model (HARD/SOFT/LIMIT, latch policy, реакции по слоям): `docs/theory/MFDC_Current_Loop_and_Fault_Model_STM32G474.md` + `docs/SAFETY.md`.
- Safety shutdown-path (BKIN/DRV_EN, no auto-restart, watchdog, CBM706T роли): `docs/SAFETY.md` + `docs/safety/SAFETY_CONCEPT_CBM706T_RU.md`.
- Бюджеты времени реакции и распределение safety-функций (SFAT): `docs/safety/SFAT_and_Timing_Budget_MFDC_ru.md`.
- Протокол CAN с ТК (таймауты, seq, слова fault/limit/status): `docs/protocols/PROTOCOL_TK.md`.
- Протокол плата ↔ ПК (настройка/логирование/отладка, PCcom4): `docs/protocols/PCCOM4.02.md`.
- Стратегия доказательств/регрессии (unit/SIL/on-target/HIL/bench): `docs/TEST_PLAN.md` + `docs/verification/MFDC_Master_Document_RU.md`.
- Design review / “где сломается в реальности”: `docs/reviews/MFDC_Red_Team_Design_Review.md` и `docs/verification/MFDC_Red_Team_Review_RU.md`.

---

## 2) Ежедневная работа (коротко)

### 2.1 Правило “один источник правды”
- Если меняются железо/тайминги/политика аварий/интерфейсы — обновляй `docs/PROJECT_CONTEXT.md` в том же изменении.
- Термины — только через `docs/GLOSSARY.md`.
- Детали safety/таймингов — через `docs/SAFETY.md` + `docs/safety/*`.
- Детали обмена — через `docs/protocols/PROTOCOL_TK.md`.
- Детали обмена “плата ↔ ПК” — через `docs/protocols/PCCOM4.02.md`.
- Стратегия доказательства/регрессии — через `docs/TEST_PLAN.md`.

### 2.2 Процесс изменения (Gate → Spec → DN → Tests/Proof → Code → Review)
Рекомендуемый порядок для любой фичи/изменения поведения (особенно control/measurement/safety/comms):

0) **Gate (проверка полноты задачи)**  
   Если не определены: цель, границы изменений, контекст исполнения (task/ISR/DMA), временной домен, критерий “готово” — сначала использовать skill `ask-questions-embedded-stm32-freertos`.

1) **Spec (требования)**  
   Зафиксировать: что меняем, что не ломаем, критерии приёмки и инварианты.
   Быстрые якоря:
   - контур тока/контракт с ТК: `docs/design-notes/DN-001_MFDC_Current_Control.md`
   - измерения/агрегация по периоду: `docs/measurements/MEASUREMENT_ARCHITECTURE_RU.md`
   - safety shutdown-path и latch/recovery: `docs/SAFETY.md`

2) **DN (Design Note — фиксируем итог Spec “в репо”)**  
   Если изменение “не тривиальное” или затрагивает timing/safety/protocol/control/measurement — сохраняем выжимку обсуждения как Design Note в `docs/design-notes/`.

   Это основной workflow “через Codex”: обсудили задачу в чате → попросили Codex оформить итог в DN → потом по DN делаем реализацию.

   Минимум, который должен быть в DN:
   - что делаем (Decision) и что не делаем (Non-goals);
   - почему так (Rationale, ключевые ограничения/инварианты);
   - как проверить и как откатить (Test plan / Proof / Rollback);
   - ссылки на связанные документы/задачи/PR/коммиты (когда появятся).

3) **Tests/Proof (доказательства)**  
   Определить, чем доказываем корректность: unit/host, SIL, on-target измерения (GPIO/осциллограф/trace), HIL, bench.
   Для safety/таймингов — обязательно: `BKIN_RAW → PWM_OUT safe`, jitter/latency и fault-injection.
   Базовая опора: `docs/TEST_PLAN.md` и `docs/verification/MFDC_Master_Document_RU.md`.

4) **Code (минимальный патч)**  
   Делать минимальные правки, не смешивать с рефакторингом.

5) **Review**  
   После патча — отдельный проход `red-team-review-welding`; если нужен вердикт PASS/FAIL перед стендом — `strict-audit`.

### 2.3 Чек-лист “что обновить вместе с изменениями”
- PWM/TIM1/BKIN/DRV_EN/пины/тайминги/домены: `docs/PROJECT_CONTEXT.md` (+ при необходимости `docs/SAFETY.md`).
- Измерения/усреднение/энергия: `docs/measurements/MEASUREMENT_ARCHITECTURE_RU.md` (+ `docs/PROJECT_CONTEXT.md`).
- Контур тока/контракт “1 шаг на период PWM”: `docs/design-notes/DN-001_MFDC_Current_Control.md` (+ `docs/ARCHITECTURE.md` при изменении границ).
- Протокол/таймауты/коды ошибок: `docs/protocols/PROTOCOL_TK.md` (+ `docs/GLOSSARY.md` для новых терминов/состояний).
- Протокол связи с ПК (PCcom4): `docs/protocols/PCCOM4.02.md`.
- Safety/latch/recovery/watchdog: `docs/SAFETY.md` (+ `docs/safety/*` при необходимости).
- Минимальная регрессия/доказательства: `docs/TEST_PLAN.md`.

### 2.4 Как держать документы актуальными
- Любое изменение, влияющее на “территорию”, обязано обновлять “карту” (`docs/*`) в том же изменении.
- Не оставлять “TBD” в `docs/PROJECT_CONTEXT.md`, если решение уже принято/закодировано.
- При добавлении нового документа — дополнять `docs/DOCS_INDEX.md`, чтобы не потерять ссылку.

---

## 3) Как пользоваться Codex/LLM в этом проекте

### 3.1 Минимальный “контекст-стек” для задачи
- `docs/CONTEXT_SNAPSHOT.md`
- `docs/CODING_STANDARD_RU.md` (если будет патч/код: комментарии на русском, UTF-8/CRLF)
- при необходимости: `docs/PROJECT_CONTEXT.md`, `docs/ARCHITECTURE.md`, `docs/SAFETY.md`, `docs/protocols/PROTOCOL_TK.md`, `docs/protocols/PCCOM4.02.md`
- 1–3 релевантных файла кода (если задача про реализацию)

### 3.2 Обязательная мини-шапка (5 строк)
Копипаста: `docs/PROMPT_TEMPLATES.md` (раздел “Мини-шапка запроса (MUST)”).

Если хоть одна строка пустая/TBD — сначала использовать skill `ask-questions-embedded-stm32-freertos`.

### 3.3 Рекомендуемые связки skills (практика)
- Новая фича/изменение поведения: `workflow-spec-tests-code` + `test-verification-welding-short`.
- Изменение safety/восстановления/watchdog: `safety-invariants-welding` + `test-verification-welding-short`.
- Реализация “минимальным патчем”: `patch-discipline-small-diffs` + `test-verification-welding-short`.
- Ревью готового диффа: `red-team-review-welding` (и при необходимости `strict-audit`).

---

## 4) Как формулировать задачу, чтобы она “не сломала реальность”

Для embedded/силовой части почти всегда критичны:
- контекст исполнения (task/ISR/DMA),
- временной домен (PWM / 1 мс / async fault),
- измеримые доказательства (GPIO/осциллограф/trace + fault-injection),
- правило: критические аварии отключают силовую часть аппаратно (BKIN/DRV_EN), а не “по задаче”.

Шаблоны запросов: `docs/PROMPT_TEMPLATES.md`.

---

## 5) STM32CubeIDE: навигация по коду (Indexer/CDT)

Симптомы:
- не работает переход к объявлению (Open Declaration) по клику на поле структуры/символ;
- подсветка/вид кода “как будто это не C-проект”.

Причина (типовая для CubeIDE/CDT):
- папка с исходниками не добавлена в настройки проекта:
  - и/или отсутствует в include paths (`-I`), из-за чего не резолвятся `#include` и пропадает навигация;
  - и/или отсутствует как source root в `sourceEntries` файла проекта `.cproject`,
    из-за чего индексатор может не “видеть” исходники как часть проекта.

Правило:
- при добавлении кода в каталог, который сейчас не входит в build path/индексацию (например, `Fw/*`), обязательно:
  1) добавить нужный include path (`-I`) в Project Properties / `.cproject`,
  2) добавить source root в `sourceEntries` (как `kind="sourcePath"`),
  3) проверить, что изменения внесены во все активные конфигурации (обычно Debug и Release).

Проверка после правок (обязательно):
- В `Project Properties -> C/C++ General -> Paths and Symbols`:
  - вкладка `Includes`: есть путь к новому каталогу заголовков (например, `Fw/control`);
  - вкладка `Source Location`: есть source root (например, `Fw`).
- В `.cproject`:
  - есть `listOptionValue ... value=\"../Fw/control\"` (или нужный путь),
  - есть `entry ... kind=\"sourcePath\" name=\"Fw\"` (или нужный source root).

После правок:
- `Project -> C/C++ Index -> Rebuild` (перестроить индекс).
