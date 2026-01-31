### Bash Script 和 C/C++ 實現 Raspberry Pi 4 LED 閃爍狀態指示，哪種更節省系統資源的分析：
綜合分析

    系統資源消耗：
        C/C++ (常駐程式)： 通常會顯著更節省系統資源。C/C++ 編譯後的二進位檔案直接執行，對記憶體和 CPU 的需求極低。一個設計良好的 C/C++ 常駐程式，可以非常高效地運行，幾乎不佔用系統資源。
        Bash Script： 相對而言會消耗更多的系統資源。Bash 腳本需要 Bash 直譯器來解析和執行。每次腳本運行時，都會啟動一個 Bash 進程，並且腳本中調用的外部命令（如 gpio 或 echo 到 /sys/class/gpio）也會啟動單獨的進程。這會導致更高的 CPU 和記憶體開銷，尤其是在頻繁操作 LED 的情況下。

    開發複雜度：
        C/C++： 開發複雜度相對較高。需要了解 C/C++ 語言特性、編譯器使用、GPIO 庫（如 WiringPi、pigpio 或直接操作 /sys/class/gpio）的用法，並處理錯誤和異常。對於初學者來說，學習曲線較陡峭。
        Bash Script： 開發複雜度相對較低。語法簡單，容易上手，可以直接使用現有的 gpio 命令或者透過 echo 命令操作 /sys/class/gpio。對於快速原型開發和簡單任務，Bash Script 是不錯的選擇。

    執行效率：
        C/C++： 執行效率極高。由於是編譯後的機器碼，執行速度快，響應時間短。
        Bash Script： 執行效率較低。每次命令都需要 Bash 直譯器解析，並且會啟動子進程來執行外部命令，這會引入延遲。

    可靠性與穩定性：
        C/C++： 編譯後的程式碼通常更穩定，也更容易進行錯誤處理和記憶體管理，從而提高可靠性。
        Bash Script： 相對容易出現錯誤，特別是在處理複雜邏輯或並發操作時。錯誤處理相對不夠靈活。

具體實現方案分析

Bash Script 方案

優點：

    易於實現： 學習成本低，可以快速搭建。
    無需編譯： 直接在 Raspberry Pi 上編輯和運行。

缺點：

    資源消耗較高： 每次操作 GPIO 都可能啟動新的進程（例如 gpio 命令或 echo 到 /sys/class/gpio）。
    延遲： 腳本解析和外部命令執行會引入一定的延遲，不適合對實時性要求高的應用。
    常駐運行複雜： 需要使用循環和延遲來實現閃爍，並可能需要額外的機制來監控系統狀態（開機/關機）。

大致實現思路：

開機運行中閃爍：
在 /etc/rc.local 或 systemd service 中啟動一個 Bash 腳本。
腳本進入一個無限循環，在循環中定時（例如每 0.5 秒）切換 GPIO 狀態。
可以使用 gpio -g mode 17 out 設定 GPIO 模式，然後使用 gpio -g write 17 1 和 gpio -g write 17 0 來控制 LED 亮滅。
或者直接操作 /sys/class/gpio： 
```bash       
Bash

echo 17 > /sys/class/gpio/export # 匯出 GPIO
echo out > /sys/class/gpio/gpio17/direction # 設定方向
while true; do
    echo 1 > /sys/class/gpio/gpio17/value
    sleep 0.5
    echo 0 > /sys/class/gpio/gpio17/value
    sleep 0.5
done
```

關機程序閃爍：

需要一個機制來監聽關機事件。這通常是透過 systemd 的 Before=shutdown.target 或類似的機制來實現。
在關機前執行的腳本中，讓 LED 快速閃爍 3 次，然後熄滅。
例如：
```bash
#!/bin/bash
# 假設 GPIO 17 已匯出並設定為輸出
for i in {1..3}; do
    echo 1 > /sys/class/gpio/gpio17/value
    sleep 0.1
    echo 0 > /sys/class/gpio/gpio17/value
    sleep 0.1
done
echo 0 > /sys/class/gpio/gpio17/value # 確保最終熄滅
echo 17 > /sys/class/gpio/unexport # 解除匯出 GPIO
```
C/C++ 常駐程式方案

