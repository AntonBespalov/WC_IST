# tools/hsi — HSI документация (генератор)

## Что это
`gen_hsi_docs.py` — детерминированный генератор snapshot-документов HSI из STM32CubeMX `.ioc`.

Генерирует:
- `docs/hsi/HSI_IO_MAP.md`
- `docs/hsi/HSI_DMA_MAP.md`
- `docs/hsi/HSI_TRIGGER_MAP.md`
- `docs/hsi/HSI_SAFETY_PINS.md`

Опционально обновляет:
- `docs/design-notes/DN-015_HSI_Pinmux_Map.md` (дата + IOC SHA256 + ссылки + команды)

## Использование
```bash
python tools/hsi/gen_hsi_docs.py --ioc uspf_421243_064.ioc
python tools/hsi/gen_hsi_docs.py --ioc uspf_421243_064.ioc --msp Core/Src/stm32g4xx_hal_msp.c
python tools/hsi/gen_hsi_docs.py --ioc uspf_421243_064.ioc --check
```

## Manual notes
В каждом `docs/hsi/*.md` секция между маркерами сохраняется при регенерации:

- `<!-- MANUAL_NOTES:START -->`
- `<!-- MANUAL_NOTES:END -->`

Туда можно вписывать “IOC vs MSP divergence” пояснения, заметки по стенду и т.п.
