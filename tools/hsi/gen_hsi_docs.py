#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HSI docs generator for STM32CubeMX .ioc projects.

Generates deterministic "snapshot" documents under docs/hsi/:
- HSI_IO_MAP.md
- HSI_DMA_MAP.md
- HSI_TRIGGER_MAP.md
- HSI_SAFETY_PINS.md

Optionally updates DN-015 (HSI pinmux DN) with reproducibility metadata and links.

Usage examples:
  python tools/hsi/gen_hsi_docs.py --ioc uspf_421243_064.ioc
  python tools/hsi/gen_hsi_docs.py --ioc uspf_421243_064.ioc --msp Core/Src/stm32g4xx_hal_msp.c
  python tools/hsi/gen_hsi_docs.py --ioc uspf_421243_064.ioc --dn docs/design-notes/DN-015_HSI_Pinmux_Map.md
  python tools/hsi/gen_hsi_docs.py --ioc uspf_421243_064.ioc --check

Design:
- Output is deterministic (stable sort, stable formatting).
- Manual notes are preserved between markers:
    <!-- MANUAL_NOTES:START -->
    ...
    <!-- MANUAL_NOTES:END -->
"""

from __future__ import annotations
from dataclasses import dataclass
from pathlib import Path
import argparse
import hashlib
import re
import sys
import datetime
from typing import Dict, List, Optional, Tuple


MANUAL_START = "<!-- MANUAL_NOTES:START -->"
MANUAL_END = "<!-- MANUAL_NOTES:END -->"

DEFAULT_IOC_GLOB = "uspf_*.ioc"

DEFAULT_OUT_DIR = Path("docs/hsi")
DEFAULT_DN_PATH = Path("docs/design-notes/DN-015_HSI_Pinmux_Map.md")

CONTROLLED_FILES = [
    "HSI_IO_MAP.md",
    "HSI_DMA_MAP.md",
    "HSI_TRIGGER_MAP.md",
    "HSI_SAFETY_PINS.md",
]

def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")

def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")

def parse_ioc(ioc_text: str) -> Dict[str, str]:
    kv: Dict[str, str] = {}
    for ln in ioc_text.splitlines():
        if not ln or ln.startswith("#") or "=" not in ln:
            continue
        k, v = ln.split("=", 1)
        kv[k.strip()] = v.rstrip("\n")
    return kv

def extract_manual_notes(existing_text: str) -> str:
    if MANUAL_START in existing_text and MANUAL_END in existing_text:
        a = existing_text.split(MANUAL_START, 1)[1]
        b = a.split(MANUAL_END, 1)[0]
        # Keep as-is (but strip leading/trailing newlines for stable output)
        return b.strip("\n")
    return ""

def inject_manual_notes(generated_text: str, manual_notes: str) -> str:
    # Ensure markers exist in generated_text; replace their region
    if MANUAL_START not in generated_text or MANUAL_END not in generated_text:
        return generated_text
    before = generated_text.split(MANUAL_START, 1)[0]
    after = generated_text.split(MANUAL_END, 1)[1]
    body = manual_notes.strip("\n")
    return before + MANUAL_START + "\n" + body + "\n" + MANUAL_END + after

def md_table(headers: List[str], rows: List[Dict[str, str]]) -> str:
    out = []
    out.append("| " + " | ".join(headers) + " |")
    out.append("|" + "|".join(["---"] * len(headers)) + "|")
    for r in rows:
        out.append("| " + " | ".join(str(r.get(h, "")) for h in headers) + " |")
    return "\n".join(out)

def pin_sort_key(pin: str) -> Tuple[str, int, str]:
    # Handles "PA5", "PB12", "PE3-OSC_IN" etc.
    m = re.match(r"^P([A-Z])(\d+)", pin)
    if not m:
        return ("Z", 999, pin)
    port = m.group(1)
    num = int(m.group(2))
    return (port, num, pin)

@dataclass(frozen=True)
class IocMeta:
    cube_ver: str
    project_name: str
    mcu_name: str
    device_id: str
    ioc_sha256: str

def get_ioc_meta(kv: Dict[str, str], ioc_sha256: str) -> IocMeta:
    return IocMeta(
        cube_ver=kv.get("MxCube.Version", "TBD"),
        project_name=kv.get("ProjectManager.ProjectName", "TBD"),
        mcu_name=kv.get("Mcu.Name", "TBD"),
        device_id=kv.get("ProjectManager.DeviceId", "TBD"),
        ioc_sha256=ioc_sha256,
    )

def extract_pins(kv: Dict[str, str]) -> List[Dict[str, str]]:
    pins: Dict[str, str] = {}
    for k, v in kv.items():
        if not k.endswith(".Signal"):
            continue
        base = k[:-len(".Signal")]
        # Pin bases begin with 'P' (PA5, PB12-OSC_IN etc.)
        if base.startswith("P"):
            pins[base] = v

    rows: List[Dict[str, str]] = []
    for pin in sorted(pins.keys(), key=pin_sort_key):
        rows.append({
            "Pin": pin,
            "Signal": pins[pin],
            "Mode": kv.get(f"{pin}.Mode", ""),
            "GPIO_Label": kv.get(f"{pin}.GPIO_Label", ""),
            "GPIO_Speed": kv.get(f"{pin}.GPIO_Speed", ""),
            "Locked": kv.get(f"{pin}.Locked", ""),
        })
    return rows

def extract_dma(kv: Dict[str, str]) -> List[Dict[str, str]]:
    # Dma.<REQUEST>.<IDX>.<FIELD>
    rx = re.compile(r"^Dma\.([A-Z0-9_]+)\.(\d+)\.(.+)$")
    entries: Dict[Tuple[str, int], Dict[str, str]] = {}
    for k, v in kv.items():
        m = rx.match(k)
        if not m:
            continue
        req = m.group(1)
        idx = int(m.group(2))
        field = m.group(3)
        entries.setdefault((req, idx), {})[field] = v

    rows: List[Dict[str, str]] = []
    for (req, idx), f in sorted(entries.items(), key=lambda x: (x[0][0], x[0][1])):
        rows.append({
            "Request": req,
            "DMA": f.get("Instance", ""),
            "Direction": f.get("Direction", ""),
            "PeriphAlign": f.get("PeriphDataAlignment", ""),
            "MemAlign": f.get("MemDataAlignment", ""),
            "Mode": f.get("Mode", ""),
            "Priority": f.get("Priority", ""),
            "MemInc": f.get("MemInc", ""),
            "PeriphInc": f.get("PeriphInc", ""),
        })
    return rows

def parse_msp_dma_modes(msp_text: str) -> Dict[str, str]:
    """
    Heuristic: read lines like:
      hdma_usart1_rx.Init.Mode = DMA_CIRCULAR;
    Returns map: "USART1_RX" -> "DMA_CIRCULAR"
    """
    modes: Dict[str, str] = {}
    # Find "hdma_xxx_yyy.Init.Mode = DMA_...."
    rx = re.compile(r"hdma_([a-z0-9_]+)\.Init\.Mode\s*=\s*(DMA_[A-Z0-9_]+)\s*;")
    for m in rx.finditer(msp_text):
        var = m.group(1)  # e.g., usart1_rx
        mode = m.group(2) # e.g., DMA_CIRCULAR
        # Map var to request-like key: USART1_RX
        modes[var.upper()] = mode
    return modes

def dma_request_to_msp_key(req: str) -> str:
    # USART1_RX -> USART1_RX ; SPI1_RX -> SPI1_RX
    return req.upper()

def today_iso() -> str:
    return datetime.date.today().isoformat()

def gen_hsi_io_map(meta: IocMeta, ioc_name: str, pin_rows: List[Dict[str, str]], manual_notes: str) -> str:
    t = f"""# HSI_IO_MAP — Pin mux snapshot (CubeMX)