優點：

    資源消耗極低： 編譯後的二進位檔案直接執行，記憶體和 CPU 佔用極小。
    執行效率高： 幾乎沒有延遲，實時性好。
    穩定可靠： 容易進行錯誤處理和記憶體管理。
    更靈活： 可以集成更複雜的邏輯，例如監聽系統事件、處理多個 LED 等。

缺點：

    開發複雜度較高： 需要編程知識和對 GPIO 庫的理解。
    需要編譯： 在 Raspberry Pi 上進行編譯，或者在交叉編譯環境中編譯。

大致實現思路：

* 選擇 GPIO 庫：
WiringPi (已停止維護，但仍廣泛使用): 簡單易用，提供類似 Arduino 的 API。
* pigpio: 更強大，提供精確的 PWM 控制，適合對時間要求嚴格的應用。
* 直接操作 /sys/class/gpio: 最底層，但需要手動處理檔案操作。
* libgpiod: 較新的 Linux GPIO 介面，是未來趨勢。

常駐程式結構：
使用 daemon() 函數或手動實現守護進程邏輯。
在主循環中，使用 poll() 或 select() 監聽系統事件（例如 SIGTERM 訊號用於關機），或者使用 sleep() 定時。
透過選定的 GPIO 庫來控制 LED 亮滅。

開機運行中閃爍：
在程式初始化時，設定好 GPIO 模式。
進入一個無限循環，在循環中定時切換 LED 狀態。

例如，使用 WiringPi 庫： 
```cpp
#include <wiringPi.h>
#include <iostream>
#include <unistd.h> // For sleep

const int LED_PIN = 17; // Use BCM pin numbering

int main() {
    if (wiringPiSetupGpio() == -1) { // Initialize WiringPi with BCM numbering
        std::cerr << "wiringPiSetupGpio failed!" << std::endl;
        return 1;
    }
    pinMode(LED_PIN, OUTPUT);

    while (true) {
        digitalWrite(LED_PIN, HIGH);
        usleep(500000); // 0.5 seconds
        digitalWrite(LED_PIN, LOW);
        usleep(500000);
    }
    return 0;
}
```
關機程序閃爍：

    程式需要捕獲 SIGTERM (終止訊號) 或 SIGINT (中斷訊號)。
    在訊號處理函數中，執行 LED 閃爍 3 次的邏輯，然後退出。
    例如： 
```cpp=
#include <wiringPi.h>
#include <iostream>
#include <unistd.h>
#include <signal.h> // For signal handling

const int LED_PIN = 17;
bool running = true;

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    running = false; // Set flag to exit main loop
    // Perform shutdown sequence here
    for (int i = 0; i < 3; ++i) {
        digitalWrite(LED_PIN, HIGH);
        usleep(100000); // 0.1 seconds
        digitalWrite(LED_PIN, LOW);
        usleep(100000);
    }
    digitalWrite(LED_PIN, LOW); // Ensure LED is off
    // You might want to unexport the GPIO here if using sysfs directly
    exit(signum); // Exit the program
}

int main() {
    // Register signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    if (wiringPiSetupGpio() == -1) {
        std::cerr << "wiringPiSetupGpio failed!" << std::endl;
        return 1;
    }
    pinMode(LED_PIN, OUTPUT);

    while (running) {
        digitalWrite(LED_PIN, HIGH);
        usleep(500000);
        digitalWrite(LED_PIN, LOW);
        usleep(500000);
    }

    return 0;
}
```
將 C/C++ 程式配置為 systemd service，以便在開機時啟動並在關機時正確終止。在 systemd service unit file 中，可以設定 ExecStop 來觸發關機動畫，或者讓程式本身處理 SIGTERM。

結論與建議

就節省系統資源而言，C/C++ 常駐程式會顯著優於 Bash Script。

建議：

如果你是初學者，且對資源消耗不是極度敏感，或者只是需要快速原型開發，可以先嘗試 Bash Script。 它的學習曲線更平緩，可以讓你更快地看到結果。

