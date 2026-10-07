# WeMosD1_PC_PowerCtrl

通过局域网 HTTP 控制 PC 主机电源（模拟主板电源键短按/长按）。

---

## 硬件

| 部件 | 型号 / 规格 | 数量 |
|------|------------|------|
| 主控 | WeMos D1 Mini（ESP8266，ESP-12F 模组） | 1 |
| MOS 模块 | LR7843 光耦隔离 MOS 模块（30V / 161A） | 1 |
| 主板电源针 | 主板前面板 `PWR_BTN` 双针 | — |

---

## 功能

通过局域网 HTTP 接口远程控制 PC 主机的电源，模拟物理电源键的短按与长按：

- **短按 300ms**：开机 / 从睡眠唤醒 / 软关机（取决于系统状态）
- **长按 5000ms**：强制断电（>4s 大多数主板会触发硬关机）
- Web 控制页面（手机/电脑浏览器均可访问）
- RESTful JSON API（便于接入 Home Assistant、自动化脚本等）

---

## HTTP API

| 方法 | 路径 | 说明 |
|------|------|------|
| `GET /` | 浏览器访问的 Web UI（两个大按钮 + 状态） | |
| `GET /power?action=press` | 短按电源键（300ms） | |
| `GET /power?action=long`  | 长按电源键（5s，强制关机） | |
| `GET /status` | 返回 JSON：`{"wifi":true,"ip":"192.168.x.x"}` | |

### 示例

```bash
# 短按一次（开机）
curl http://esp8266-pc.local/power?action=press

# 长按（强制关机）
curl http://esp8266-pc.local/power?action=long

# 查看状态
curl http://esp8266-pc.local/status
```

---

## 接线

### ESP8266 ↔ LR7843 MOS 模块

```
WeMos D1 Mini                LR7843 模块（PWM 侧）
┌──────────────┐             ┌──────────────┐
│           5V │────────────►│              │
│          GND │────────────►│ GND          │
│           D1 │────────────►│ PWM          │
└──────────────┘             └──────────────┘
```

> WeMos D1 上 D1 引脚 = **GPIO5**。

### LR7843 MOS 模块 ↔ PC 主板 PWR_BTN

> ⚠️ **重要**：LR7843 模块的负载侧（+ / - / LOAD）是低端开关拓扑，
> 适合驱动电机/电磁阀这类有源负载；直接用于主板 PWR_BTN 时需要先
> 用万用表确认 + / LOAD 在 PWM=HIGH 时的导通关系，再确定如何接到
> 主板 `PWR_BTN` 的两根针。
>
> 推荐接线：用主板 `PWR_BTN` 两针作为负载，外部不另接电源；
> 若模块需要独立供电，把 + / - 接到一对「低电压、低电流」信号源上。
>
> 如果接线后 PC 没有反应或表现异常，请改用 5V 继电器模块代替 MOS
> 模块（继电器是机械触点，更接近物理按键的「按下-弹起」行为）。

---

## 编译与烧录

> 使用 **PlatformIO + Arduino 框架**，与仓库其他 WeMosD1 项目一致。

### 1. 配置 WiFi

编辑 `platformio.ini`，把 `WIFI_SSID` 和 `WIFI_PASSWORD` 改成你自己的：

```ini
-D WIFI_SSID="你的WiFi名"
-D WIFI_PASSWORD="你的WiFi密码"
```

或者更安全的做法：通过环境变量传入，避免把密码提交到 git：

```bash
export PLATFORMIO_BUILD_FLAGS="-DWIFI_SSID=\\\"MyWiFi\\\" -DWIFI_PASSWORD=\\\"MyPass\\\""
```

### 2. 编译

```bash
cd WeMosD1_PC_PowerCtrl
pio run
```

### 3. 烧录

```bash
# 自动检测串口
pio run -t upload

# 或指定串口（将 /dev/cu.usbserial-XXX 替换成实际串口）
pio run -t upload --upload-port /dev/cu.usbserial-210
```

### 4. 查看日志

```bash
pio device monitor
# 或指定串口和波特率
pio device monitor -p /dev/cu.usbserial-210 -b 115200
```

烧录后第一次启动会自动连接配置的 WiFi；连上后串口日志会打印 `IP`，用浏览器访问 `http://<IP>/` 或 `http://pc-power.local/` 即可看到 Web UI。

---

## 状态指示（板载 LED = GPIO2）

| 状态 | 行为 |
|------|------|
| WiFi 未连接 | 1 秒 1 次慢闪 |
| WiFi 已连接，空闲 | 常亮 |
| 触发电源动作 | 串口日志 `[POWER] PWR_BTN pulse: XXXms` |

---

## 后续可扩展方向

- [ ] 接入 MQTT（需要本地 broker 或云平台）
- [ ] 增加「读取 PC 当前电源状态」功能（通过主板 USB 待机电压反馈，需要 ADC + 光耦）
- [ ] 改用 5V 继电器模块以提高兼容性
- [ ] OTA 升级
- [ ] Home Assistant 集成（REST sensor / command_line 平台）