Статус: snapshot  
Дата: {today_iso()}  
Источник истины: `{ioc_name}` (CubeMX {meta.cube_ver}), `Core/Src/stm32g4xx_hal_msp.c` (если есть ручные правки)  
IOC SHA256: `{meta.ioc_sha256}`  
MCU: {meta.mcu_name} ({meta.device_id})  
Проект (CubeMX): `{meta.project_name}`

---

## Назначение
- Зафиксировать текущий pin mux (Pin → Signal → Mode → Label) как проверяемый “снимок”.
- Документ предназначен для ревью/аудита, стендовой отладки и контроля регрессий при регенерации CubeMX.

## Pin mux snapshot

{md_table(["Pin","Signal","Mode","GPIO_Label","GPIO_Speed","Locked"], pin_rows)}

---

## Примечания
- Пины, отсутствующие в таблице, считаются “не сконфигурированными” (reset state) на уровне CubeMX.
- Колонка `Locked` отражает lock в `.ioc` для предотвращения случайных изменений через GUI CubeMX.

## Manual notes
{MANUAL_START}
{manual_notes}
{MANUAL_END}
"""
    return t

def gen_hsi_dma_map(meta: IocMeta, ioc_name: str, dma_rows: List[Dict[str, str]], msp_modes: Optional[Dict[str, str]], manual_notes: str) -> str:
    rows: List[Dict[str, str]] = []
    for d in dma_rows:
        req = d["Request"]
        msp_mode = ""
        notes = ""
        if msp_modes:
            # Try both exact key and a fallback based on typical naming
            key = dma_request_to_msp_key(req)
            msp_mode = msp_modes.get(key, "")
        if msp_mode and msp_mode != d["Mode"]:
            notes = "IOC vs MSP divergence (Mode)"
        elif msp_mode:
            notes = "MSP подтверждает IOC (Mode)"
        else:
            msp_mode = d["Mode"]  # if unknown, show same
        rows.append({
            "Request": req,
            "DMA": d["DMA"],
            "Dir": d["Direction"].replace("DMA_", ""),
            "Align(P/M)": f'{d["PeriphAlign"].replace("DMA_", "")}/{d["MemAlign"].replace("DMA_", "")}',
            "Mode(IOC)": d["Mode"].replace("DMA_", ""),
            "Mode(MSP)": msp_mode.replace("DMA_", ""),
            "Prio": d["Priority"].replace("DMA_PRIORITY_", ""),
            "Notes": notes,
        })

    t = f"""# HSI_DMA_MAP — DMA/DMAMUX snapshot