如果你希望獲得最佳的性能、最低的資源消耗，以及更高的可靠性和靈活性，那麼強烈建議使用 C/C++ 來編寫常駐程式。 雖然初期學習成本較高，但對於長期運行和需要精確控制的應用來說，這是更好的選擇。
    
特性	Bash Script	C/C++ 常駐程式
資源消耗	較高	極低
開發難度	較低	較高
執行效率	較低	極高
穩定性	一般	高
適用場景	快速原型、簡單任務	長期運行、性能敏感應用

---
### 這是一份基於 C++ 語言，為 Raspberry Pi 4 實現 LED 閃爍狀態指示的完整方案。這個方案會包含常駐程式的設計、GPIO 操作、以及如何在系統啟動和關機時整合它。
Raspberry Pi 4 LED 狀態指示 C++ 方案

這個 C++ 方案旨在提供一個高效且資源友善的 LED 狀態指示器。它將使用 libgpiod 庫來控制 GPIO，因為它是 Linux 核心推薦的現代 GPIO 介面，比舊的 sysfs 介面更高效和安全。
方案概述

常駐程式 (Daemon)：程式將作為一個系統服務在後台運行，持續監控系統狀態並控制 LED 閃爍。
開機運行中：LED 會以特定頻率閃爍，表示系統正常運作。
關機程序：當系統接收到關機訊號時，LED 會快速閃爍 3 下，然後熄滅，指示關機過程。
錯誤處理：程式會包含基本的錯誤處理，例如 GPIO 初始化失敗。

軟體要求

    Raspberry Pi OS (或任何基於 Debian 的 Linux 發行版)
    C++ 編譯器 (通常預裝 g++)
    libgpiod 開發庫 (libgpiod-dev)

1. 安裝 libgpiod 開發庫

在你的 Raspberry Pi 上打開終端機，執行以下命令安裝必要的開發庫：
```bash
sudo apt update
sudo apt install libgpiod-dev
```
2. C++ 程式碼

這個程式會創建一個守護進程，控制一個連接到 GPIO pin 的 LED。

led_status_daemon.cpp

