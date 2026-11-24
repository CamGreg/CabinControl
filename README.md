# CabinControl

## Set up dependencies

Install PlatformIO Core and Node.js with npm. From the project root in Windows PowerShell, run:

```powershell
.\scripts\install-dependencies.ps1
```

The script uses `pio.exe` on PATH or the default PlatformIO installation under your user profile. It installs the firmware toolchain and libraries from `platformio.ini`, installs frontend packages from `frontend/package-lock.json`, and generates `compile_commands.json` for Zed.

## Build and check

Run these commands from the project root. If `pio` is unavailable on PATH, replace it with `& ~\.platformio\penv\Scripts\pio.exe`.

| Task | Command |
| --- | --- |
| Compile and link the ESP32-C3 firmware | `pio run` |
| Type-check the frontend | `.\frontend\node_modules\.bin\tsc.cmd --noEmit --project frontend/tsconfig.json` |
| Type-check and build the frontend | `npm.cmd --prefix frontend run build` |
| Refresh Zed’s firmware include paths and flags | `pio run -t compiledb` |

The frontend build writes to `frontend_build/`. Copy its contents to the SD card root to serve the dashboard on the device. The firmware build also serves as the firmware compile check. The project currently has no automated test suites.
