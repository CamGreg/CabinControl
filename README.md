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

## Daily history

The firmware samples the first configured VE.Direct controller and up to three temperature sensors every ten minutes. It enables that controller's serial isolator for the duration of the HEX GET requests. The project currently declares both isolator selects as GPIO 0. Each isolator needs its own GPIO assignment before both controllers can be polled independently.

The firmware writes one CSV per device-local day to /data/YYYY-MM-DD.csv on the SD card. Blank cells represent unavailable readings. The dashboard opens one day at a time and falls back to a monthly CSV for older dates.

After a cold boot with an unset ESP clock, open the dashboard once from a device with the correct local date and time. The dashboard initializes the ESP clock from that device. Samples logged before initialization will be stored under the ESP's default date.
