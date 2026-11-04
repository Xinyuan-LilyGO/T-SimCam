<div align="center" markdown="1">
  <img src=".github/LilyGo_logo.png" alt="LilyGo logo" width="100"/>
</div>

<h1 align="center">T-SimCam</h1>

## **[English](README.MD) | 中文**

## 最新消息

- T-SimCam V1.3 使用 I2S 麦克风。
- T-SimCam V1.4 使用 PDM 麦克风。
- 所有示例均使用 Arduino-ESP32 `3.3.11`。开发板没有显示屏，因此所有测试信息均通过串口输出。

## 1️⃣ 示例预览

```text
examples
├─factory                           # 完整串口出厂测试
├─ATDebug                           # Modem AT 指令透传
├─CameraWebServerExample            # 通过开放 Wi-Fi 热点查看摄像头画面
├─MinimalAuxSerialExample           # 辅助 UART 透传及回环接口
├─MinimalButtonExample              # BOOT 按键测试
├─MinimalCameraExample              # 摄像头抓帧测试
├─MinimalIRCutExample               # IR-cut 控制测试
├─MinimalMicrophoneExample          # I2S/PDM 实时声压条
├─MinimalSDCardExample              # microSD 读写测试
└─MinimalWirelessExample            # Wi-Fi 和 Bluetooth LE 扫描
```

## 2️⃣ 快速开始

#### PlatformIO

1. 安装 [Visual Studio Code](https://code.visualstudio.com/) 和 [Python](https://www.python.org/).
2. 在 Visual Studio Code 扩展中安装 `PlatformIO IDE`.
3. 扩展安装完成后重启 Visual Studio Code。
4. 选择 `File` -> `Open Folder`，打开 `T-SimCam` 目录。
5. V1.3 I2S 麦克风版本选择 `v1_3`，V1.4 PDM 麦克风版本选择 `v1_4`.
6. 点击状态栏中的对勾进行编译。
7. 使用 USB 将开发板连接到电脑，然后点击上传箭头。
8. 打开串口监视器，波特率设置为 `115200`.

PlatformIO 的示例目录由 `platformio.ini` 中的 `src_dir` 选择。编译其他示例时修改该项，例如：

```ini
src_dir = examples/MinimalMicrophoneExample
```

如需直接通过网页查看摄像头，使用 `examples/CameraWebServerExample`。示例会建立 `T-SimCam-Cam-XXXX` 开放热点，每秒重复输出连接提示，并在 `http://192.168.4.1` 提供画面。

```ini
src_dir = examples/CameraWebServerExample
```

命令行编译：

```bash
pio run -e v1_3
pio run -e v1_4
```

编译 `factory` 前，在 `examples/factory/config.h` 中设置摄像头验收使用的 Wi-Fi：

```cpp
#define FACTORY_WIFI_SSID "测试Wi-Fi名称"
#define FACTORY_WIFI_PASSWORD "测试Wi-Fi密码"
```

摄像头连接最长等待 30 秒。无法连接时，开发板会自动建立开放的 `T-SimCam-XXXX` 热点，并通过串口输出浏览器地址。

#### Arduino IDE

1. 安装 [Arduino IDE](https://www.arduino.cc/en/software).
2. 安装 [Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/) 开发板包 `3.3.11`.
3. 打开所需示例目录中的 `.ino` 文件。每个示例都有本地 `config.h`.
4. 打开 `Tools` 并使用以下设置：
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
5. 默认选择 V1.3,V1.4 用户修改当前示例 `config.h` 中的版本：

```cpp
#define T_SIMCAM_BOARD_VERSION T_SIMCAM_V1_4
```

6. 连接开发板并点击 Upload。

## 3️⃣ 注意事项

1. 所有示例都可以直接使用 Arduino IDE 打开，`.ino` 文件名与所在目录名称一致。
2. 使用板载外设前必须将 `BOARD_POWER_ON_PIN` 设置为 HIGH，本仓库中的所有示例均已完成该操作。
3. 麦克风硬件版本必须与固件环境一致：V1.3 为 I2S，V1.4 为 PDM。
4. 所有示例均使用 `115200` 波特率输出 USB 串口信息。
5. Modem、SIM 卡、摄像头和 microSD 卡必须在断电状态下安装。Factory 测试要求 SIM 卡未设置 PIN 锁，并检测 `CPIN READY` 和 ICCID。
6. 摄像头位于 Factory 测试最后，会尝试连接设置的 Wi-Fi 30 秒；失败时自动建立开放的 `T-SimCam-XXXX` 热点，并通过串口输出实时画面的浏览器地址。
7. 无法上传固件时，手动进入下载模式：
   1. 连接 USB 线。
   2. 按住 BOOT。
   3. 保持 BOOT 按下，同时按下并释放 RESET。
   4. 释放 BOOT，然后重新上传。


## 🔧 硬件说明

下面的 ESP32-S3 GPIO 编号同时也是 Arduino pin 编号。以下为当前 T-SimCam 硬件 pinout；V1.3 和 V1.4 共用摄像头、存储和 modem 引脚。

### 摄像头 pinout

| 名称 | GPIO | 说明 |
| ---------------- | ---- | -------------------------------- |
| Camera PWDN      | -1   | 未连接                           |
| Camera RESET     | -1   | 未连接                           |
| Camera XCLK      | 14   |                                  |
| Camera SCCB SDA  | 4    | 摄像头控制总线                   |
| Camera SCCB SCL  | 5    | 摄像头控制总线                   |
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

### 麦克风 pinout

| 名称 | GPIO | 说明 |
| ---------------- | ---- | -------------------------------- |
| 麦克风 DATA      | 2    | V1.3 I2S 与 V1.4 PDM 共用       |
| I2S BCLK         | 41   | 仅 V1.3                         |
| I2S WS           | 42   | 仅 V1.3                         |
| PDM CLK          | 42   | 仅 V1.4                         |

### microSD pinout

| 名称 | GPIO | 说明 |
| ---------------- | ---- | -------------------------------- |
| microSD MISO     | 40   | SPI                              |
| microSD MOSI     | 38   | SPI                              |
| microSD SCLK     | 39   | SPI                              |
| microSD CS       | 47   | SPI                              |

### Modem pinout

| 名称 | GPIO | 说明 |
| ---------------- | ---- | -------------------------------- |
| Modem RX         | 46   | UART                             |
| Modem TX         | 45   | UART                             |
| Modem 电源键     | 48   | 由 modem 电路定义有效电平       |
| Modem 状态       | 21   | Modem 状态输入                  |

### 其他引脚

| 名称 | GPIO | 说明 |
| ------------------------- | ---- | -------------------------------- |
| 板载外设电源使能          | 1    | 使用外设前设置为 HIGH             |
| BOOT 按键                 | 0    | 低电平有效                       |
| 电池 ADC                  | 3    | 2:1 分压                         |
| IR-cut 控制               | 18   |                                  |
| 辅助 UART RX              | 44   |                                  |
| 辅助 UART TX              | 43   |                                  |

* GPIO43,GPIO44 也可以被配置为I2C,注意这两个GPIO并未配置上拉电阻,默认作为UART输出
