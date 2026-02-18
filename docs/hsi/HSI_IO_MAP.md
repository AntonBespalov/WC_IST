# HSI_IO_MAP — Pin mux snapshot (CubeMX)

Статус: snapshot  
Дата: 2026-02-18  
Источник истины: `uspf_421243_064.ioc` (CubeMX 6.16.0), `Core/Src/stm32g4xx_hal_msp.c` (если есть ручные правки)  
IOC SHA256: `36b05e8f41ed15ad4cc81ed802916b88b7ed9cc768db850f0db60300d2cb3804`  
MCU: STM32G474Q(B-C-E)Tx (STM32G474QETx)  
Проект (CubeMX): `uspf_421243_064`

---

## Назначение
- Зафиксировать текущий pin mux (Pin → Signal → Mode → Label) как проверяемый “снимок”.
- Документ предназначен для ревью/аудита, стендовой отладки и контроля регрессий при регенерации CubeMX.

## Pin mux snapshot

| Pin | Signal | Mode | GPIO_Label | GPIO_Speed | Locked |
|---|---|---|---|---|---|
| PA0 | GPXTI0 |  | COMX_IRQ |  | true |
| PA1 | GPIO_Output |  | COMX_RESET |  | true |
| PA2 | QUADSPI1_BK1_NCS | Single Bank 1 |  |  |  |
| PA3 | QUADSPI1_CLK | Single Bank 1 |  |  |  |
| PA5 | SPI1_SCK | Full_Duplex_Master |  | GPIO_SPEED_FREQ_VERY_HIGH |  |
| PA6 | SPI1_MISO | Full_Duplex_Master |  | GPIO_SPEED_FREQ_VERY_HIGH |  |
| PA7 | QUADSPI1_BK1_IO2 | Single Bank 1 |  |  |  |
| PA8 | GPIO_Input |  | SKYPER_ERROUT2 |  | true |
| PA9 | GPIO_Input |  | SKYPER_ERROUT1 |  | true |
| PA11 | USART1_CTS | CTS_RTS |  |  |  |
| PA12 | USART1_RTS | CTS_RTS |  |  |  |
| PA13 | SYS_JTMS-SWDIO | Trace_Asynchronous_SW |  |  |  |
| PA14 | SYS_JTCK-SWCLK | Trace_Asynchronous_SW |  |  |  |
| PB0 | TIM1_CH2N | PWM Generation2 CH2 CH2N |  |  |  |
| PB3 | SYS_JTDO-SWO | Trace_Asynchronous_SW |  |  |  |
| PB4 | GPIO_Input |  | AD7606_BUSY2 |  | true |
| PB5 | GPIO_Input |  | AD7606_BUSY1 |  | true |
| PB10 | TIM1_BKIN | Activate-Break-Input |  |  |  |
| PB12 | SPI2_NSS | NSS_Signal_Hard_Input |  | GPIO_SPEED_FREQ_VERY_HIGH |  |
| PB13 | SPI2_SCK | Full_Duplex_Slave |  | GPIO_SPEED_FREQ_VERY_HIGH |  |
| PB14 | SPI2_MISO | Full_Duplex_Slave |  | GPIO_SPEED_FREQ_VERY_HIGH |  |
| PB15 | SPI2_MOSI | Full_Duplex_Slave |  | GPIO_SPEED_FREQ_VERY_HIGH |  |
| PC0 | S_TIM1_CH1 |  |  |  |  |
| PC1 | S_TIM1_CH2 |  |  |  |  |
| PC2 | GPIO_Output |  | EXTWDG_OUT |  | true |
| PC3 | TIM1_BKIN2 | Activate-Break-Input-2 |  |  |  |
| PC4 | USART1_TX | Asynchronous |  |  |  |
| PC5 | USART1_RX | Asynchronous |  |  |  |
| PC9 | GPIO_Output |  | SKYPER_ERR_IN |  | true |
| PC10 | SPI3_SCK | Full_Duplex_Master |  |  |  |
| PC11 | SPI3_MISO | Full_Duplex_Master |  |  |  |
| PC12 | SPI3_MOSI | Full_Duplex_Master |  |  |  |
| PC13 | TIM1_CH1N | PWM Generation1 CH1 CH1N |  |  |  |
| PD0 | FMC_D2_DA2 |  |  |  |  |
| PD1 | FMC_D3_DA3 |  |  |  |  |
| PD3 | GPIO_Output |  | AD7606_CONVST |  | true |
| PD4 | FMC_NOE |  |  |  |  |
| PD5 | FMC_NWE |  |  |  |  |
| PD6 | FMC_NWAIT |  |  |  |  |
| PD7 | FMC_NE1 | NorPsramChipSelect1_1 |  |  |  |
| PD8 | FMC_D13_DA13 |  |  |  |  |
| PD9 | FMC_D14_DA14 |  |  |  |  |
| PD10 | FMC_D15_DA15 |  |  |  |  |
| PD14 | FMC_D0_DA0 |  |  |  |  |
| PD15 | FMC_D1_DA1 |  |  |  |  |
| PE0 | FMC_NBL0 |  |  |  |  |
| PE1 | FMC_NBL1 |  |  |  |  |
| PE2 | SPI4_SCK | Full_Duplex_Master |  |  |  |
| PE3 | S_TIM3_CH2 |  | AD7380_CS |  | true |
| PE5 | SPI4_MISO | Full_Duplex_Master |  |  |  |
| PE6 | SPI4_MOSI | Full_Duplex_Master |  |  |  |
| PE7 | FMC_D4_DA4 |  |  |  |  |
| PE8 | FMC_D5_DA5 |  |  |  |  |
| PE9 | FMC_D6_DA6 |  |  |  |  |
| PE10 | FMC_D7_DA7 |  |  |  |  |
| PE11 | FMC_D8_DA8 |  |  |  |  |
| PE12 | FMC_D9_DA9 |  |  |  |  |
| PE13 | FMC_D10_DA10 |  |  |  |  |
| PE14 | FMC_D11_DA11 |  |  |  |  |
| PE15 | FMC_D12_DA12 |  |  |  |  |
| PF0-OSC_IN | RCC_OSC_IN | HSE-External-Oscillator |  |  |  |
| PF1-OSC_OUT | RCC_OSC_OUT | HSE-External-Oscillator |  |  |  |
| PF2 | FMC_A2 |  |  |  |  |
| PF3 | FMC_A3 |  |  |  |  |
| PF4 | FMC_A4 |  |  |  |  |
| PF5 | FMC_A5 |  |  |  |  |
| PF6 | QUADSPI1_BK1_IO3 | Single Bank 1 |  |  |  |
| PF7 | FMC_A1 |  |  |  |  |
| PF8 | QUADSPI1_BK1_IO0 | Single Bank 1 |  |  |  |
| PF9 | QUADSPI1_BK1_IO1 | Single Bank 1 |  |  |  |
| PF10 | FMC_A0 |  |  |  |  |
| PF12 | FMC_A6 |  |  |  |  |
| PF13 | FMC_A7 |  |  |  |  |
| PF14 | FMC_A8 |  |  |  |  |
| PF15 | FMC_A9 |  |  |  |  |
| PG0 | FMC_A10 |  |  |  |  |
| PG1 | FMC_A11 |  |  |  |  |
| PG2 | FMC_A12 |  |  |  |  |
| PG3 | FMC_A13 |  |  |  |  |
| PG4 | SPI1_MOSI | Full_Duplex_Master |  | GPIO_SPEED_FREQ_VERY_HIGH |  |
| PG6 | GPIO_Output |  | USER_LED1 |  | true |
| PG7 | GPIO_Output |  | USER_LED2 |  | true |
| PG8 | GPIO_Output |  |  |  | true |
| PG9 | GPIO_Output |  | USER_TEST2 |  | true |

---

## Примечания
- Пины, отсутствующие в таблице, считаются “не сконфигурированными” (reset state) на уровне CubeMX.
- Колонка `Locked` отражает lock в `.ioc` для предотвращения случайных изменений через GUI CubeMX.

## Manual notes
<!-- MANUAL_NOTES:START -->

<!-- MANUAL_NOTES:END -->
