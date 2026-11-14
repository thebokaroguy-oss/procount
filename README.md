# ProCount v1

ProCount is an ESP32 powder dispenser prototype with a browser companion. The device reads RFID profiles, animates an 8×8 MAX7219 display, shows values on a 4-digit display, scans a keypad, controls a 28BYJ-48 stepper through ULN2003, and exchanges status and commands over MQTT.

## V1 limits

- There is no load cell. The device cannot measure dispensed mass; grams are an open-loop estimate based on motor steps.
- Motor calibration defaults to `0.0 steps/g`, which keeps the motor disabled. Calibrate with an external scale before enabling it.
- The browser can read PDF and DOCX files, including scanned PDFs. OCR and extraction are best-effort; verify every value, unit, and reference range against the original report.
- ProCount does not diagnose deficiencies or calculate a treatment dose. A person must choose and confirm any dispense target.

## Hardware

- ESP32 DevKit-class board
- RC522 RFID reader over SPI
- MAX7219 8×8 matrix
- 4-digit V1.2 HW 069 display (firmware currently assumes TM1637; check the board controller and pinout)
- 28BYJ-48 5 V stepper with ULN2003 driver and a suitable external 5 V supply
- 4×4 keypad and red/yellow/green status-light module

Check the exact ESP32 board pinout and the example map in `firmware/procount/src/config.h` before wiring. Share ground between the ESP32 and motor supply. Do not power the motor from a GPIO. Keep credentials, RFID UIDs, local broker addresses, and other device-specific values in the ignored `firmware/procount/src/secrets.h`.

## MQTT

The device topic root defaults to `procount/device/procount-esp32-01`:

- Commands: `.../command`
- Status: `.../status`

The default web broker URL is `ws://localhost:8000/mqtt`. It works when the browser and Docker broker run on the same computer. A public HTTPS deployment cannot connect to that local URL for remote control. Remote device control needs a TLS-protected WSS broker with authentication and topic permissions. Do not expose the unauthenticated development broker publicly.

## Web app and accounts

The Vite app is in `web/`; Vercel's project root is `web`. It uses Supabase email one-time codes and an admin-managed allowlist. Anyone can request a code, but only approved accounts can open the dashboard.

Profiles are saved per Supabase account. Scanned report files and extracted text/values are stored in a private Supabase bucket and database rows. Row-level policies restrict each account to its own profile and report data; admins can manage the allowlist but cannot read medical records. A required checkbox links to `web/public/privacy.html`, and the accepted policy version is recorded per account. Users can delete their saved data or withdraw consent and delete it.

Apply the SQL files in `supabase/migrations/` in timestamp order. Seed initial admins privately in the Supabase SQL Editor by inserting each admin email into `private.procount_admin_emails` and into `public.procount_allowed_users` with `role='admin'` and `active=true`. Do not put admin addresses in public source.

For local development, copy `web/.env.example` to `web/.env.local`, set `VITE_SUPABASE_URL` and `VITE_SUPABASE_PUBLISHABLE_KEY`, then run `npm install` and `npm run dev` from `web/` using Node.js 22 or newer. Never put a Supabase service-role key in the browser app or public repository.

The current Supabase Free email template sends a single-use sign-in link. To send numeric OTP codes, configure a custom SMTP provider (or a Supabase plan that allows template editing) in Auth → Emails, then set the Magic link or OTP template body to use `{{ .Token }}`. Enter SMTP credentials directly in Supabase; never commit them. The ProCount redirect is already allowlisted. Google OAuth is not enabled in this version.

## Firmware

Copy `firmware/procount/src/secrets.example.h` to `firmware/procount/src/secrets.h` and fill in Wi-Fi, broker, and RFID values locally. `secrets.h` is ignored by Git. Install PlatformIO, open `firmware/platformio.ini`, then build and flash with `pio run -t upload`; use Serial Monitor at 115200 baud.

## Local HiveMQ

Install Docker Desktop, provide the HiveMQ license key through the hidden prompt in `./start-hivemq.sh`, then verify with `docker compose ps`. The script does not save the license key in the project. The local broker permits unauthenticated development clients by default; keep it on a trusted LAN.
