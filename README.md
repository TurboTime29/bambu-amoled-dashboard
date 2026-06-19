# Bambu Lab AMOLED Dashboard

A round, Apple-esque dashboard for **Bambu Lab 3D printers**, running on the
**Waveshare ESP32-S3-Touch-AMOLED-1.75** (466×466 circular AMOLED) with
[ESPHome](https://esphome.io) + LVGL. It pulls live print data from **Home
Assistant** and is designed to be OLED-safe (no static burn-in elements).

<img width="300" height="300" alt="Main Screen" src="https://github.com/user-attachments/assets/7b6f8e75-d252-4c75-96f3-f48dce0b7111" />
<img width="300" height="300" alt="AMS Screen" src="https://github.com/user-attachments/assets/1b6457d1-b87f-4b10-9f24-9a2fe62cf5af" />
<img width="300" height="300" alt="Restart/Battery/Brightness Screen" src="https://github.com/user-attachments/assets/81ccb0f8-ee88-4ff1-944f-e992d8d5de5d" />





## Features

- **Adaptive home face** — a clean clock/date when idle; flips to live print
  progress (arc, time-remaining, %, layer, nozzle & bed temps) while printing.
- **Tap the time** to flip between *time remaining* and *projected end time*.
- **AMS view** — spool colors/materials for an AMS + AMS HT, with humidity/temp.
- **Settings** — battery (with live charging voltage), brightness, restart.
- **Screensaver** — twinkling stars + occasional shooting stars, pure motion so
  no pixel stays lit (protects the OLED).
- **Swipe navigation** and AXP2101 battery management (fast-charge tuned).

## Navigation (all swipe-based)

| Gesture | Action |
|---|---|
| Swipe **←ﾠ/ ﾠ→** | Page between **Home** and **AMS** |
| Swipe **↑** | Open **Settings** (battery · brightness · restart) |
| Swipe **↓** | Open **Screensaver** |
| Swipe **↓** again on the screensaver | **Deep sleep** (touch the screen to wake) |
| Inside Settings/Screensaver | Swipe the **opposite** direction to go back |

## Hardware

- **Waveshare ESP32-S3-Touch-AMOLED-1.75** (ESP32-S3R8, 16 MB flash, 8 MB PSRAM)
  - Display: CO5300 (QSPI), 466×466 round AMOLED
  - Touch: CST9217 (I²C)
  - PMU: AXP2101 (battery/charging)
- A 2.4 GHz Wi-Fi network
- Optional: a LiPo battery (JST) for cordless use

## Prerequisites

1. **Home Assistant** with the **Bambu Lab integration**
   (<https://github.com/greghesp/ha-bambulab>) set up and reporting your printer.
   This dashboard reads its data from HA — it does **not** talk to the printer
   directly.
2. **ESPHome** — either the Home Assistant add-on, or the CLI:
   ```bash
   pip install esphome
   ```

## Setup

### 1. Get the files

```bash
git clone https://github.com/<your-username>/bambu-amoled-dashboard.git
cd bambu-amoled-dashboard
```

The `local_components/cst9217/` folder ships with this repo — it's a small
patched touchscreen driver and **must stay next to the YAML**.

### 2. Configure your entities (the only required edit)

Open `bambu-dashboard-1.75-amoled.yaml` and edit the `substitutions:` block near
the top:

```yaml
substitutions:
  printer_id: "x1c"        # sensor.<printer_id>_nozzle_temperature, etc.
  printer_name: "Bambu Lab"
  ams_id: "ams_2_pro"      # sensor.<ams_id>_tray_1 … _tray_4
  ams_ht_id: "ams_ht"      # sensor.<ams_ht_id>_tray_1
  timezone: "America/New_York"
  device_name: "bambu-dashboard"
```

**How to find your prefixes:** in Home Assistant go to
**Developer Tools → States** and look at your printer's entities. For example,
if you see `sensor.p1s_nozzle_temperature`, then `printer_id` is `p1s`. Do the
same for your AMS (`sensor.<something>_tray_1`).

> No AMS? Leave `ams_id` / `ams_ht_id` as-is — those tiles simply show `--`.

### 3. Create your secrets file

```bash
cp secrets.yaml.example secrets.yaml
```

Edit `secrets.yaml` and fill in your Wi-Fi:

```yaml
wifi_ssid: "YourNetwork"
wifi_password: "YourPassword"
```

`secrets.yaml` is gitignored, so your credentials never get committed.

### 4. Flash the firmware

**The first flash must be over USB** (the board is blank). After that, every
update is wireless (OTA).

#### Option A — ESPHome CLI

Plug the board into your computer via USB-C, then:

```bash
esphome run bambu-dashboard-1.75-amoled.yaml
```

ESPHome compiles, then asks where to upload — pick the **USB serial port** for
the first flash. For later updates, run the same command and choose the
**OTA / network** option (or `--device <node-name>.local`).

#### Option B — ESPHome Dashboard (Home Assistant add-on)

1. Copy `bambu-dashboard-1.75-amoled.yaml`, the `local_components/` folder, and
   your `secrets.yaml` into the ESPHome add-on's config directory
   (e.g. `/config/esphome/`).
2. In the ESPHome dashboard the device appears — click **⋮ → Install**.
3. Choose **Plug into this computer** (or *Manual download* → flash the `.bin`
   with [ESPHome Web](https://web.esphome.io)) for the first flash, then
   **Wirelessly** for updates.

### 5. Add the device to ESPHome / Home Assistant

Once flashed and on Wi-Fi, the node advertises itself via mDNS:

- **ESPHome dashboard:** it shows up automatically under *Discovered*; click
  **Adopt/Take control**.
- **Home Assistant:** go to **Settings → Devices & Services**. ESPHome devices
  are auto-discovered — click **Configure** on the discovered
  `bambu-dashboard` and confirm. (If not discovered, **Add Integration →
  ESPHome** and enter `bambu-dashboard.local`.) The API encryption is open by
  default in this config; HA only needs the hostname.

That's it — the screen should connect to Wi-Fi, pull data from HA, and come to
life within a minute.

## Charging / battery notes

The firmware configures the AXP2101 PMU on boot for a **2200 mAh** cell:
raises the USB input-current limit to 2 A and the charge current to 1 A. If your
battery is **smaller**, lower the charge current in the `on_boot:` lambda
(register `0x62`) — e.g. `0x0B` = 500 mA, `0x0F` = 800 mA. Charging always uses
proper CC/CV, so the cell never exceeds the configured current.

## Re-enabling the camera (optional / advanced)

A live camera page existed in an earlier version and was removed for stability.
If you want to bring it back, you'll need to re-add an `online_image` feed from
an HA `camera_proxy` URL plus a fetch loop and a `page_camera`. It's not in this
release — open an issue if you'd like guidance.

## Troubleshooting

- **Tiles show `--`** — the entity prefix in `substitutions:` doesn't match your
  HA entity names. Double-check in Developer Tools → States.
- **Won't connect to Wi-Fi** — confirm it's a **2.4 GHz** SSID and the values in
  `secrets.yaml` are correct.
- **Compile error about `cst9217`** — make sure the `local_components/` folder is
  in the same directory as the YAML.
- **Board rolls back after flashing** — don't reset/power-cycle for ~60 s after
  an OTA; ESP-IDF needs the new image to boot cleanly before it marks it valid.

## Credits

- Built with [ESPHome](https://esphome.io) and [LVGL](https://lvgl.io).
- Printer data via the [Home Assistant Bambu Lab integration](https://github.com/greghesp/ha-bambulab).
- Hardware: [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75).

## License

MIT — see [LICENSE](LICENSE).
