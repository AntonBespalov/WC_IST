# HSI_SAFETY_PINS — safety‑critical IO snapshot

Статус: snapshot  
Дата: 2026-02-18  
Источник: `uspf_421243_064.ioc` (CubeMX 6.16.0) + проектный safety‑контекст (TIM1 BKIN/BKIN2, драйверы)  
IOC SHA256: `36b05e8f41ed15ad4cc81ed802916b88b7ed9cc768db850f0db60300d2cb3804`

---

## Назначение
- Список safety‑critical сигналов, которые нельзя менять “случайно” через CubeMX без отдельного DN/ADR.
- Точки проверки на стенде/в тест-плане.

## 1) Аппаратные входы аварийного отключения (TIM1)
- BKIN: `PB10` — `TIM1_BKIN`
- BKIN2: `PC3` — `TIM1_BKIN2`

Требование: работоспособность линий аварийного отключения должна быть независима от логики fast/slow loop.

## 2) Сигналы драйверов (SKYPER)
- `PA9` — `SKYPER_ERROUT1` (GPIO_Input)
- `PA8` — `SKYPER_ERROUT2` (GPIO_Input)
- `PC9` — `SKYPER_ERR_IN` (GPIO_Output; проверить назначение по схеме)

## 3) Внешний watchdog / supervisor
- `PC2` — `EXTWDG_OUT` (GPIO_Output)

## 4) PWM/actuator outputs (TIM1)
| Pin | Signal | Notes |
|---|---|---|
| PC13 | TIM1_CH1N | TIM1 PWM output (check polarity/complementary & driver mapping) |
| PB0 | TIM1_CH2N | TIM1 PWM output (check polarity/complementary & driver mapping) |

## Manual notes
<!-- MANUAL_NOTES:START -->

<!-- MANUAL_NOTES:END -->