```cpp
#include <iostream>
#include <gpiod.h> // libgpiod 庫
#include <unistd.h> // for sleep functions (usleep)
#include <signal.h> // for signal handling (SIGTERM, SIGINT)
#include <syslog.h> // for logging to syslog
#include <thread>   // for std::this_thread::sleep_for
#include <chrono>   // for std::chrono::milliseconds
#include <sys/stat.h>   // <-- Add this line for umask()
// --- 配置參數 ---
// 根據你的 Raspberry Pi 型號和連接方式選擇正確的 GPIO chip 和 line number。
// 通常 Raspberry Pi 4 的 GPIO 是 gpiocip0。
// 請查閱你的硬體資料以確定正確的 GPIO BCM 編號。
// 例如，BCM 17 通常對應到 gpiocip0 的 line 17。
const char *GPIO_CHIP_NAME = "gpiochip0";
const unsigned int LED_GPIO_LINE = 17; // 例如，BCM 編號為 17 的 GPIO

// 運行中閃爍頻率 (單位: 毫秒)
const std::chrono::milliseconds RUNNING_BLINK_ON_MS(500);
const std::chrono::milliseconds RUNNING_BLINK_OFF_MS(500);

// 關機閃爍頻率 (單位: 毫秒)
const std::chrono::milliseconds SHUTDOWN_BLINK_ON_MS(100);
const std::chrono::milliseconds SHUTDOWN_BLINK_OFF_MS(100);
const int SHUTDOWN_BLINK_COUNT = 3;

// --- 全域變數 ---
struct gpiod_chip *chip = nullptr;
struct gpiod_line *led_line = nullptr;
volatile sig_atomic_t running = 1; // 控制主循環運行，volatile 確保跨執行緒可見

// --- 函式宣告 ---
void cleanup_gpio();
void signal_handler(int signum);
void shutdown_sequence();
void daemonize();

int main() {
    // 1. 設定訊號處理器
    // 捕獲 SIGTERM (終止訊號) 和 SIGINT (中斷訊號)
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    // 2. 轉換為守護進程 (Daemon)
    daemonize();

    // 初始化 syslog，用於記錄日誌
    openlog("led_status_daemon", LOG_PID|LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "LED status daemon starting.");

    // 3. 初始化 GPIO
    chip = gpiod_chip_open_by_name(GPIO_CHIP_NAME);
    if (!chip) {
        syslog(LOG_ERR, "Failed to open GPIO chip: %s", GPIO_CHIP_NAME);
        closelog();
        return 1;
    }

    led_line = gpiod_chip_get_line(chip, LED_GPIO_LINE);
    if (!led_line) {
        syslog(LOG_ERR, "Failed to get GPIO line: %d", LED_GPIO_LINE);
        gpiod_chip_close(chip);
        closelog();
        return 1;
    }

    // 請求 GPIO 線為輸出模式，並設置為初始狀態 (低電平，即 LED 滅)
    // 這裡設置 "led_status_daemon" 作為消費者名稱，方便調試
    int ret = gpiod_line_request_output(led_line, "led_status_daemon", 0);
    if (ret < 0) {
        syslog(LOG_ERR, "Failed to request GPIO line as output.");
        gpiod_line_release(led_line);
        gpiod_chip_close(chip);
        closelog();
        return 1;
    }

    syslog(LOG_INFO, "GPIO %d initialized successfully.", LED_GPIO_LINE);

    // 4. 主循環：開機運行中閃爍
    while (running) {
        gpiod_line_set_value(led_line, 1); // LED 亮
        std::this_thread::sleep_for(RUNNING_BLINK_ON_MS);

        if (!running) break; // 檢查是否收到停止訊號

        gpiod_line_set_value(led_line, 0); // LED 滅
        std::this_thread::sleep_for(RUNNING_BLINK_OFF_MS);
    }

    // 5. 執行關機序列
    syslog(LOG_INFO, "Received shutdown signal. Performing shutdown sequence.");
    shutdown_sequence();

    // 6. 清理 GPIO 資源
    cleanup_gpio();
    syslog(LOG_INFO, "LED status daemon stopped.");
    closelog();

    return 0;
}

// 清理 GPIO 資源
void cleanup_gpio() {
    if (led_line) {
        gpiod_line_release(led_line); // 釋放 GPIO 線
    }
    if (chip) {
        gpiod_chip_close(chip); // 關閉 GPIO chip
    }
}

// 訊號處理器
void signal_handler(int signum) {
    // 設置 running 旗標為 0，通知主循環停止
    running = 0;
}

// 關機序列：快速閃爍 3 次後熄滅
void shutdown_sequence() {
    for (int i = 0; i < SHUTDOWN_BLINK_COUNT; ++i) {
        gpiod_line_set_value(led_line, 1); // LED 亮
        std::this_thread::sleep_for(SHUTDOWN_BLINK_ON_MS);
        gpiod_line_set_value(led_line, 0); // LED 滅
        std::this_thread::sleep_for(SHUTDOWN_BLINK_OFF_MS);
    }
    gpiod_line_set_value(led_line, 0); // 確保 LED 最終熄滅
}

// 將程式轉換為守護進程
void daemonize() {
    pid_t pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE); // 錯誤處理
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS); // 父進程退出
    }

    // 子進程繼續
    if (setsid() < 0) { // 創建新的會話
        exit(EXIT_FAILURE);
    }

    signal(SIGHUP, SIG_IGN); // 忽略 SIGHUP 訊號

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS); // 再次退出父進程，確保程式沒有控制終端
    }

    umask(0); // 設定檔案權限遮罩
    chdir("/"); // 更改工作目錄到根目錄

    // 關閉標準檔案描述符
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}
```
程式碼說明：

