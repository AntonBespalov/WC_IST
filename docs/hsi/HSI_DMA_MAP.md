# HSI_DMA_MAP — DMA/DMAMUX snapshot

Статус: snapshot  
Дата: 2026-02-18  
Источник: `uspf_421243_064.ioc` (CubeMX 6.16.0) + `stm32g4xx_hal_msp.c` (если указан)  
IOC SHA256: `36b05e8f41ed15ad4cc81ed802916b88b7ed9cc768db850f0db60300d2cb3804`

---

## Назначение
- Зафиксировать соответствие DMAMUX request → DMA channel и ключевые параметры.
- Для расхождений IOC vs MSP используется колонка `Mode(MSP)` и `Notes`.

## DMA map

| Request | DMA | Dir | Align(P/M) | Mode(IOC) | Mode(MSP) | Prio | Notes |
|---|---|---|---|---|---|---|---|
| SPI1_RX | DMA1_Channel3 | PERIPH_TO_MEMORY | PDATAALIGN_HALFWORD/MDATAALIGN_HALFWORD | NORMAL | NORMAL | HIGH |  |
| SPI2_RX | DMA1_Channel2 | PERIPH_TO_MEMORY | PDATAALIGN_HALFWORD/MDATAALIGN_HALFWORD | NORMAL | NORMAL | HIGH |  |
| TIM3_UP | DMA1_Channel1 | MEMORY_TO_PERIPH | PDATAALIGN_HALFWORD/MDATAALIGN_HALFWORD | NORMAL | NORMAL | LOW |  |
| USART1_RX | DMA1_Channel4 | PERIPH_TO_MEMORY | PDATAALIGN_BYTE/MDATAALIGN_BYTE | NORMAL | NORMAL | LOW |  |
| USART1_TX | DMA1_Channel5 | MEMORY_TO_PERIPH | PDATAALIGN_BYTE/MDATAALIGN_BYTE | NORMAL | NORMAL | LOW |  |

## Manual notes
<!-- MANUAL_NOTES:START -->

<!-- MANUAL_NOTES:END -->
