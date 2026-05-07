#include <iostream>
#include <gpiod.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <thread>
#include <chrono>
#include <sys/stat.h>
#include <string>
#include <cstdlib>
#include <getopt.h>

// ============================================================
// 1. 預設配置（当没有 CLI args 或环境变数时使用）
// ============================================================

const char *DEFAULT_GPIO_CHIP = "gpiochip0";
const unsigned int DEFAULT_LED_GPIO = 17;
const unsigned int DEFAULT_BUTTON_SHUTDOWN = 18;
const unsigned int DEFAULT_BUTTON_REBOOT = 26;

// 按钮长按触发时间（毫秒）
const int DEFAULT_SHUTDOWN_HOLD_MS = 10000;
const int DEFAULT_REBOOT_HOLD_MS = 3000;

// LED 闪烁时间（毫秒）
const int DEFAULT_LED_BLINK_ON_MS = 1000;
const int DEFAULT_LED_BLINK_OFF_MS = 1000;

// 关机/重启闪烁
const int DEFAULT_SHUTDOWN_BLINK_ON_MS = 100;
const int DEFAULT_SHUTDOWN_BLINK_OFF_MS = 100;
const int DEFAULT_SHUTDOWN_BLINK_COUNT = 10;
const int DEFAULT_REBOOT_BLINK_COUNT = 3;

// 防抖动延迟
const int DEFAULT_DEBOUNCE_MS = 80;

// ============================================================
// 2. 执行期配置（由 config_parser 填充）
// ============================================================

struct gpio_config {
    const char *chip_name;
    unsigned int led_gpio;
    unsigned int button_shutdown_gpio;
    unsigned int button_reboot_gpio;
    int shutdown_hold_ms;
    int reboot_hold_ms;
    int led_blink_on_ms;
    int led_blink_off_ms;
    int shutdown_blink_on_ms;
    int shutdown_blink_off_ms;
    int shutdown_blink_count;
    int reboot_blink_count;
    int debounce_ms;
};

gpio_config g_config = {
    DEFAULT_GPIO_CHIP,
    DEFAULT_LED_GPIO,
    DEFAULT_BUTTON_SHUTDOWN,
    DEFAULT_BUTTON_REBOOT,
    DEFAULT_SHUTDOWN_HOLD_MS,
    DEFAULT_REBOOT_HOLD_MS,
    DEFAULT_LED_BLINK_ON_MS,
    DEFAULT_LED_BLINK_OFF_MS,
    DEFAULT_SHUTDOWN_BLINK_ON_MS,
    DEFAULT_SHUTDOWN_BLINK_OFF_MS,
    DEFAULT_SHUTDOWN_BLINK_COUNT,
    DEFAULT_REBOOT_BLINK_COUNT,
    DEFAULT_DEBOUNCE_MS
};

// ============================================================
// 3. 全域变数
// ============================================================

struct gpiod_chip *chip = nullptr;
struct gpiod_line *led_line = nullptr;
struct gpiod_line *button_line = nullptr;
struct gpiod_line *button_reboot = nullptr;

volatile sig_atomic_t running = 1;
volatile bool shutdown_initiated = false;
volatile bool reboot_initiated = false;

// ============================================================
// 4. 函式宣告
// ============================================================

void print_usage(const char *prog_name);
void parse_config(int argc, char *argv[]);
int init_gpio();
void cleanup_gpio();
void signal_handler(int signum);
void shutdown_sequence();
void reboot_sequence();
void daemonize();
void execute_shutdown();
void execute_reboot();