* GPIO_CHIP_NAME 和 LED_GPIO_LINE: 這兩個參數非常重要，需要根據你的實際硬體連接進行調整。 對於 Raspberry Pi 4，通常 GPIO_CHIP_NAME 是 "gpiochip0"。LED_GPIO_LINE 是你連接 LED 的 GPIO 的 BCM 編號。例如，如果你連接到物理針腳 11 (BCM 17)，那麼 LED_GPIO_LINE 就是 17。
* libgpiod: 現代 Linux 系統推薦的 GPIO 介面。它提供了更 robust 和高效的方式來控制 GPIO。
* 守護進程 (Daemon): daemonize() 函數將程式轉換為一個在後台獨立運行的守護進程，脫離終端控制。
* 訊號處理: 程式捕獲 SIGTERM 和 SIGINT 訊號。當收到這些訊號時（通常在關機時由 systemd 發送），running 旗標會被設置為 0，觸發主循環退出並執行關機序列。
* 日誌記錄: 使用 syslog 將程式運行訊息記錄到系統日誌中，方便調試和監控。
* std::this_thread::sleep_for: 使用 C++11 引入的 std::chrono 和 std::thread 函式來進行時間延遲，提供更高精度和可讀性。

編譯程式碼

在 Raspberry Pi 上，打開終端機，導航到你保存 led_status_daemon.cpp 的目錄，然後執行以下命令編譯：

```bash
g++ led_status_daemon.cpp -o led_status_daemon -lgpiod -lstdc++fs -Wall -std=c++17
```

* -o led_status_daemon: 指定輸出可執行檔的名稱為 led_status_daemon。
* -lgpiod: 鏈接 libgpiod 庫。
* -lstdc++fs: (可選) 針對 C++17 的檔案系統庫，本程式未使用，但一些複雜 C++ 專案可能會用到。
* -Wall: 開啟所有常見警告。
* -std=c++17: 使用 C++17 標準編譯 (推薦，因為程式使用了 std::chrono 等現代 C++ 特性)。

編譯成功後，你會在同一個目錄下得到一個名為 led_status_daemon 的可執行檔。

---
***詳解 void daemonize() 函數***
在 Linux 系統中，守護進程（Daemon）是一種在後台運行的特殊程式。它通常沒有控制終端，不受用戶登入登出的影響，並且作為一個系統服務持續運行。daemonize() 函數的目的就是讓我們的 LED 狀態指示程式符合這些特性。

這個函數實現了一個經典的 Unix 守護進程化流程，它包含了幾個關鍵步驟：
1. 第一次 fork()
```cpp
    pid_t pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE); // 錯誤處理
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS); // 父進程退出
    }
```
* fork() 的作用：fork() 系統呼叫會創建一個新的子進程，這個子進程是父進程的一個幾乎完全相同的副本（包括記憶體、檔案描述符等）。
* 父進程退出：當 fork() 成功時，父進程會收到子進程的 Process ID (PID)，此時 pid 大於 0。父進程會立即呼叫 exit(EXIT_SUCCESS) 結束。
* 為什麼要父進程退出？

    - 脫離控制終端：這是關鍵一步。當父進程退出時，它與控制它的終端之間的連接會被切斷。這使得子進程（現在是孤兒進程）不再受該終端的控制，即使終端關閉，子進程也能繼續運行。
    - 讓 shell 認為命令已經完成：如果你直接在終端執行一個程式，然後它自己變成了守護進程，那麼終端會一直等待這個程式結束。通過父進程退出，shell 會立即返回提示符，讓用戶可以繼續操作。
2. 創建新會話 setsid()
```cpp
// 子進程繼續
    if (setsid() < 0) { // 創建新的會話
        exit(EXIT_FAILURE);
    }
```

* setsid() 的作用：這個函數用於創建一個新的會話（Session ID, SID）。
    * 呼叫 setsid() 的進程會成為新會話的會話首領（Session Leader）。
    * 同時，它會成為一個新的**進程組（Process Group）**的組首領。
    * 最重要的是，它會脫離任何控制終端。如果一個進程有控制終端，它就不能成為新的會話首領。
* 為什麼需要 setsid()？ 雖然第一次 fork() 已經讓進程脫離了終端，但 setsid() 更徹底地確保了進程與任何現有控制終端的完全分離，防止未來可能意外地被終端控制。這是一個標準的守護進程步驟。



