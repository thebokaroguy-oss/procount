# ProCount v1 starter

ProCount v1 firmware and a browser companion app. The ESP32 drives a 28BYJ-48 through a ULN2003 board, reads the keypad and RC522, animates the MAX7219 matrix, updates a compatible TM1637 display, and connects directly to your local HiveMQ broker over MQTT on port 1883. The companion app stores the user profile locally, parses PDF/DOCX report content locally, and sends approved gram targets over HiveMQ WebSockets on port 8000.

## Version 1 limits

- No load cell or HX711 is included. The device cannot measure actual dispensed mass, detect an empty hopper, or close the loop on a gram target. Its amount is a step-count estimate only.
- `STEPS_PER_GRAM` is deliberately `0.0`; set a calibration from external-scale trials before the device will run the motor.
- The lab reader supports text extraction from PDF/DOCX and OCR of image-only PDF pages and embedded DOCX images. Extraction is best-effort. Users must verify all reported values, units, and reference intervals against the original file.
- Lab flags are only comparisons with a reference range extracted from that same document. The app does not diagnose a condition or calculate a therapeutic powder dose. A user/clinician-approved target is required before sending a command.
- Profile and report content stay in the browser. HiveMQ carries only the user profile letter, target grams, request ID, and command metadata. Do not put names, reports, credentials, or other personal details in MQTT payloads.

## Hardware assumptions to confirm

- ESP32 DevKit-class board.
- 28BYJ-48 5 V motor and ULN2003 driver board with a suitable external 5 V supply. Share ground with the ESP32; do not power the motor from a GPIO.
- The 4-digit V1.2 HW 069 board is assumed to use TM1637. Confirm its controller, voltage, and pinout before connecting it.
- RC522 uses SPI. MAX7219 matrix shares the SPI clock/data pins and has its own CS pin.
- The example GPIO map in `firmware/procount/src/config.h` is a starting point, not a wiring guarantee. It includes ESP32 boot-strapping pins for some boards/peripherals. Review the exact ESP32 board and module pinouts, change pins as needed, and check for conflicts before powering anything. Status-light pins must be set for the actual module.
- The keypad matrix layout in the firmware assumes rows `1 2 3 A / 4 5 6 B / 7 8 9 C / * 0 # D`.

## HiveMQ topics

For device ID `procount-esp32-01`:

- App publishes commands to `procount/device/procount-esp32-01/command`.
- ESP32 publishes retained state to `procount/device/procount-esp32-01/status`.
- Command JSON: `{"requestId":"unique-id","user":"A","grams":25,"source":"companion-app"}`.
- Status includes `state`, `message`, `user`, `targetGrams`, and `requestId` where available.
- ESP32 connects to local HiveMQ over MQTT/TCP port 1883. Set `MQTT_HOST` in `secrets.h` to the LAN IP address of the computer running Docker (never `localhost` from the ESP32).
- Browser connects to the local broker over WebSocket at `ws://localhost:8000/mqtt` when running on that same computer. If the browser is on another device, use `ws://<computer-LAN-IP>:8000/mqtt`. HiveMQ routes the WebSocket listener at `/mqtt`.
- Create separate credentials for the ESP32 and browser, with permissions limited to the device command and status topics. A browser MQTT credential is visible to its user, so never use an administrator credential or broad shared secret.

## Configure and flash ESP32

1. Install PlatformIO and open `firmware/platformio.ini` as a PlatformIO project.
2. Copy `firmware/procount/src/secrets.example.h` to `firmware/procount/src/secrets.h`.
3. Enter Wi-Fi credentials and the host computer LAN IP in `secrets.h`. For this local Docker setup, use port 1883 and keep `MQTT_USE_TLS` at `0`; the firmware connects without MQTT credentials unless you configure broker authentication. The license key belongs only in the Docker environment, never in ESP32 firmware. Keep `secrets.h` private; it is ignored by Git.
4. Confirm the GPIO map and display driver assumptions. Change `MQTT_DEVICE_ID` and the app topic root together if you use a different ID.
5. Calibrate the powder mechanism on an external scale. Record repeatable step counts across several portions for each powder, then set `STEPS_PER_GRAM` in `config.h`. This is open-loop calibration and will vary with powder and mechanism conditions.
6. Build and flash with PlatformIO (`pio run -t upload`); open serial monitor at 115200 baud.

## Run the companion app

1. Install Node.js 20 or newer.
2. In `web/`, run `npm install`, then `npm run dev`.
3. Enter `ws://localhost:8000/mqtt` if the app browser is on the Docker host, or use the host computer LAN IP from another device. Leave username/password blank for the default local development broker; configure authentication before allowing broader network access. Set the topic root to match the device ID.
4. Save profile details in the local browser, upload a PDF or DOCX report if needed, verify extracted values, and separately enter a user/clinician-approved target before sending a command.

The app is a local development interface; serving it publicly requires a proper HTTPS deployment and a reviewed credential/access-control design.

## Start the local HiveMQ broker

1. Install and start Docker Desktop. On first launch, review and accept Docker's terms yourself.
2. Rotate any HiveMQ license key that was pasted into chat, then run `./start-hivemq.sh` from this folder. The script prompts for the replacement key with hidden input and does not save it into the project files.
3. Confirm the container is running with `docker compose ps`. The compose file exposes MQTT on 1883, browser WebSockets on 8000, and the HiveMQ Control Center on 8080. Open `http://localhost:8080` to confirm the broker starts.
4. Find the computer's LAN IP and set it as `MQTT_HOST` in the ESP32 `secrets.h`. The ESP32 and computer must be on the same reachable LAN.

The supplied local broker defaults to allowing clients without authentication. Keep it on a trusted network for development; add authentication and topic permissions before exposing the broker beyond your LAN. The browser and ESP32 use the same command/status topics. Copy `firmware/procount/src/secrets.example.h` to the ignored `secrets.h` and add Wi-Fi, broker, and RFID UID values locally. Do not commit `secrets.h`.
# Web sign-in and deployment

The web companion uses Supabase email one-time codes and an allowlist. Admin emails are held in a private Supabase schema and are not committed with the public source. Admins can approve or disable user emails from the signed-in dashboard. Access is enforced by the `procount_allowed_users` table's row-level security policies, not only by the browser UI.

Apply the SQL migration in `supabase/migrations/202611140001_procount_access_control.sql` to the Supabase project before deploying. For local development, copy `web/.env.example` to `web/.env.local` and set `VITE_SUPABASE_URL` plus `VITE_SUPABASE_PUBLISHABLE_KEY`. The publishable key is designed for browser use; never add a Supabase service-role key to the web app or repository. Set the same two `VITE_` values as Vercel environment variables for Preview and Production.

In Supabase Auth, set the site URL and allowed redirect URL to the deployed Vercel origin. Configure the email template to include `{{ .Token }}` if you want a numeric code rather than a magic link. Google OAuth is not enabled in this first release.

The Vercel site can be publicly visited, while the dashboard remains behind OTP and admin approval. The device dashboard's current MQTT default is `ws://localhost:8000/mqtt`, which works only when the browser and broker share a machine. For remote use, configure a TLS protected WSS broker and credentials with topic permissions before exposing control to the internet. Do not expose the current unauthenticated local HiveMQ broker publicly.
