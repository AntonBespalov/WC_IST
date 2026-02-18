# HSI_TRIGGER_MAP — ADC/SPI/Timer trigger chains

Статус: snapshot  
Дата: 2026-02-18  
Источник: `uspf_421243_064.ioc` (CubeMX 6.16.0) + проектные DN/архитектура  
IOC SHA256: `36b05e8f41ed15ad4cc81ed802916b88b7ed9cc768db850f0db60300d2cb3804`

---

## 1) Fast chain: AD7380 (SPI1+SPI2, timer-driven CS)
**Намерение:**
- TIM1 выдаёт TRGO по update: `TIM_TRGO_UPDATE`.
- TIM3 используется как slave reset (источник ITRx от TIM1 TRGO) и формирует PWM на CH2 как **CS** для AD7380.
- SPI1 (master) + SPI2 (slave) принимают два канала данных через DMA.

**Пины (из `.ioc`):**
- AD7380_CS: `PE3` (label `AD7380_CS`, signal `S_TIM3_CH2`)
- SPI1: `PA5` SCK, `PA6` MISO, `PG4` MOSI
- SPI2: `PB12` NSS, `PB13` SCK, `PB14` MISO, `PB15` MOSI

**TIM3 параметры из `.ioc` (CH2):**
- PeriodNoDither: `424`
- PulseNoDither_2: `320`
- OCPolarity_2: `TIM_OCPOLARITY_LOW`

**Точка внимания:**
- сопоставление ITRx ↔ TIM1 TRGO нужно подтверждать в коде/или в `.ioc` (CubeMX/RM), чтобы цепочка не “ломалась” при рефакторинге.

---

## 2) AD7606 #1 (SPI3)
- Trigger: software GPIO `AD7606_CONVST`.
- Busy: GPIO `AD7606_BUSY1`.
- SPI3: PC10/PC11/PC12 (SCK/MISO/MOSI).

## 3) AD7606 #2 (SPI4)
- Trigger: software GPIO `AD7606_CONVST` (общий).
- Busy: GPIO `AD7606_BUSY2`.
- SPI4: PE2/PE5/PE6 (SCK/MISO/MOSI).

## Manual notes
<!-- MANUAL_NOTES:START -->

<!-- MANUAL_NOTES:END -->