3. 忽略 SIGHUP 訊號
```cpp
signal(SIGHUP, SIG_IGN); // 忽略 SIGHUP 訊號
```
* SIGHUP 訊號：這個訊號通常在控制終端被關閉（掛斷）時發送給進程組首領。如果你的程式沒有處理這個訊號，可能會在終端關閉時終止。
* SIG_IGN：將 SIGHUP 訊號的處理方式設置為 SIG_IGN（忽略）。
* 為什麼要忽略它？ 因為我們的程式現在是一個守護進程，它不應該因為終端關閉而受到影響。忽略此訊號可以提高其穩定性。

4. 第二次 fork()
```cpp
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS); // 再次退出父進程，確保程式沒有控制終端
    }
```
* 為什麼需要第二次 fork()？ 這是守護進程化的核心安全步驟。

    - 防止重新取得控制終端：如果第一次 fork() 後的子進程（即會話首領）打開了一個新的終端設備（例如 /dev/ttyS0），那麼它可能會再次被該終端控制。
    - 非會話首領不能取得控制終端：第二次 fork() 後，新的子進程不再是會話首領（因為會話首領是第一次 fork() 後的子進程，它現在是第二次 fork() 的父進程，並立即退出了）。根據 Unix 規則，只有會話首領才能取得控制終端。因此，第二次 fork() 創建的子進程，就永遠不可能再取得控制終端了，這確保了它的獨立性。

5. 重設檔案權限遮罩 umask(0)

6. 更改工作目錄 chdir("/")

7. 關閉標準檔案描述符

