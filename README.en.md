[中文](README.md) | **English**

<div align="center">

# PokeTrail · Pokémon Journey

Walk with your partner, encounter Pokémon, and catch them through rhythm battles and ball throws.<br>
An offline walking adventure for the round screen of the M5Stack StopWatch (C152).

<img src="docs/images/readme/home.png" width="360" alt="Round home screen: Bulbasaur stands on a Kanto farm path, marked as the carried partner, with 0 species caught beside the ball icon and 4 steps today shown below. Host Runtime/M5GFX render, not a device photo.">

[Getting started](#getting-started) · [Desktop simulation](#desktop-simulation) · [Build and flash](#build-and-flash) · [Development guide (Chinese only)](docs/development.md) · [Verification notes (Chinese only)](docs/verification.md)

<sub><em>Unofficial fan project in development · Images are host Runtime/M5GFX renders, not device photos</em></sub>

</div>

## Gameplay

- **Walking adventures**: Switch freely between five regions—Kanto, Johto, Hoenn, Sinnoh, and Unova. Each region only has Pokémon from its own generation. An encounter occurs every 200–500 steps (random).
- **Rhythm battles and catching**: When the cue circles the opponent, press the right button to attack; when it circles your partner, press the left button to dodge. Time your press by the closing ring. A neutral dot on the bottom track previews the next beat without indicating a button. After winning, throw up to two balls; Poké Balls are unlimited. Forms you have caught, received as a gift, or obtained through evolution no longer appear in new encounters, together with all of their ancestors.
- **Partner training**: Carry one partner at a time. Only the carried partner earns training steps. Open "Carried partner" from the menu to view training and evolution requirements, switch partners, or confirm an evolution.
- **Pokédex**: Freely browse National Pokédex entries 1–649 across Kanto, Johto, Hoenn, Sinnoh, and Unova, with animated pixel art from Pokémon Black and White.

<p align="center">
  <a href="docs/images/readme/battle-neutral-preview.png"><img src="docs/images/readme/battle-neutral-preview.png" width="234" alt="Round battle screen: Charmander faces Pikachu, with health segments for both. A red cue circles Pikachu on the right and closes inward; a neutral grey dot on the bottom track previews the next beat. Host Runtime/M5GFX render, not a device photo."></a>
  <a href="docs/images/readme/throw-single-ring.png"><img src="docs/images/readme/throw-single-ring.png" width="234" alt="Round ball-throwing screen: Bulbasaur in Kanto after an 800ms hold. The upper timing ring closes toward the target, while the ball below has no charge ring. Two balls under the title indicate two attempts left. Host Runtime/M5GFX render, not a device photo."></a>
  <a href="docs/images/readme/viewer.png"><img src="docs/images/readme/viewer.png" width="234" alt="Round Pokédex screen: Bulbasaur, entry 001/649, with Grass and Poison types, ownership status, and an evolution preview. Host Runtime/M5GFX render, not a device photo."></a>
  <br>
  <sub>Rhythm battles · Charged throws · Pokédex · Host Runtime/M5GFX renders, not device photos</sub>
</p>

<p align="center">
  <a href="docs/images/readme/region-kanto.png"><img src="docs/images/readme/region-kanto.png" width="180" alt="Kanto on the round home screen: a red-roofed town, wooden fences, and a farm path. Host Runtime/M5GFX render, not a device photo."></a>
  <a href="docs/images/readme/region-johto.png"><img src="docs/images/readme/region-johto.png" width="180" alt="Johto on the round home screen: a five-story pagoda at dusk, a torii gate, autumn maples, and stone lanterns. Host Runtime/M5GFX render, not a device photo."></a>
  <a href="docs/images/readme/region-hoenn.png"><img src="docs/images/readme/region-hoenn.png" width="180" alt="Hoenn on the round home screen: a smoking volcano, palm trees, the sea, and a beach. Host Runtime/M5GFX render, not a device photo."></a>
  <a href="docs/images/readme/region-sinnoh.png"><img src="docs/images/readme/region-sinnoh.png" width="180" alt="Sinnoh on the round home screen: snowy mountains, conifers, a snowfield, and a cabin. Host Runtime/M5GFX render, not a device photo."></a>
  <a href="docs/images/readme/region-unova.png"><img src="docs/images/readme/region-unova.png" width="180" alt="Unova on the round home screen: a night skyline, a cable-stayed bridge, and a street. Host Runtime/M5GFX render, not a device photo."></a>
  <br>
  <sub>Kanto · Johto · Hoenn · Sinnoh · Unova · Host Runtime/M5GFX renders, not device photos</sub>
</p>

See the [usage guide](docs/usage.en.md) for probabilities, evolution costs, controls, and save rules. See the [gameplay contract (Chinese only)](docs/design/walking-progression.md) for accounts and migration.

<details>
<summary>Image sources</summary>

All images are screenshots rendered offscreen with the host Runtime/M5GFX from the 649-species version. The battle and throw images were updated on 2026-11-03; the other images are from 2026-11-02. They are not C152 device photos and cannot establish on-device frame rate, visual quality, or acceptance results. Click an image to open the original 468×468 file. The physical round screen is 466×466; the current M5GFX driver reports 468×468. The PNGs retain only pixels inside the round screen, with transparency outside it.

The table records the original screenshot sources. Some of these tests and screenshot exports were removed during the test audit; they are not current reproduction commands.

| Screen | Original render suite | Screenshot |
| --- | --- | --- |
| Initial home screen | `test_home_render` | `home-today-complete-live`: the 4 steps today come from simulated walking in the test; nothing caught yet |
| Five regions | `test_home_render` | `home-scene-0`–`home-scene-4`: today's count shows -- because the clock is not set; the test save has caught 2 species |
| Rhythm battle | `test_battle_render` | `battle-idle`: Kanto, Charmander against Pikachu, attack cue |
| Charged throw | `test_capture_render` | `throw-aim-key-800`: Kanto, facing Bulbasaur after an 800ms hold, upper timing ring closing, two attempts left |
| Pokédex | `test_adventure_render` | `viewer-entry`: the starter partner in the test save |

The battery display shows -- because no battery reading was available during rendering. For current behavior and screenshots, run the relevant cases below in a new verification directory. They produce their own scenario evidence and do not recreate or replace the historical images on this page:

```bash
rtk proxy mise run test-simulator-e2e -- \
  --case capture --case battle-cues --case regions
```

</details>

## Getting started

On boot, a Bulbasaur animation and the PokeTrail title appear for at least 1.2 seconds, followed by setup or the page restored from your save. Storage failures lead to the error screen.

Choose **语言 / Language → English** from the main menu. The choice is saved and restored after restart, including Pokémon names, types, and instructions. On first launch, the same entry is on the second page of the date setup screen. New and upgraded saves default to Chinese. See [language settings](docs/usage.en.md#language-settings) for save errors and temporary use.

After flashing with the project's script, the device synchronizes its clock to the flashing computer's current time. Once storage is available, choose Bulbasaur, Charmander, or Squirtle as your starter partner. You can also set the clock manually on the device or leave it unset for now. Total steps and exploration continue to increase; steps recorded while the date is unknown are not added to today's count later.

On the home screen, press either side button or tap the screen to open the menu. The screen turns off after 30 seconds of inactivity; step counting and saving continue. Only a short press of the power key wakes the screen; touch and side buttons do not. See the [usage guide](docs/usage.en.md) for page controls.

## Desktop simulation

After installing the development dependencies below and SDL2, open a desktop window without connecting a device:

```bash
rtk proxy mise run simulate
```

The left/right arrow keys act as the side buttons, `Enter` presses both for confirmation, and mouse clicks and drags simulate touch. `W` toggles walking, `Tab` switches to or from 16× time, and `P` wakes the display. Desktop saves are separate, in `.simulator/save/`. See the [simulator guide (Chinese only)](docs/simulator.md) for dependencies, controls, and limitations.

## Build and flash

Use [mise](https://mise.jdx.dev/) for the development environment and run commands through [RTK](https://github.com/rtk-ai/rtk). Host regression tests also require a system C/C++ compiler; render regression tests require SDL2 and `sdl2-config`. The current verification environment is macOS. Linux and Windows have not been tested.

```bash
rtk proxy mise trust
rtk proxy mise install
rtk proxy mise exec -- python3 -m pip install -r requirements-dev.txt
rtk proxy mise run build
```

The repository includes all generated firmware data, so normal builds do not require downloading the Pokémon assets again. The first PlatformIO build requires network access to fetch the pinned toolchain and libraries. See the [development guide (Chinese only)](docs/development.md) for asset updates, test commands, and code ownership.

Before flashing, confirm the target device. Back up the entire Flash before replacing the factory firmware for the first time:

```bash
rtk proxy mise exec -- python3 scripts/detect_port.py
rtk proxy mise exec -- ./scripts/backup.sh /dev/cu.usbmodemXXXX
rtk proxy mise exec -- ./scripts/flash.sh /dev/cu.usbmodemXXXX
rtk proxy mise exec -- pio device monitor -p /dev/cu.usbmodemXXXX --baud 115200
```

The port above is a macOS example; replace it with the actual target. Automatic detection supports only macOS USB serial ports. Specify the port explicitly when multiple devices are connected. The backup script reads the device's **current** contents. A backup made after flashing custom firmware will not contain the factory image. Keep existing factory backups for the long term.

The flashing script builds and writes the bootloader, partition table, and application. It does not write a filesystem image. After the application starts, the script sends the computer's current time over USB serial. The device writes it to the RTC and save through the existing clock-setting transaction, then reads it back for confirmation. A device with a previously set clock keeps its time zone; the first clock setting uses the computer's current time zone offset. The computer's clock must be correct. Normal restarts use the RTC without setting the date again. Flashing with PlatformIO or esptool alone does not synchronize the clock.

If clock synchronization fails, the script reports that the firmware has already been written and exits with a nonzero status. Test mode, a busy device, or storage faults prevent automatic clock synchronization. The firmware requires a mountable LittleFS filesystem already present in the `spiffs` partition. **There is currently no way to format it for first use.** If a new device shows "Storage not initialized", the Pokédex remains available, but walking gameplay is unavailable. Preserve the original Flash contents. Initialize storage separately only after confirming the backup and the partition's purpose. Do not use automatic formatting to bypass mount errors.

## Hardware

The target device is the M5Stack StopWatch C152 with an ESP32-S3R8, 16 MB Flash, and 8 MB PSRAM. It uses a CO5300 AMOLED display and CST820B touch controller. The physical round screen is 466×466; the current M5GFX driver reports 468×468. The application leaves a margin around the circular edge.

- Facing the screen, `BtnA` (GPIO2) is on the left and `BtnB` (GPIO1) is on the right.
- The M5PM1 hardware powers the device off when you double-press the power button. On each boot, the firmware configures its LDO to maintain the 3.3V supply. The RTC keeps time while the device is off, so the clock need not be set again at startup. Standby power consumption while powered off has not been measured.
- To enter download mode manually, connect USB, hold the power button for about 2 seconds, and release it when the internal green LED lights up.
- Some v1.0 units have an incorrect `BAT` label: the pin is actually 5V IN and must not be connected to a battery. Hardware revision v1.0.1 changes the pin to an actual BAT connection. Check the [official hardware revision notes](https://docs.m5stack.com/en/core/StopWatch) before wiring.

## Documentation and licensing

The 649-species firmware booted on a C152 and reloaded a save on 2026-10-30. That limited observation predates the schema7 five-region version and does not establish complete device acceptance for the current firmware. See the [verification notes (Chinese only)](docs/verification.md) for current host checks, device observations, and pending checks for real step counting, physical interaction, RTC retention, power-loss recovery, and power consumption. Host tests cannot establish real step counting, on-device feel, latency, or power-loss recovery.

- [Usage guide](docs/usage.en.md): controls, catching, training, dates, and saves.
- [Simulator guide (Chinese only)](docs/simulator.md): keyboard and mouse controls, separate saves, and simulation boundaries.
- [Development guide (Chinese only)](docs/development.md): environment, asset generation, tests, and module responsibilities.
- [Gameplay and migration contract (Chinese only)](docs/design/walking-progression.md): account invariants, evolution, and compatibility with older saves.
- [Verification notes (Chinese only)](docs/verification.md): scope of automated checks, current limitations, and pending device checks.

Original code is licensed under [MIT](LICENSE). Pokémon images, names, data snapshots, and third-party libraries are not covered by this project's MIT license. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for sources and their respective notices. This project is not affiliated with or endorsed by Nintendo, Creatures, GAME FREAK, or The Pokémon Company.
