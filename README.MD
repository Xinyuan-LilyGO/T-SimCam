<div align="center" markdown="1">
  <img src=".github/LilyGo_logo.png" alt="LilyGo logo" width="100"/>
</div>

<h1 align="center">T-SimCam</h1>

## **English | [中文](README_CN.MD)**

## News

- T-SimCam V1.3 uses an I2S microphone.
- T-SimCam V1.4 uses a PDM microphone.
- All examples use Arduino-ESP32 `3.3.11` and print test information through the serial port because the board has no display.

## 1️⃣ Examples preview

```text
examples
├─factory                           # Complete serial factory test
├─ATDebug                           # Modem AT command bridge
├─CameraWebServerExample            # Camera viewer through an open Wi-Fi hotspot
├─MinimalAuxSerialExample           # Auxiliary UART bridge and loopback interface
├─MinimalButtonExample              # BOOT button test
├─MinimalCameraExample              # Camera capture test
├─MinimalIRCutExample               # IR-cut control test
├─MinimalMicrophoneExample          # I2S/PDM live audio level meter
├─MinimalSDCardExample              # microSD read/write test
└─MinimalWirelessExample            # Wi-Fi and Bluetooth LE scan
```

## 2️⃣ Quick Start

#### PlatformIO

1. Install [Visual Studio Code](https://code.visualstudio.com/) and [Python](https://www.python.org/).
2. Install the `PlatformIO IDE` extension in Visual Studio Code.
3. Restart Visual Studio Code after the extension installation finishes.
4. Select `File` -> `Open Folder`, then open the `T-SimCam` directory.
5. Select `v1_3` for the V1.3 I2S microphone board or `v1_4` for the V1.4 PDM microphone board.
6. Click the check mark in the status bar to compile.
7. Connect the board to the computer through USB and click the upload arrow.
8. Open the serial monitor at `115200` baud.

The example source directory is selected by `src_dir` in `platformio.ini`. To compile another example, change it, for example:

```ini
src_dir = examples/MinimalMicrophoneExample
```

For direct browser camera viewing, use `examples/CameraWebServerExample`. It starts an open hotspot named `T-SimCam-Cam-XXXX`, prints the connection instructions once per second, and serves the viewer at `http://192.168.4.1`.

```ini
src_dir = examples/CameraWebServerExample
```

Command-line builds:

```bash
pio run -e v1_3
pio run -e v1_4
```

Before building `factory`, set the camera inspection network in `examples/factory/config.h`:

```cpp
#define FACTORY_WIFI_SSID "your-test-wifi"
#define FACTORY_WIFI_PASSWORD "your-test-password"
```

The camera connection waits up to 30 seconds. If it cannot join this network, the board starts an open `T-SimCam-XXXX` hotspot and prints its browser URL over serial.

#### Arduino IDE

1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Install the [Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/) board package version `3.3.11`.
3. Open the `.ino` file in the required example directory. Each example has a local `config.h` .
4. Open `Tools` and use these settings:
   - Board -> ESP32S3 Dev Module
   - USB CDC On Boot -> Enabled
   - CPU Frequency -> 240MHz (WiFi)
   - USB DFU On Boot -> Disabled
   - Flash Mode -> QIO 80MHz
   - Flash Size -> 16MB (128Mb)
   - USB Firmware MSC On Boot -> Disabled
   - PSRAM -> OPI PSRAM
   - Partition Scheme -> 16M Flash (3MB APP/9.9MB FATFS)
   - USB Mode -> Hardware CDC and JTAG
   - Upload Mode -> UART0 / Hardware CDC
   - Upload Speed -> 921600
5. V1.3 is selected by default. For V1.4, change the following line in the current sketch's `config.h`:

```cpp
#define T_SIMCAM_BOARD_VERSION T_SIMCAM_V1_4
```

6. Connect the board and click Upload.

## 3️⃣ Tips

1. All examples can be opened directly with Arduino IDE. Every sketch file name matches its containing directory.
2. `BOARD_POWER_ON_PIN` must be driven HIGH before using onboard peripherals. All examples in this repository already do this.
3. Use the correct firmware environment for the microphone hardware. V1.3 is I2S and V1.4 is PDM.
4. All examples use `115200` baud for USB serial output.
5. Install the modem, SIM card, camera and microSD card while the board is powered off. The factory test requires an unlocked SIM card and verifies both `CPIN READY` and ICCID.
6. The camera runs last in the factory test. It tries the configured Wi-Fi for 30 seconds, then falls back to an open `T-SimCam-XXXX` hotspot if necessary. The browser URL is printed over serial.
7. If uploading fails, enter download mode manually:
   1. Connect the USB cable.
   2. Press and hold BOOT.
   3. Press and release RESET while BOOT remains pressed.
   4. Release BOOT and upload again.

## 🔧 Hardware Description

The ESP32-S3 GPIO numbers below are also the Arduino pin numbers. The pinout is the current T-SimCam hardware pinout; V1.3 and V1.4 share the camera, storage and modem pins.

### Camera pinout

| Name | GPIO | Note |
| ---------------- | ---- | -------------------------------- |
| Camera PWDN      | -1   | Not connected                    |
| Camera RESET     | -1   | Not connected                    |
| Camera XCLK      | 14   |                                  |
| Camera SCCB SDA  | 4    | Camera control bus               |
| Camera SCCB SCL  | 5    | Camera control bus               |
| Camera Y9 / D9   | 15   |                                  |
| Camera Y8 / D8   | 16   |                                  |
| Camera Y7 / D7   | 17   |                                  |
| Camera Y6 / D6   | 12   |                                  |
| Camera Y5 / D5   | 10   |                                  |
| Camera Y4 / D4   | 8    |                                  |
| Camera Y3 / D3   | 9    |                                  |
| Camera Y2 / D2   | 11   |                                  |
| Camera VSYNC     | 6    |                                  |
| Camera HREF      | 7    |                                  |
| Camera PCLK      | 13   |                                  |

### Microphone pinout

| Name | GPIO | Note |
| ---------------- | ---- | -------------------------------- |
| Microphone DATA  | 2    | Shared by I2S V1.3 and PDM V1.4 |
| I2S BCLK         | 41   | V1.3 only                       |
| I2S WS           | 42   | V1.3 only                       |
| PDM CLK          | 42   | V1.4 only                       |

### microSD pinout

| Name | GPIO | Note |
| ---------------- | ---- | -------------------------------- |
| microSD MISO     | 40   | SPI                              |
| microSD MOSI     | 38   | SPI                              |
| microSD SCLK     | 39   | SPI                              |
| microSD CS       | 47   | SPI                              |

### Modem pinout

| Name | GPIO | Note |
| ---------------- | ---- | -------------------------------- |
| Modem RX         | 46   | UART                               |
| Modem TX         | 45   | UART                               |
| Modem power key  | 48   | Active according to modem circuit |
| Modem status     | 21   | Modem status input                 |

### Other pins

| Name | GPIO | Note |
| ------------------------- | ---- | -------------------------------- |
| Board power enable        | 1    | Drive HIGH before using peripherals |
| BOOT button               | 0    | Active low                         |
| Battery ADC               | 3    | 2:1 divider                        |
| IR-cut control            | 18   |                                   |
| Auxiliary UART RX         | 44   |                                   |
| Auxiliary UART TX         | 43   |                                   |

* GPIO43 and GPIO44 can also be configured for I2C; note that these two GPIOs do not have pull-up resistors configured and default to functioning as UART outputs.
