# docs/hsi — HSI snapshots

Эта папка содержит **snapshot-документы** (derived artifacts), регенерируемые из `.ioc` (и опционально MSP).

## Файлы
- `HSI_IO_MAP.md` — pin mux snapshot
- `HSI_DMA_MAP.md` — DMA/DMAMUX snapshot (включая IOC vs MSP divergence по Mode)
- `HSI_TRIGGER_MAP.md` — trigger chains (таймеры/ADC/SPI)
- `HSI_SAFETY_PINS.md` — safety-critical IO

## Как поддерживать актуальность
После изменения `.ioc` (или `stm32g4xx_hal_msp.c`) выполните:
```bash
python tools/hsi/gen_hsi_docs.py --ioc <file.ioc>
```

Проверка (для CI):
```bash
python tools/hsi/gen_hsi_docs.py --ioc <file.ioc> --check
```

## Ручные примечания
Секция между маркерами `MANUAL_NOTES` не перезаписывается генератором.
