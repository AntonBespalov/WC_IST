# tests/unit/

L1 unit tests: проверка core-логики на host без HAL/RTOS.

Локальный запуск (см. также `docs/verification/MFDC_SIL_First_Build_Contract_RU.md` / 4.3):
- `cmake --preset host`
- `cmake --build --preset host`
- `ctest --preset host-l1`

Точечный запуск для отладки:
- multi-config (Visual Studio, PowerShell): `.\build\host_local\tests\unit\RelWithDebInfo\control_core_tests.exe --list`
- multi-config (Visual Studio, PowerShell): `.\build\host_local\tests\unit\RelWithDebInfo\control_core_tests.exe --run <name>`
- single-config (Makefiles/Ninja, bash): `./build/host_local/tests/unit/control_core_tests --list`
- single-config (Makefiles/Ninja, bash): `./build/host_local/tests/unit/control_core_tests --run <name>`
