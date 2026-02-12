# Design Assurance Package (DAP)
## MFDC Source – STM32G474 / AD7380 / Rogowski / SKYPER 42R

### 1. Scope
This document adapts Safety-by-Design principles to the concrete MFDC welding current source architecture:
- MCU: STM32G474
- Current sensing: Rogowski coil + AD7380
- Power stage drivers: SKYPER 42R (x2), SEMiX252GB12 modules
- Control: PWM 1–4 kHz, EtherCAT PDO command/status loop 4 kHz (250 us)

---

## 2. System Architecture Overview
The source acts as a **fast deterministic current actuator**, while the Technological Controller (TC) performs process-level control.

Safety principle: **Energy removal must not rely solely on software or EtherCAT communication.**

---

## 3. Safety Functions Mapping (HW / SW)

### SF-1: Emergency Energy Removal
- Purpose: Prevent hazardous current delivery.
- HW:
  - SKYPER 42R DESAT / OC protection
  - Hardware gate-enable chain (fail-safe = OFF)
  - Timer PWM break input
- SW:
  - Fault latching
  - Controlled ramp-down before disable (if time allows)

### SF-2: MCU Failure Detection
- HW:
  - Independent watchdog
- SW:
  - Window watchdog servicing in control ISR
  - Fault log + safe-state entry

### SF-3: Overcurrent / Abnormal Current Shape
- HW:
  - Driver-level protection
- SW:
  - ADC plausibility check (Rogowski + AD7380)
  - dI/dt and Imax supervision

### SF-4: Loss of Control Command (EtherCAT PDO Timeout)
- HW:
- None (information-level)
- SW:
  - EtherCAT/PDO watchdog (staleness / timeout)
  - Iref ramp to zero
  - Disable if timeout persists

---

## 4. Risk Register (ISO 12100-style, condensed)

### Hazard: Electric shock / excessive welding current
- Hazardous situation: MCU freeze with PWM active
- Risk before mitigation: High
- Mitigations:
  - Hardware PWM break
  - Gate-enable interlock
  - Independent watchdog
- Residual risk: Low

### Hazard: Fire / overheating
- Hazardous situation: Cooling failure with continued current
- Mitigations:
  - Temperature monitoring
  - Thermal shutdown
- Residual risk: Low–Medium

### Hazard: Unintended energization
- Hazardous situation: EtherCAT communication fault
- Mitigations:
  - EtherCAT PDO timeout
  - Default-safe startup (PWM disabled)
- Residual risk: Low

---

## 5. Determinism & Timing Assurance

- Control ISR isolated from EtherCAT and logging
- ADC sampling synchronized to PWM
- Worst-case latency budget documented
- Logging via DMA only, never blocking ISR

---

## 6. Verification & Validation Strategy

- Unit tests for:
  - Current scaling
  - Saturation logic
  - Fault thresholds
- Integration tests:
  - EtherCAT PDO timeout behavior
  - Fault injection (ADC freeze, watchdog reset)
- Traceability:
  - Each safety function ↔ test case

---

## 7. Conclusion

This Design Assurance Package provides:
- Clear separation of safety vs. control logic
- Hardware-first energy removal strategy
- Audit-ready structure aligned with ISO 12100 / ISO 13849 / IEC 62135-1

The architecture is suitable for future formal certification without major redesign.