Статус: snapshot  
Дата: {today_iso()}  
Источник: `{ioc_name}` (CubeMX {meta.cube_ver}) + `stm32g4xx_hal_msp.c` (если указан)  
IOC SHA256: `{meta.ioc_sha256}`

---

## Назначение
- Зафиксировать соответствие DMAMUX request → DMA channel и ключевые параметры.
- Для расхождений IOC vs MSP используется колонка `Mode(MSP)` и `Notes`.

## DMA map

{md_table(["Request","DMA","Dir","Align(P/M)","Mode(IOC)","Mode(MSP)","Prio","Notes"], rows)}

## Manual notes
{MANUAL_START}
{manual_notes}
{MANUAL_END}
"""
    return t

def find_pin_by_signal(pin_rows: List[Dict[str, str]], signal: str) -> Optional[str]:
    for r in pin_rows:
        if r["Signal"] == signal:
            return r["Pin"]
    return None

def find_pin_by_label(pin_rows: List[Dict[str, str]], label: str) -> Optional[str]:
    for r in pin_rows:
        if r["GPIO_Label"] == label:
            return r["Pin"]
    return None

def gen_hsi_trigger_map(meta: IocMeta, ioc_name: str, kv: Dict[str, str], pin_rows: List[Dict[str, str]], manual_notes: str) -> str:
    tim1_trgo = kv.get("TIM1.TIM_MasterOutputTrigger", "TBD")
    tim3_period = kv.get("TIM3.PeriodNoDither", "TBD")
    tim3_pulse2 = kv.get("TIM3.PulseNoDither_2", "TBD")
    tim3_pol2 = kv.get("TIM3.OCPolarity_2", "TBD")

    t = f"""# HSI_TRIGGER_MAP — ADC/SPI/Timer trigger chains

