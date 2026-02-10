# DN-XXX_<short_name> — <title> (шаблон DN)

Статус: draft  
Дата: YYYY-MM-DD  
Владелец: <name>  
Связано: <issue/PR/commit/links>  

---

## 1) Context / Problem
- Что меняем и зачем (в 3–7 строк).
- В каком контексте исполнения это живёт (ISR/task/DMA), какой временной домен (PWM/1ms/async fault).

## 2) Goal / Non-goals
### Goal
- …

### Non-goals
- …

## 3) Decision (что делаем)
- …

## 4) Rationale (почему так)
- Ключевые аргументы/ограничения/инварианты.
- Какие варианты были и почему не они (если есть).

## 5) Interfaces / Data / Timing impact
- Сигналы/поля/структуры данных, которые добавляем/меняем.
- Тайминги/джиттер/бюджет ISR (если применимо).
- Обратная совместимость (если применимо).

## 6) Risks / Edge cases
- Что может пойти не так и как это ловим.

## 7) Test plan / Proof / Rollback
### Test plan / Proof
- Unit/host:
- SIL:
- On-target измерения (GPIO/осциллограф/trace):
- HIL/bench:

### Rollback
- Как быстро вернуться в безопасное состояние/предыдущую версию поведения.

## 8) Status / Implementation links
- Status: draft | accepted | implemented | obsolete
- Links: PR/commit/issue
