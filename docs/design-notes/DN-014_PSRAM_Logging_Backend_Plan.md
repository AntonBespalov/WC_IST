# DN-014 — План расширения логирования: PSRAM backend

**Статус:** Draft (план, без реализации)  
**Основано на:** `DN-011_Service_Logging_Capture_Framework.md`, `DN-012_PCCOM4_Emu_Logging_Capture.md`  
**Контекст:** см. `docs/PROJECT_CONTEXT.md` / разделы 1–2, 5; термины — `docs/GLOSSARY.md`

---

## 1) Цель
Расширить механизм логирования/захвата для хранения окна в PSRAM без влияния на fast‑контур и без операций QSPI в ISR/fast‑loop.

## 2) Границы
- Не менять fast‑контур управления и аппаратный shutdown‑path.
- Не менять формат PCcom4 без отдельного апдейта протокола.

## 3) Решение (план)
### 3.1. Архитектура
- Ввести **storage‑interface** (vtable) для capture‑хранилища.
- Оставить текущий SRAM backend как default.
- Добавить PSRAM backend с **SRAM‑буфером для fast‑push** и flush‑задачей в slow‑домене.

### 3.2. Backend‑интерфейс (без кода)
Минимальный набор операций:
- `init(buffer, size_bytes)`
- `capacity_samples(bytes_per_sample)`
- `push(sample_bytes, bytes_per_sample)` (fast)
- `read(start_sample, count, bytes_per_sample, out)` (slow)
- `set_frozen(bool)`

### 3.3. Fallback‑флаг (предложение)
Добавить `CAPTURE_META_FLAG_STORAGE_FALLBACK` (бит 3) в `CaptureMeta.flags`
для фиксации падения на SRAM при ошибке PSRAM.

---

## 4) План миграции (минимальный дифф)
1) Добавить storage‑interface и обвязать `capture_core` через него.
2) Не менять поведение SRAM backend (регрессии ≈ 0).
3) Реализовать PSRAM backend + SRAM‑буфер.
4) Подключить конфигурацию выбора backend в `capture_core_cfg_t`.
5) Опционально добавить флаг fallback и обновить `PCCOM4.02_PROJECT.md`.

---

## 5) Риски
- Рост джиттера fast‑контуров при неправильной изоляции QSPI.
- Пропускная способность QSPI ниже требуемой при max sample_rate.
- Потеря данных при переполнении SRAM‑буфера (нужны счётчики/метки).

---

## 6) Доказательства / проверки
- Осциллограф: jitter PWM, время ISR/fast‑loop с PSRAM.
- Логика/trace: время flush‑задачи и устойчивость к burst нагрузке.
- Fault‑injection: недоступность PSRAM → корректный fallback + флаг.

---

## 7) Rollback
Отключить PSRAM backend (конфиг) и вернуться к SRAM без изменения протокола.
