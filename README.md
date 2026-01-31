# Raspberry Pi LED 狀態指示與按鈕控制守護進程

這是一個為 Raspberry Pi 設計的高效 LED 狀態指示系統，使用 C++ 編寫的常駐程式，透過 libgpiod 控制硬體。系統包含 LED 閃爍狀態指示和按鈕控制功能，可透過按鈕執行關機與重開機操作。

## 功能特性

### LED 狀態指示
- **運行狀態**：系統運行中時，LED 以 1 秒間隔規律閃爍
- **關機序列**：收到關機訊號時，LED 快速閃爍 10 次後熄滅
- **重開機序列**：收到重開機訊號時，LED 快速閃爍 3 次後熄滅

### 按鈕控制
- **關機按鈕**（GPIO 18）：按住 10 秒執行系統關機
- **重開機按鈕**（GPIO 26）：按住 3 秒執行系統重開機
- 按鈕具備防抖動機制（80ms 延遲）防止誤觸發

### 系統特點
- 低資源消耗的 C++ 常駐程式
- 使用現代化的 libgpiod GPIO 介面
- 完整的訊號處理（SIGTERM、SIGINT、SIGHUP）
- 系統日誌記錄（syslog）
- 守護進程模式運行

## 硬體需求

### Raspberry Pi 相容性
- Raspberry Pi 4
- Raspberry Pi 3 或其他相容機型

### GPIO 連接

| 功能 | GPIO 針腳 (BCM) | 物理針腳 | 說明 |
|------|-----------------|----------|------|
| LED | GPIO 17 | Pin 11 | 輸出模式 |
| 關機按鈕 | GPIO 18 | Pin 12 | 輸入模式，使用內部上拉電阻 |
| 重開機按鈕 | GPIO 26 | Pin 37 | 輸入模式，使用內部上拉電阻 |

### 按鈕接線說明
- 按鈕一端連接對應的 GPIO 針腳
- 按鈕另一端連接到 GND（接地）
- 程式會啟用內部上拉電阻，因此按鈕按下時讀取值為 LOW（0）

### GPIO 晶片
- 使用 `gpiochip0`（Raspberry Pi 預設 GPIO 晶片）

## 系統需求

### 軟體套件
```bash
# 更新系統套件清單
sudo apt update

# 安裝 libgpiod 開發庫
sudo apt install libgpiod-dev

# 確認 C++ 編譯器已安裝（通常預裝）
g++ --version
```

### 編譯需求
- C++17 或更新標準的編譯器
- libgpiod 開發庫

## 編譯與安裝

### 方法一：使用建置腳本（推薦）

```bash
# 給予執行權限（如果尚未設定）
chmod +x build.sh

# 執行編譯
./build.sh

# 將編譯好的程式複製到系統路徑
sudo cp output/led_status_daemon_button /usr/local/bin/
```

### 方法二：手動編譯

```bash
# 建立輸出目錄
mkdir -p output

# 編譯完整版本（含按鈕功能）
g++ led_status_daemon_button.cpp -o output/led_status_daemon_button -lgpiod -lstdc++fs -Wall -std=c++17

# 編譯簡易版本（僅 LED 功能，無按鈕）
g++ led_status_daemon.cpp -o output/led_status_daemon -lgpiod -lstdc++fs -Wall -std=c++17
```

### 編譯參數說明
- `-std=c++17`：使用 C++17 標準
- `-Wall`：啟用所有常見警告
- `-lgpiod`：連結 libgpiod 函式庫
- `-lstdc++fs`：連結 C++ 檔案系統函式庫

## 系統服務設定

### 建立 Systemd 服務檔案

建立 `/etc/systemd/system/led_status_daemon.service`：

```ini
[Unit]
Description=LED Status Daemon with Button Shutdown for Raspberry Pi
After=network.target

[Service]
ExecStart=/usr/local/bin/led_status_daemon_button
ExecStop=/bin/kill -TERM $MAINPID
Type=forking
Restart=on-failure
User=root
Group=root
StandardOutput=syslog
StandardError=syslog
SyslogIdentifier=led_status_daemon

[Install]
WantedBy=multi-user.target
```

### 啟動服務