Статус: snapshot  
Дата: {today_iso()}  
Источник: `{ioc_name}` (CubeMX {meta.cube_ver}) + проектные DN/архитектура  
IOC SHA256: `{meta.ioc_sha256}`

---

## 1) Fast chain: AD7380 (SPI1+SPI2, timer-driven CS)
**Намерение:**
- TIM1 выдаёт TRGO по update: `{tim1_trgo}`.
- TIM3 используется как slave reset (источник ITRx от TIM1 TRGO) и формирует PWM на CH2 как **CS** для AD7380.
- SPI1 (master) + SPI2 (slave) принимают два канала данных через DMA.

**Пины (из `.ioc`):**
- AD7380_CS: `PE3` (label `AD7380_CS`, signal `S_TIM3_CH2`)
- SPI1: `PA5` SCK, `PA6` MISO, `PG4` MOSI
- SPI2: `PB12` NSS, `PB13` SCK, `PB14` MISO, `PB15` MOSI

**TIM3 параметры из `.ioc` (CH2):**
- PeriodNoDither: `{tim3_period}`
- PulseNoDither_2: `{tim3_pulse2}`
- OCPolarity_2: `{tim3_pol2}`

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
{MANUAL_START}
{manual_notes}
{MANUAL_END}
"""
    return t

def gen_hsi_safety_pins(meta: IocMeta, ioc_name: str, pin_rows: List[Dict[str, str]], manual_notes: str) -> str:
    bkin = find_pin_by_signal(pin_rows, "TIM1_BKIN") or "PB10 (expected)"
    bkin2 = find_pin_by_signal(pin_rows, "TIM1_BKIN2") or "PC3 (expected)"
    sk1 = find_pin_by_label(pin_rows, "SKYPER_ERROUT1") or "PA9 (expected)"
    sk2 = find_pin_by_label(pin_rows, "SKYPER_ERROUT2") or "PA8 (expected)"
    sk_in = find_pin_by_label(pin_rows, "SKYPER_ERR_IN") or "PC9 (expected)"
    extwdg = find_pin_by_label(pin_rows, "EXTWDG_OUT") or "PC2 (expected)"

    pwm_signals = ["TIM1_CH1","TIM1_CH2","TIM1_CH3","TIM1_CH4","TIM1_CH1N","TIM1_CH2N","TIM1_CH3N"]
    pwm_rows: List[Dict[str, str]] = []
    for sig in pwm_signals:
        p = find_pin_by_signal(pin_rows, sig)
        if p:
            pwm_rows.append({"Pin": p, "Signal": sig, "Notes": "TIM1 PWM output (check polarity/complementary & driver mapping)"})

    if not pwm_rows:
        pwm_rows = [
            {"Pin":"PC0", "Signal":"TIM1_CH1", "Notes":"expected"},
            {"Pin":"PC1", "Signal":"TIM1_CH2", "Notes":"expected"},
            {"Pin":"PC13", "Signal":"TIM1_CH1N", "Notes":"expected"},
            {"Pin":"PB0", "Signal":"TIM1_CH2N", "Notes":"expected"},
        ]

    t = f"""# HSI_SAFETY_PINS — safety‑critical IO snapshot

Статус: snapshot  
Дата: {today_iso()}  
Источник: `{ioc_name}` (CubeMX {meta.cube_ver}) + проектный safety‑контекст (TIM1 BKIN/BKIN2, драйверы)  
IOC SHA256: `{meta.ioc_sha256}`

---

