# Каталог документов проекта (каноника)

Все документы проекта хранятся и актуализируются **только** в `docs/`.
Задача этого файла — быстро находить нужный документ и не таскать всё в контекст.

Примечание: папки `obsolete/` (например `docs/**/obsolete/`) — архив. Их содержимое не является каноникой и не должно использоваться как источник требований/решений.

Статус индекса: обновлено на **2026-02-19**.

---

## Safety / Functional Safety
- `docs/safety/SAFETY_CONCEPT_CBM706T_RU.md` — Safety Concept (fail-safe остановка, CBM706T как FAULT/RESET, latched-off).
- `docs/safety/SFAT_and_Timing_Budget_MFDC_ru.md` — SFAT + бюджет времени реакции (вплоть до мкс).
- `docs/safety/FMEA-001_MFDC_Source.md` — FMEA-lite (критичные отказы и контрмеры).
- `docs/reviews/MFDC_Red_Team_Design_Review.md` — red-team дизайн-ревью (где сломается в реальности).
- `docs/safety/Unified_Safety_by_Design_MFDC.md` — обзор сертификации и Safety-by-Design.
- `docs/safety/DAP_MFDC_ru.md` и `docs/safety/Design_Assurance_Package_MFDC.md` — DAP (карта safety-функций и V&V).

---

## Измерения / Контур тока / Fault model
- `docs/measurements/MEASUREMENT_ARCHITECTURE_RU.md` — архитектура измерения/усреднения/энергии (ключевой инвариант: усреднение по периоду ШИМ).
- `docs/design-notes/DN-TEMPLATE.md` — шаблон Design Note (DN) для фиксации “spec из чата” в репозитории.
- `docs/design-notes/DN-001_MFDC_Current_Control.md` — DN-001 (нормативные SHALL/SHALL NOT требования к контуру тока и контракту с ТК).
- `docs/design-notes/DN-002_MFDC_ManualDuty_Service_Mode.md` — DN-002 (сервисный режим ручной скважности ШИМ для отладки, с сохранением safety-инвариантов).
- `docs/theory/MFDC_Current_Loop_and_Fault_Model_STM32G474.md` — детализация PI+ограничения+fault-model.
- `docs/theory/MFDC_Software_Architecture_STM32G474.md` — архитектура ПО (fast/slow слои).
- `docs/theory/MFDC_Welding_Power_Source_Software_Architecture.md` — единый инженерный обзор (роль трансформатора, R_dyn(t)).

---

## Верификация / Тестирование
- `docs/verification/MFDC_Master_Document_RU.md` — master-архитектура V&V (пирамиды тестов, SIL/HIL, критерии выхода).
- `docs/verification/MFDC_Verification_Plan_RU.md` — formal verification plan (критерии PASS/FAIL и уровни).
- `docs/verification/MFDC_Red_Team_Review_RU.md` — red-team (слепые зоны SIL/HIL, аппаратные/интеграционные риски).
- `docs/verification/MFDC_SIL_First_Build_Contract_RU.md` — контракт SIL-first: host L1/L2 через CMake+CTest + интеграция с Jenkins.
- `docs/MISSING_SPECS_IMPLEMENTATION_RU.md` — перечень недостающих спецификаций (P0/P1/P2), которые нужно зафиксировать до/в процессе реализации прошивки и тестов.

---

## Skills/LLM workflow (мета)
- `docs/meta/LLM_SKILLS_GUIDE_RU.md` — принципы Ground Truth, как использовать docs/skills с Codex.
- `docs/HOW_TO_USE_RU.md` — mind map + памятка (onboarding): как пользоваться docs/ + `.codex/skills/` в проекте.
- `docs/ENGINEERING_CONTRACT.md` — общий инженерный контракт разработки (defensive design, safety/RT, proof).

---

## Datasheets / Reference
- `docs/datasheets/README.md` — как устроен каталог даташитов и правила его поддержки.
- `docs/datasheets/` — даташиты на применяемые микросхемы и модули (см. `docs/PROJECT_CONTEXT.md` / раздел 1 и `docs/GLOSSARY.md`).

---

## Архитектурные решения (ADR)
- `docs/decisions/README.md` — что такое ADR и как пользоваться.
- `docs/decisions/ADR-TEMPLATE.md` — шаблон ADR (варианты, критерии, риски, доказательства, rollback).

---

## Протоколы и интерфейсы
- `docs/protocols/obsolete/PROTOCOL_TK.md` *(устар.)* — протокол обмена с ТК по CAN (legacy/fallback): кадры/таймауты/seq/CRC/сценарии.
- `docs/protocols/PROTOCOL_TK_ETHERCAT.md` — протокол обмена с ТК по EtherCAT PDO (RxPDO/TxPDO, валидаторы/таймауты, сценарии).
- `docs/protocols/PCCOM4.02.md` — протокол PCcom4 (USB-UART плата ↔ ПК): настройка/логирование/отладка (в т.ч. обёртка legacy CAN-кадров *(устар.)*).
- `docs/protocols/PCCOM4.02_PROJECT.md` — профиль проекта поверх PCcom4: узлы/операции/форматы данных, сервисные режимы.
- `docs/protocols/obsolete/tk_protocol.dbc` *(устар.)* — DBC-описание CAN/CAN FD протокола для декодирования/сниффинга (сейчас скелет, до фиксации байтовой раскладки полей).
- `docs/protocols/obsolete/CAN_LOGGING_HOWTO_RU.md` *(устар.)* — как снимать/хранить логи CAN/CAN FD (candump/pcapng/ASC) для анализа и V&V.

---

## Логирование / Осциллографирование
- `docs/LOGGING_CAPTURE_GUIDE_RU.md` — руководство по механизму логирования/захвата (PCcom4 Scope): настройка, расширение, протокол.
- `docs/design-notes/DN-014_PSRAM_Logging_Backend_Plan.md` — план расширения логирования: PSRAM backend (draft).
- `docs/design-notes/DN-022_EtherCAT_Log_Stream_and_Multi_Capture_Spec.md` — spec: EtherCAT‑лог‑стрим + multi‑session capture.