### 總結 daemonize() 函數的流程圖
![圖片](https://hackmd.io/_uploads/HJufVnH7el.png)

---

#### 以此程式為基礎，要再加入 新增一GPIO是按鈕按3秒可以關機
```cpp
#include <iostream>
#include <gpiod.h>      // libgpiod 庫
#include <unistd.h>     // for sleep functions (usleep)
#include <signal.h>     // for signal handling (SIGTERM, SIGINT)
#include <syslog.h>     // for logging to syslog
#include <thread>       // for std::this_thread::sleep_for
#include <chrono>       // for std::chrono::milliseconds
#include <sys/stat.h>   // for umask()
#include <string>       // for std::string
#include <cstdlib>      // for system()

// --- 配置參數 ---
// GPIO Chip Name (通常 Raspberry Pi 4 是 gpiocip0)
const char *GPIO_CHIP_NAME = "gpiochip0";

// LED GPIO Line (BCM 編號為 17)
const unsigned int LED_GPIO_LINE = 17;

// 按鈕 GPIO Line (BCM 編號為 18，請根據你的連接調整)
const unsigned int BUTTON_GPIO_LINE = 18;

// 按鈕按下觸發關機所需的持續時間 (單位: 毫秒)
const std::chrono::milliseconds BUTTON_PRESS_DURATION_MS(3000); // 3 秒

// 運行中閃爍頻率 (單位: 毫秒)
const std::chrono::milliseconds RUNNING_BLINK_ON_MS(500);
const std::chrono::milliseconds RUNNING_BLINK_OFF_MS(500);

// 關機閃爍頻率 (單位: 毫秒)
const std::chrono::milliseconds SHUTDOWN_BLINK_ON_MS(100);
const std::chrono::milliseconds SHUTDOWN_BLINK_OFF_MS(100);
const int SHUTDOWN_BLINK_COUNT = 3;

// 按鈕防抖動延遲 (單位: 毫秒)
const std::chrono::milliseconds DEBOUNCE_DELAY_MS(50);

// 主循環每次迭代的延遲 (單位: 毫秒)
const std::chrono::milliseconds MAIN_LOOP_DELAY_MS(50);

// --- 全域變數 ---
struct gpiod_chip *chip = nullptr;
struct gpiod_line *led_line = nullptr;
struct gpiod_line *button_line = nullptr;
volatile sig_atomic_t running = 1; // 控制主循環運行
volatile bool shutdown_initiated = false; // 標記是否已觸發關機

// --- 函式宣告 ---
void cleanup_gpio();
void signal_handler(int signum);
void shutdown_sequence();
void daemonize();
void execute_shutdown();

int main() {
    // 1. 設定訊號處理器
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    // 2. 轉換為守護進程 (Daemon)
    daemonize();

    // 初始化 syslog，用於記錄日誌
    openlog("led_status_daemon", LOG_PID|LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "LED status daemon starting with button support.");

    // 3. 初始化 GPIO
    chip = gpiod_chip_open_by_name(GPIO_CHIP_NAME);
    if (!chip) {
        syslog(LOG_ERR, "Failed to open GPIO chip: %s", GPIO_CHIP_NAME);
        closelog();
        return 1;
    }

    // 初始化 LED GPIO
    led_line = gpiod_chip_get_line(chip, LED_GPIO_LINE);
    if (!led_line) {
        syslog(LOG_ERR, "Failed to get LED GPIO line: %d", LED_GPIO_LINE);
        gpiod_chip_close(chip);
        closelog();
        return 1;
    }
    int ret = gpiod_line_request_output(led_line, "led_status_daemon_led", 0);
    if (ret < 0) {
        syslog(LOG_ERR, "Failed to request LED GPIO line as output.");
        gpiod_line_release(led_line);
        gpiod_chip_close(chip);
        closelog();
        return 1;
    }
    syslog(LOG_INFO, "LED GPIO %d initialized successfully.", LED_GPIO_LINE);

    // 初始化按鈕 GPIO
    button_line = gpiod_chip_get_line(chip, BUTTON_GPIO_LINE);
    if (!button_line) {
        syslog(LOG_ERR, "Failed to get Button GPIO line: %d", BUTTON_GPIO_LINE);
        // 如果按鈕初始化失敗，仍然嘗試運行 LED 部分
        gpiod_line_release(led_line); // 釋放已請求的 LED line
        gpiod_chip_close(chip);
        closelog();
        return 1;
    }
    // 請求 GPIO 線為輸入模式，並啟用內部上拉電阻
    // "led_status_daemon_button" 作為消費者名稱
    ret = gpiod_line_request_input_flags(button_line, "led_status_daemon_button", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);
    if (ret < 0) {
        syslog(LOG_ERR, "Failed to request Button GPIO line as input with pull-up.");
        gpiod_line_release(button_line); // 釋放已請求的 button line
        gpiod_line_release(led_line);   // 釋放已請求的 LED line
        gpiod_chip_close(chip);
        closelog();
        return 1;
    }
    syslog(LOG_INFO, "Button GPIO %d initialized successfully with pull-up.", BUTTON_GPIO_LINE);

    // 4. 主循環：監控按鈕和閃爍 LED
    auto button_press_start_time = std::chrono::steady_clock::now();
    bool button_was_pressed = false;

    while (running) {
        // LED 閃爍邏輯 (每輪迴閃爍一次，或只是保持 LED 狀態)
        static bool led_state = false; // 記錄 LED 當前狀態
        if (led_state) {
            gpiod_line_set_value(led_line, 0); // 滅
            led_state = false;
        } else {
            gpiod_line_set_value(led_line, 1); // 亮
            led_state = true;
        }

        // 讀取按鈕狀態 (通常按鈕接到 GND，所以按下時是 LOW 電平)
        int button_value = gpiod_line_get_value(button_line);

        if (button_value == 0) { // 按鈕被按下 (LOW 電平)
            if (!button_was_pressed) {
                // 第一次檢測到按下
                button_press_start_time = std::chrono::steady_clock::now();
                button_was_pressed = true;
                syslog(LOG_INFO, "Button %d pressed.", BUTTON_GPIO_LINE);
            } else {
                // 按鈕持續按下
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - button_press_start_time);

                if (elapsed >= BUTTON_PRESS_DURATION_MS && !shutdown_initiated) {
                    syslog(LOG_WARNING, "Button %d held for %lld ms. Initiating shutdown!",
                           BUTTON_GPIO_LINE, elapsed.count());
                    shutdown_initiated = true; // 設置關機標記
                    running = 0; // 停止主循環，觸發關機序列
                }
            }
        } else { // 按鈕沒有按下 (HIGH 電平)
            if (button_was_pressed) {
                // 按鈕鬆開了
                button_was_pressed = false;
                syslog(LOG_INFO, "Button %d released.", BUTTON_GPIO_LINE);
            }
        }

        // 主循環延遲，控制 LED 閃爍速度和按鈕檢測頻率
        // 確保總的閃爍週期符合預期
        std::this_thread::sleep_for(RUNNING_BLINK_ON_MS + RUNNING_BLINK_OFF_MS); // 或者直接使用 MAIN_LOOP_DELAY_MS
        // 在這裡使用 MAIN_LOOP_DELAY_MS 更合適，它會頻繁檢查按鈕，並控制 LED 的間隔
        // std::this_thread::sleep_for(MAIN_LOOP_DELAY_MS);
    }

    // 5. 執行關機序列 (如果不是從按鈕觸發的，這裡也會執行)
    syslog(LOG_INFO, "Received stop signal or shutdown initiated. Performing shutdown sequence.");
    shutdown_sequence();

    // 6. 如果是通過按鈕觸發的關機，則執行系統關機命令
    if (shutdown_initiated) {
        execute_shutdown();
    }

    // 7. 清理 GPIO 資源
    cleanup_gpio();
    syslog(LOG_INFO, "LED status daemon stopped.");
    closelog();

    return 0;
}

// 清理 GPIO 資源
void cleanup_gpio() {
    if (led_line) {
        gpiod_line_set_value(led_line, 0); // 確保 LED 熄滅
        gpiod_line_release(led_line); // 釋放 GPIO 線
    }
    if (button_line) {
        gpiod_line_release(button_line); // 釋放 GPIO 線
    }
    if (chip) {
        gpiod_chip_close(chip); // 關閉 GPIO chip
    }
}

// 訊號處理器
void signal_handler(int signum) {
    syslog(LOG_INFO, "Received signal %d. Setting running flag to false.", signum);
    running = 0; // 設置 running 旗標為 0，通知主循環停止
}

// 關機序列：快速閃爍 3 次後熄滅
void shutdown_sequence() {
    for (int i = 0; i < SHUTDOWN_BLINK_COUNT; ++i) {
        gpiod_line_set_value(led_line, 1); // LED 亮
        std::this_thread::sleep_for(SHUTDOWN_BLINK_ON_MS);
        gpiod_line_set_value(led_line, 0); // LED 滅
        std::this_thread::sleep_for(SHUTDOWN_BLINK_OFF_MS);
    }
    gpiod_line_set_value(led_line, 0); // 確保 LED 最終熄滅
}

// 執行系統關機命令
void execute_shutdown() {
    syslog(LOG_WARNING, "Executing system shutdown command: 'sudo shutdown -h now'");
    // 使用 system() 執行 shell 命令，需要 root 權限
    // 這裡使用 "sudo" 是為了確保命令能被執行，即使 daemon user 不是 root
    // 但因為 service unit 中設定 User=root，所以直接 "shutdown -h now" 也可以
    system("sudo shutdown -h now");
}

// 將程式轉換為守護進程 (與之前相同)
void daemonize() {
    pid_t pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}
```

更新 Systemd 服務 (可選，如果你想用新名稱)

如果你想用新的可執行檔名稱，需要更新 Systemd 服務。

1. 移動可執行檔：
```bash
sudo mv led_status_daemon_button /usr/local/bin/
```
2. 更新 Systemd 服務檔案：

如果這是你第一次設置服務，內容如下：
```bash
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
SyslogIdentifier=led_status_daemon_button

[Install]
WantedBy=multi-user.target
```
3. 重新載入和啟動服務：
```bash
sudo systemctl daemon-reload
sudo systemctl enable led_status.service # 如果服務名稱沒變
sudo systemctl restart led_status.service
```
4. 檢查服務狀態和日誌：
```bash
sudo systemctl status led_status.service
journalctl -u led_status.service -f
```

5. 測試開機與關機
    - 開機測試：重啟你的 Raspberry Pi (sudo reboot)。在系統啟動過程中，你會看到 LED 以約 0.5 秒的頻率閃爍。
    - 按鈕關機測試：在系統正常運行時，按住你連接的按鈕，持續 3 秒。

    - 在按住期間，日誌會顯示按鈕被按下的訊息。
    - 3 秒後，日誌會顯示觸發關機的訊息。
    - LED 會執行 3 次快速閃爍然後熄滅。
    - 系統將開始關機。 