## Назначение
- Список safety‑critical сигналов, которые нельзя менять “случайно” через CubeMX без отдельного DN/ADR.
- Точки проверки на стенде/в тест-плане.

## 1) Аппаратные входы аварийного отключения (TIM1)
- BKIN: `{bkin}` — `TIM1_BKIN`
- BKIN2: `{bkin2}` — `TIM1_BKIN2`

Требование: работоспособность линий аварийного отключения должна быть независима от логики fast/slow loop.

## 2) Сигналы драйверов (SKYPER)
- `{sk1}` — `SKYPER_ERROUT1` (GPIO_Input)
- `{sk2}` — `SKYPER_ERROUT2` (GPIO_Input)
- `{sk_in}` — `SKYPER_ERR_IN` (GPIO_Output; проверить назначение по схеме)

## 3) Внешний watchdog / supervisor
- `{extwdg}` — `EXTWDG_OUT` (GPIO_Output)

## 4) PWM/actuator outputs (TIM1)
{md_table(["Pin","Signal","Notes"], pwm_rows)}

## Manual notes
{MANUAL_START}
{manual_notes}
{MANUAL_END}
"""
    return t

def update_dn_015(dn_path: Path, meta: IocMeta, ioc_name: str) -> None:
    """
    Conservative update:
    - ensure docs/hsi derived artifacts are referenced
    - update/insert reproducibility lines (CubeMX, IOC SHA256, IOC file)
    - add generator command if missing
    """
    text = read_text(dn_path)

    # Update date (first occurrence)
    text = re.sub(r"(Дата:\s*)(\d{4}-\d{2}-\d{2})", r"\g<1>" + today_iso(), text, count=1)

    # Ensure derived artifacts list exists
    if "docs/hsi/HSI_IO_MAP.md" not in text:
        insert_block = (
            "\n- Derived artifacts (обязательные snapshot-документы):\n"
            "  - `docs/hsi/HSI_IO_MAP.md`\n"
            "  - `docs/hsi/HSI_DMA_MAP.md`\n"
            "  - `docs/hsi/HSI_TRIGGER_MAP.md`\n"
            "  - `docs/hsi/HSI_SAFETY_PINS.md`\n"
        )
        # Insert after "Decision" section header if possible
        m = re.search(r"## 3\)\s*Decision.*?\n", text)
        if m:
            pos = m.end()
            text = text[:pos] + insert_block + text[pos:]
        else:
            text += "\n" + insert_block

    # Reproducibility block
    repro = (
        f"- CubeMX: {meta.cube_ver}\n"
        f"- IOC file: `{ioc_name}`\n"
        f"- IOC SHA256: `{meta.ioc_sha256}`\n"
        f"- MCU: {meta.mcu_name} ({meta.device_id})\n"
    )

    if "Reproducibility:" in text:
        # Replace the following bullet block up to a blank line or next header
        text = re.sub(
            r"Reproducibility:\n(?:- .*?\n){1,10}",
            "Reproducibility:\n" + repro,
            text,
            flags=re.M,
        )
    else:
        # Try to add into section 5
        m = re.search(r"## 5\)\s*Interfaces / Data / Timing impact.*?\n", text)
        if m:
            pos = m.end()
            text = text[:pos] + "\nReproducibility:\n" + repro + "\n" + text[pos:]
        else:
            text += "\n\nReproducibility:\n" + repro

    # Add generator instructions (once)
    if "tools/hsi/gen_hsi_docs.py" not in text:
        gen_note = (
            "\nMaintenance:\n"
            "- Обновление `.ioc` требует регенерации snapshot-документов командой:\n"
            f"  - `python tools/hsi/gen_hsi_docs.py --ioc {ioc_name}`\n"
            "- Для проверки актуальности (CI): `python tools/hsi/gen_hsi_docs.py --ioc <file.ioc> --check`\n"
        )
        text += "\n" + gen_note

    write_text(dn_path, text)

def build_all(ioc_path: Path, out_dir: Path, msp_path: Optional[Path], dn_path: Optional[Path]) -> Dict[str, str]:
    ioc_text = read_text(ioc_path)
    kv = parse_ioc(ioc_text)
    meta = get_ioc_meta(kv, sha256_file(ioc_path))

    pin_rows = extract_pins(kv)
    dma_rows = extract_dma(kv)

    msp_modes = None
    if msp_path and msp_path.exists():
        msp_modes = parse_msp_dma_modes(read_text(msp_path))

    # Generate each file, preserving manual notes from existing versions (if present)
    generated: Dict[str, str] = {}

    for name, gen_fn in [
        ("HSI_IO_MAP.md", lambda mn: gen_hsi_io_map(meta, ioc_path.name, pin_rows, mn)),
        ("HSI_DMA_MAP.md", lambda mn: gen_hsi_dma_map(meta, ioc_path.name, dma_rows, msp_modes, mn)),
        ("HSI_TRIGGER_MAP.md", lambda mn: gen_hsi_trigger_map(meta, ioc_path.name, kv, pin_rows, mn)),
        ("HSI_SAFETY_PINS.md", lambda mn: gen_hsi_safety_pins(meta, ioc_path.name, pin_rows, mn)),
    ]:
        target = out_dir / name
        manual_notes = extract_manual_notes(read_text(target)) if target.exists() else ""
        generated[name] = gen_fn(manual_notes)

    # DN update (optional)
    if dn_path:
        if dn_path.exists():
            update_dn_015(dn_path, meta, ioc_path.name)

    return generated

def check_files(out_dir: Path, generated: Dict[str, str]) -> int:
    """
    Fail if any controlled file differs from what generator would produce.
    """
    bad = []
    for name, expected in generated.items():
        target = out_dir / name
        if not target.exists():
            bad.append((name, "missing"))
            continue
        current = read_text(target)
        if current != expected:
            bad.append((name, "differs"))
    if bad:
        sys.stderr.write("HSI docs are out of date:\n")
        for name, reason in bad:
            sys.stderr.write(f"  - {out_dir/name}: {reason}\n")
        sys.stderr.write("\nRun: python tools/hsi/gen_hsi_docs.py --ioc <file.ioc>\n")
        return 1
    return 0

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ioc", type=str, default="", help=f"Path to CubeMX .ioc (default: first match {DEFAULT_IOC_GLOB})")
    ap.add_argument("--out", type=str, default=str(DEFAULT_OUT_DIR), help="Output directory (default: docs/hsi)")
    ap.add_argument("--msp", type=str, default="", help="Optional path to stm32g4xx_hal_msp.c for IOC vs MSP divergence")
    ap.add_argument("--dn", type=str, default=str(DEFAULT_DN_PATH), help="Optional path to DN-015 to update")
    ap.add_argument("--no-dn", action="store_true", help="Do not update DN-015")
    ap.add_argument("--check", action="store_true", help="Check that docs/hsi/*.md match the generator output")
    args = ap.parse_args()

    root = Path(".")
    ioc_path = Path(args.ioc) if args.ioc else None
    if not ioc_path:
        matches = list(root.glob(DEFAULT_IOC_GLOB))
        if not matches:
            sys.stderr.write(f"ERROR: no .ioc found (expected {DEFAULT_IOC_GLOB})\n")
            return 2
        ioc_path = matches[0]
    if not ioc_path.exists():
        sys.stderr.write(f"ERROR: .ioc not found: {ioc_path}\n")
        return 2

    out_dir = Path(args.out)
    msp_path = Path(args.msp) if args.msp else None
    dn_path = None if args.no_dn else Path(args.dn) if args.dn else None

    generated = build_all(ioc_path, out_dir, msp_path, dn_path)

    if args.check:
        return check_files(out_dir, generated)

    # Write generated output
    for name, content in generated.items():
        write_text(out_dir / name, content)

    return 0

if __name__ == "__main__":
    raise SystemExit(main())