```bash
# 重新載入 systemd 配置
sudo systemctl daemon-reload

# 啟用開機自動啟動
sudo systemctl enable led_status_daemon.service

# 啟動服務
sudo systemctl start led_status_daemon.service

# 查看服務狀態
sudo systemctl status led_status_daemon.service
```

## 測試與驗證

### 檢查服務日誌

```bash
# 查看即時日誌
journalctl -u led_status_daemon.service -f

# 查看最近 50 行日誌
journalctl -u led_status_daemon.service -n 50
```

### 測試 LED 功能
1. 觀察 LED 是否以 1 秒間隔閃爍
2. 執行關機指令後，觀察 LED 是否快速閃爍 10 次後熄滅
   ```bash
   sudo shutdown -h now
   ```

### 測試按鈕功能
1. **關機測試**：按住 GPIO 18 的按鈕 10 秒
   - 觀察日誌顯示按鈕按下訊息
   - 10 秒後觸發關機序列
   - LED 快速閃爍 10 次
   - 系統開始關機

2. **重開機測試**：按住 GPIO 26 的按鈕 3 秒
   - 觀察日誌顯示重開機按鈕按下訊息
   - 3 秒後觸發重開機序列
   - LED 快速閃爍 3 次
   - 系統執行重開機

## 程式架構

### 主要檔案

| 檔案 | 說明 |
|------|------|
| `led_status_daemon_button.cpp` | 完整版本，包含 LED 閃爍與按鈕控制 |
| `led_status_daemon.cpp` | 簡易版本，僅 LED 閃爍功能 |
| `build.sh` | 編譯腳本 |
| `AGENTS.md` | 開發代理程式的程式碼風格指南 |

### 程式流程

1. **初始化階段**
   - 設定訊號處理器（SIGTERM、SIGINT）
   - 轉換為守護進程（daemonize）
   - 開啟 syslog 日誌系統
   - 初始化 GPIO 晶片與線路

2. **主循環**
   - LED 狀態切換（亮/滅）
   - 讀取按鈕狀態
   - 檢測長按（防抖動 80ms）
   - 等待下一次循環

3. **關閉階段**
   - 執行關機或重開機序列
   - 執行系統指令（如適用）
   - 釋放 GPIO 資源
   - 關閉日誌系統

## 常見問題

### Q: LED 沒有閃爍
**A:** 檢查以下項目：
- 確認 GPIO 17 連接正確
- 確認 LED 極性（長腳接正極）
- 查看服務日誌：`journalctl -u led_status_daemon.service -n 50`
- 確認服務狀態：`sudo systemctl status led_status_daemon.service`

### Q: 按鈕沒有作用
**A:** 檢查以下項目：
- 確認按鈕連接到正確的 GPIO 針腳（18 或 26）
- 確認按鈕另一端連接到 GND
- 查看日誌中是否有按鈕按下的訊息
- 確認按鈕按住時間達到要求（10 秒或 3 秒）

### Q: 如何調整閃爍頻率？
**A:** 編輯原始碼中的常數：
```cpp
// 修改這些數值（單位：毫秒）
const std::chrono::milliseconds RUNNING_BLINK_ON_MS(1000);
const std::chrono::milliseconds RUNNING_BLINK_OFF_MS(1000);
```
修改後需重新編譯並重新啟動服務。

### Q: 如何調整按鈕長按時間？
**A:** 編輯原始碼中的常數：
```cpp
// 關機按鈕長按時間（10 秒）
const std::chrono::milliseconds BUTTON_PRESS_DURATION_MS(10000);

// 重開機按鈕長按時間（3 秒）
const std::chrono::milliseconds REBOOT_PRESS_DURATION_MS(3000);
```

### Q: 如何停止服務？
**A:** 執行以下指令：
```bash
# 停止服務
sudo systemctl stop led_status_daemon.service

# 禁用開機自動啟動
sudo systemctl disable led_status_daemon.service
```

## 授權與貢獻

本專案為開源專案，歡迎提供問題回報與功能建議。

## 相關資源

- [libgpiod 官方文件](https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/)
- [Raspberry Pi GPIO 文件](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html)
- [Systemd 服務管理](https://www.freedesktop.org/software/systemd/man/systemd.service.html)
