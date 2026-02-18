# DN-015_HSI_Pinmux_Map — HSI: Pin Muxing + DMA/ADC Trigger Map

Статус: draft  
Дата: 2026-02-18  
Владелец: TBD  
Связано: `docs/CONTEXT_SNAPSHOT.md`, `docs/PROJECT_CONTEXT.md` (разделы 1–5, 7), `uspf_421243_064.ioc`, `Core/Src/stm32g4xx_hal_msp.c`, `Core/Src/main.c`

---

## 1) Context / Problem
- В HSI остаются `TBD` для таблицы Pin Muxing и карты DMA/триггеров АЦП.
- Нужно зафиксировать “как есть” по конфигурации CubeMX и сгенерированному HAL (init/конфигурация железа).
- Контекст исполнения: init/конфигурация железа (см. `PROJECT_CONTEXT.md` / раздел 1, 2, 3).
- Временной домен: N/A (документация/конфиг).

## 2) Goal / Non-goals
### Goal
- Заполнить таблицу Pin Muxing по `uspf_421243_064.ioc`.
- Зафиксировать карту DMA каналов + источников (DMAMUX) и триггеры измерений (AD7380/AD7606).
- Отметить конфликты (если есть) и расхождения между `.ioc` и `stm32g4xx_hal_msp.c`.
- Сослаться на актуальные разделы `PROJECT_CONTEXT.md`.

### Non-goals
- Не менять код или `.ioc`.
- Не менять схему и аппаратные интерфейсы.
- Не подтверждать тайминги/джиттер — только фиксация конфигурации.

## 3) Decision (что делаем)
- Базовый источник Pin Muxing — `uspf_421243_064.ioc`.
- DMA карта берётся из `.ioc`, с уточнениями по фактическим настройкам в `stm32g4xx_hal_msp.c`.
- Триггеры АЦП фиксируются по `main.c` (TIM1/TIM3) и сигналам GPIO/ SPI из `.ioc`.
- Делается явная проверка на конфликты DMA/DMAMUX.

- Derived artifacts (обязательные snapshot-документы):
  - `docs/hsi/HSI_IO_MAP.md`
  - `docs/hsi/HSI_DMA_MAP.md`
  - `docs/hsi/HSI_TRIGGER_MAP.md`
  - `docs/hsi/HSI_SAFETY_PINS.md`
  - Snapshot должен содержать: CubeMX version, IOC SHA256, имя `.ioc`.

## 4) Rationale (почему так)
- `.ioc` и HAL MSP — единственные воспроизводимые источники реальной конфигурации, которые CubeMX может регенерировать без потери.
- Явная фиксация нужна для трассируемости HSI и проверки инвариантов измерений/безопасности (см. `PROJECT_CONTEXT.md` / разделы 2–4, 7).
- Отдельно отмечаются ручные отличия от `.ioc` (например, режим DMA для USART1_RX).

## 5) Interfaces / Data / Timing impact

HSI snapshot вынесен в отдельные документы (derived artifacts):

- Pin mux snapshot: `docs/hsi/HSI_IO_MAP.md`
- DMA/DMAMUX snapshot: `docs/hsi/HSI_DMA_MAP.md`
- Trigger chains (ADC/SPI/Timers): `docs/hsi/HSI_TRIGGER_MAP.md`
- Safety‑critical IO snapshot: `docs/hsi/HSI_SAFETY_PINS.md`

Reproducibility:
- CubeMX: 6.16.0
- IOC file: `uspf_421243_064.ioc`
- IOC SHA256: `36b05e8f41ed15ad4cc81ed802916b88b7ed9cc768db850f0db60300d2cb3804`
- MCU: STM32G474Q(B-C-E)Tx (STM32G474QETx)

Примечание:
- Если в `stm32g4xx_hal_msp.c` есть ручные отличия от `.ioc` (пример: режим DMA для USART1_RX), они должны быть явно описаны в `docs/hsi/HSI_DMA_MAP.md` как “IOC vs MSP divergence”.
## 6) Risks / Edge cases
- В `.ioc` для `USART1_RX` указан `DMA_NORMAL`, но в `stm32g4xx_hal_msp.c` вручную выставлен `DMA_CIRCULAR`. Нужно поддерживать это расхождение осознанно.
- Значение `PA0.Signal=GPXTI0` в `.ioc` выглядит нетипично; при сомнениях сверить с CubeMX/схемой.
- `S_TIM1_CH1/CH2` и `S_TIM3_CH2` требуют проверки интерпретации (CubeMX/RM), если потребуется автоматика верификации.

## 7) Test plan / Proof / Rollback
### Test plan / Proof
- Unit/host: N/A (документация).
- SIL: N/A.
- On-target измерения (GPIO/осциллограф/trace): N/A.
- HIL/bench: N/A.
- Proof of done:
  - Таблица Pin Muxing заполнена по `uspf_421243_064.ioc`.
  - Карта DMA/триггеров сверена с `stm32g4xx_hal_msp.c` и `main.c`.
  - Явные ссылки на `PROJECT_CONTEXT.md` / разделы 1–5, 7.

### Rollback
- Откатить DN до предыдущей версии (код/конфиг не трогались).

## 8) Status / Implementation links
- Status: draft
- Implementation links: TBD
- Derived artifacts: `docs/hsi/*` (см. раздел 5)


Maintenance:
- Обновление `.ioc` требует регенерации snapshot-документов командой:
  - `python tools/hsi/gen_hsi_docs.py --ioc uspf_421243_064.ioc`
- Для проверки актуальности (CI): `python tools/hsi/gen_hsi_docs.py --ioc <file.ioc> --check`
