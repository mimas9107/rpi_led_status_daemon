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
const std::chrono::milliseconds RUNNING_BLINK_ON_MS(1000);
const std::chrono::milliseconds RUNNING_BLINK_OFF_MS(1000);

// 關機閃爍頻率 (單位: 毫秒)
const std::chrono::milliseconds SHUTDOWN_BLINK_ON_MS(100);
const std::chrono::milliseconds SHUTDOWN_BLINK_OFF_MS(100);
const int SHUTDOWN_BLINK_COUNT = 10;

// 按鈕防抖動延遲 (單位: 毫秒)
const std::chrono::milliseconds DEBOUNCE_DELAY_MS(80);

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