int main(int argc, char *argv[]) {
    // 1. 解析命令列引数与环境变数
    parse_config(argc, argv);

    // 2. 设定信号处理器（先于 daemonize，确保能捕获信号）
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    // 3. 如果需要守护进程模式则调用，否则直接运行
    // 注意：Type=simple 时不需要 daemonize()
    // daemonize();  // <-- 在 Type=simple 下不需要，注释掉

    // 4. 初始化 syslog
    openlog("led_status_daemon", LOG_PID|LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "LED status daemon starting with button support.");
    syslog(LOG_INFO, "Config: LED=%d, ShutdownBtn=%d, RebootBtn=%d",
          g_config.led_gpio, g_config.button_shutdown_gpio, g_config.button_reboot_gpio);

    // 5. 初始化 GPIO
    if (init_gpio() != 0) {
        return 1;
    }

    // 6. 主循环：监控按钮和闪烁 LED
    auto shutdown_press_start = std::chrono::steady_clock::now();
    auto reboot_press_start = std::chrono::steady_clock::now();
    bool shutdown_button_pressed = false;
    bool reboot_button_pressed = false;
    bool led_state = false;

    while (running) {
        // LED 闪烁
        led_state = !led_state;
        gpiod_line_set_value(led_line, led_state ? 1 : 0);

        // 读取按钮状态（ LOW = 按下）
        int shutdown_val = gpiod_line_get_value(button_line);
        int reboot_val = g_config.button_reboot_gpio > 0 ? gpiod_line_get_value(button_reboot) : 1;

        // 处理关机按钮
        if (shutdown_val == 0) {
            if (!shutdown_button_pressed) {
                shutdown_press_start = std::chrono::steady_clock::now();
                shutdown_button_pressed = true;
                syslog(LOG_INFO, "Shutdown button pressed.");
            } else {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - shutdown_press_start);
                if (elapsed.count() >= g_config.shutdown_hold_ms && !shutdown_initiated) {
                    syslog(LOG_WARNING, "Shutdown button held %ld ms. Initiating shutdown!",
                          elapsed.count());
                    shutdown_initiated = true;
                    running = 0;
                }
            }
        } else {
            if (shutdown_button_pressed) {
                shutdown_button_pressed = false;
                syslog(LOG_INFO, "Shutdown button released.");
            }
        }

        // 处理重启按钮
        if (g_config.button_reboot_gpio > 0 && reboot_val == 0) {
            if (!reboot_button_pressed) {
                reboot_press_start = std::chrono::steady_clock::now();
                reboot_button_pressed = true;
                syslog(LOG_INFO, "Reboot button pressed.");
            } else {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - reboot_press_start);
                if (elapsed.count() >= g_config.reboot_hold_ms && !reboot_initiated) {
                    syslog(LOG_WARNING, "Reboot button held %ld ms. Initiating reboot!",
                          elapsed.count());
                    reboot_initiated = true;
                    running = 0;
                }
            }
        } else {
            if (reboot_button_pressed) {
                reboot_button_pressed = false;
                syslog(LOG_INFO, "Reboot button released.");
            }
        }

        // 延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(
            g_config.led_blink_on_ms + g_config.led_blink_off_ms));
    }

    // 7. 执行关机或重启序列
    if (shutdown_initiated) {
        syslog(LOG_INFO, "Initiating shutdown sequence.");
        shutdown_sequence();
        execute_shutdown();
    } else if (reboot_initiated) {
        syslog(LOG_INFO, "Initiating reboot sequence.");
        reboot_sequence();
        execute_reboot();
    } else {
        // 收到 SIGTERM，正常關閉
        syslog(LOG_INFO, "Performing graceful shutdown sequence.");
        shutdown_sequence();
    }

    // 9. 清理 GPIO 资源
    cleanup_gpio();
    syslog(LOG_INFO, "LED status daemon stopped.");
    closelog();

    return 0;
}

// ============================================================
// 5. config_parser：解析 CLI args + 环境变数 + 预设值
// ============================================================

void print_usage(const char *prog_name) {
    std::printf("用法: %s [选项]\n", prog_name);
    std::printf("  -l, --led-gpio <num>       LED GPIO 编号 (默认: %d)\n", DEFAULT_LED_GPIO);
    std::printf("  -s, --shutdown-btn <num>     关机按钮 GPIO 编号 (默认: %d)\n", DEFAULT_BUTTON_SHUTDOWN);
    std::printf("  -r, --reboot-btn <num>    重启按钮 GPIO 编号 (默认: %d)\n", DEFAULT_BUTTON_REBOOT);
    std::printf("  -S, --shutdown-hold <ms> 关机按钮长按时间 (默认: %d ms)\n", DEFAULT_SHUTDOWN_HOLD_MS);
    std::printf("  -R, --reboot-hold <ms>    重启按钮长按时间 (默认: %d ms)\n", DEFAULT_REBOOT_HOLD_MS);
    std::printf("  -c, --chip <name>         GPIO chip 名称 (默认: %s)\n", DEFAULT_GPIO_CHIP);
    std::printf("  -h, --help                显示此帮助信息\n");
}

void parse_config(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"led-gpio", required_argument, 0, 'l'},
        {"shutdown-btn", required_argument, 0, 's'},
        {"reboot-btn", required_argument, 0, 'r'},
        {"shutdown-hold", required_argument, 0, 'S'},
        {"reboot-hold", required_argument, 0, 'R'},
        {"chip", required_argument, 0, 'c'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "l:s:r:S:R:c:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'l': g_config.led_gpio = std::stoi(optarg); break;
            case 's': g_config.button_shutdown_gpio = std::stoi(optarg); break;
            case 'r': g_config.button_reboot_gpio = std::stoi(optarg); break;
            case 'S': g_config.shutdown_hold_ms = std::stoi(optarg); break;
            case 'R': g_config.reboot_hold_ms = std::stoi(optarg); break;
            case 'c': g_config.chip_name = optarg; break;
            case 'h': print_usage(argv[0]); exit(0);
            default: print_usage(argv[0]); exit(1);
        }
    }

    // 环境变量优先于预设值（CLI 参数已在前面覆盖环境变量）
    const char *env;
    if ((env = std::getenv("LED_GPIO")) != nullptr) g_config.led_gpio = std::stoi(env);
    if ((env = std::getenv("BUTTON_SHUTDOWN")) != nullptr) g_config.button_shutdown_gpio = std::stoi(env);
    if ((env = std::getenv("BUTTON_REBOOT")) != nullptr) g_config.button_reboot_gpio = std::stoi(env);
    if ((env = std::getenv("SHUTDOWN_HOLD_MS")) != nullptr) g_config.shutdown_hold_ms = std::stoi(env);
    if ((env = std::getenv("REBOOT_HOLD_MS")) != nullptr) g_config.reboot_hold_ms = std::stoi(env);
    if ((env = std::getenv("GPIO_CHIP")) != nullptr) g_config.chip_name = env;
}

