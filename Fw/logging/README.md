# logging/

Минимальный каркас логирования/осциллографирования (stage-1, SRAM-only).
См. DN-011: `docs/design-notes/DN-011_Service_Logging_Capture_Framework.md`.

**Назначение**
- Сформировать единый формат Record и базовый контур захвата окна pre/post trigger.
- Разделить fast/slow домены: в fast домене только snapshot/publish, упаковка и транспорт — в slow loop.
- Обеспечить приоритет PDO над LOG при общем UART (через TX scheduler).

**Карта файлов**
- `Fw/logging/logging_types.h` — базовые типы/статусы/Record header и флаги.
- `Fw/logging/logging_record.h` / `Fw/logging/logging_record.c` — упаковка/распаковка заголовка Record (LE).
- `Fw/logging/logging_crc32.h` / `Fw/logging/logging_crc32.c` — CRC32 для payload (slow домен).
- `Fw/logging/logging_spsc.h` / `Fw/logging/logging_spsc.c` — SPSC очередь фиксированных элементов (fast→slow).
- `Fw/logging/logging_pipeline.h` — разделённый API fast/slow (publish/pack).
- `Fw/logging/logging_capture_sram.h` / `Fw/logging/logging_capture_sram.c` — SRAM-окно pre/post trigger.
- `Fw/logging/logging_packer.h` / `Fw/logging/logging_packer.c` — упаковка snapshot по профилю/полям (LE).
- `Fw/logging/logging_tx_scheduler.h` / `Fw/logging/logging_tx_scheduler.c` — приоритет PDO + token-budget на LOG.
- `Fw/logging/logging_core.h` / `Fw/logging/logging_core.c` — ядро сессий, запись Record, чтение окна.
- `Fw/logging/logging_platform.h` — platform hooks (critical section).
- `Fw/logging/CMakeLists.txt` — сборка библиотеки `mfdc_logging_core` (host/SIL).
- `tests/unit/logging_framework_tests.c` — минимальные unit‑тесты (Record, SPSC, packer, окно SRAM, scheduler).

**Примечание по формату Record**
Заголовок Record начинается с `magic = 0xA55A` (LE) для ресинхронизации парсера на стороне ПК.

**Документация**
Подробное описание формата, интеграции, практических граблей и примеров: `docs/logging/LOGGING_GUIDE.md`.
