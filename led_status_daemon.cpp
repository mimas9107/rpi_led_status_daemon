#include <iostream>
#include <gpiod.h> // libgpiod 庫
#include <unistd.h> // for sleep functions (usleep)
#include <signal.h> // for signal handling (SIGTERM, SIGINT)
#include <syslog.h> // for logging to syslog
#include <thread>   // for std::this_thread::sleep_for
#include <chrono>   // for std::chrono::milliseconds
#include <sys/stat.h>
// --- 配置參數 ---
// 根據你的 Raspberry Pi 型號和連接方式選擇正確的 GPIO chip 和 line number。
// 通常 Raspberry Pi 4 的 GPIO 是 gpiocip0。
// 請查閱你的硬體資料以確定正確的 GPIO BCM 編號。
// 例如，BCM 17 通常對應到 gpiocip0 的 line 17。
const char *GPIO_CHIP_NAME = "gpiochip0";
const unsigned int LED_GPIO_LINE = 17; // 例如，BCM 編號為 17 的 GPIO

// 運行中閃爍頻率 (單位: 毫秒)
const std::chrono::milliseconds RUNNING_BLINK_ON_MS(1000);
const std::chrono::milliseconds RUNNING_BLINK_OFF_MS(1000);

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