// ============================================================
// 6. init_gpio：GPIO 初始化
// ============================================================

int init_gpio() {
    chip = gpiod_chip_open_by_name(g_config.chip_name);
    if (!chip) {
        syslog(LOG_ERR, "Failed to open GPIO chip: %s", g_config.chip_name);
        closelog();
        return -1;
    }

    // LED GPIO
    led_line = gpiod_chip_get_line(chip, g_config.led_gpio);
    if (!led_line) {
        syslog(LOG_ERR, "Failed to get LED GPIO line: %d", g_config.led_gpio);
        gpiod_chip_close(chip);
        closelog();
        return -1;
    }
    if (gpiod_line_request_output(led_line, "led_status_daemon_led", 0) < 0) {
        syslog(LOG_ERR, "Failed to request LED GPIO line as output.");
        gpiod_line_release(led_line);
        gpiod_chip_close(chip);
        closelog();
        return -1;
    }
    syslog(LOG_INFO, "LED GPIO %d initialized.", g_config.led_gpio);

    // 关机按钮 GPIO
    button_line = gpiod_chip_get_line(chip, g_config.button_shutdown_gpio);
    if (!button_line) {
        syslog(LOG_ERR, "Failed to get shutdown button GPIO: %d", g_config.button_shutdown_gpio);
        gpiod_line_release(led_line);
        gpiod_chip_close(chip);
        closelog();
        return -1;
    }
    if (gpiod_line_request_input_flags(button_line, "led_status_daemon_button",
                                        GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP) < 0) {
        syslog(LOG_ERR, "Failed to request shutdown button as input.");
        gpiod_line_release(button_line);
        gpiod_line_release(led_line);
        gpiod_chip_close(chip);
        closelog();
        return -1;
    }
    syslog(LOG_INFO, "Shutdown button GPIO %d initialized.", g_config.button_shutdown_gpio);

    // 重启按钮 GPIO（可选）
    if (g_config.button_reboot_gpio > 0) {
        button_reboot = gpiod_chip_get_line(chip, g_config.button_reboot_gpio);
        if (!button_reboot) {
            syslog(LOG_ERR, "Failed to get reboot button GPIO: %d", g_config.button_reboot_gpio);
            gpiod_line_release(button_line);
            gpiod_line_release(led_line);
            gpiod_chip_close(chip);
            closelog();
            return -1;
        }
        if (gpiod_line_request_input_flags(button_reboot, "led_status_daemon_reboot",
                                        GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP) < 0) {
            syslog(LOG_ERR, "Failed to request reboot button as input.");
            gpiod_line_release(button_reboot);
            gpiod_line_release(button_line);
            gpiod_line_release(led_line);
            gpiod_chip_close(chip);
            closelog();
            return -1;
        }
        syslog(LOG_INFO, "Reboot button GPIO %d initialized.", g_config.button_reboot_gpio);
    }

    return 0;
}

// ============================================================
// 7-12. 保留函数（稍作整理）
// ============================================================

void cleanup_gpio() {
    if (led_line) {
        gpiod_line_set_value(led_line, 0);
        gpiod_line_release(led_line);
    }
    if (button_line) {
        gpiod_line_release(button_line);
    }
    if (button_reboot) {
        gpiod_line_release(button_reboot);
    }
    if (chip) {
        gpiod_chip_close(chip);
    }
}

void signal_handler(int signum) {
    syslog(LOG_INFO, "Received signal %d. Setting running flag to false.", signum);
    running = 0;
}

void shutdown_sequence() {
    for (int i = 0; i < g_config.shutdown_blink_count; ++i) {
        gpiod_line_set_value(led_line, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(g_config.shutdown_blink_on_ms));
        gpiod_line_set_value(led_line, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(g_config.shutdown_blink_off_ms));
    }
    gpiod_line_set_value(led_line, 0);
}

void reboot_sequence() {
    for (int i = 0; i < g_config.reboot_blink_count; ++i) {
        gpiod_line_set_value(led_line, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(g_config.shutdown_blink_on_ms));
        gpiod_line_set_value(led_line, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(g_config.shutdown_blink_off_ms));
    }
    gpiod_line_set_value(led_line, 0);
}

void execute_shutdown() {
    syslog(LOG_WARNING, "Executing system shutdown: 'sudo shutdown -h now'");
    system("sudo shutdown -h now");
}

void execute_reboot() {
    syslog(LOG_WARNING, "Executing system reboot: 'sudo reboot'");
    system("sudo reboot");
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);
    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}