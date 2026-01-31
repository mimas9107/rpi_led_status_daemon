# AGENTS.md - Agent Guidelines for rpi_led_status_daemon

## Build Commands

### Full Build
```bash
./build.sh
```
This compiles the main daemon with button support and outputs to `output/led_status_daemon_button`.

### Manual Build (for alternative targets)
```bash
# Build with all warnings enabled, C++17 standard, and required libraries
g++ led_status_daemon_button.cpp -o output/led_status_daemon_button -lgpiod -lstdc++fs -Wall -std=c++17

# Build simpler LED-only version (currently commented in build.sh)
g++ led_status_daemon.cpp -o output/led_status_daemon -lgpiod -lstdc++fs -Wall -std=c++17
```

### Install
After building, copy the binary to system path:
```bash
sudo cp output/led_status_daemon_button /usr/local/bin/
```

## Testing

This codebase currently has **no automated test framework**. When adding tests, consider:
- Unit tests: Use GoogleTest framework for GPIO mocking and daemon logic
- Integration tests: Test with actual Raspberry Pi hardware
- Signal handling tests: Verify SIGTERM/SIGINT graceful shutdown
- Button press tests: Mock long-press detection logic

### Running Tests (when implemented)
```bash
# Run all tests
./output/tests

# Run specific test (when using GoogleTest)
./output/tests --gtest_filter=TestSuite.TestCaseName
```

## Code Style Guidelines

### C++ Standard
- Use **C++17** (`-std=c++17` compiler flag)
- Leverage modern C++ features where appropriate (std::chrono, std::thread)

### Imports and Includes
Order includes consistently:
```cpp
// 1. C standard library headers (unistd.h, signal.h, syslog.h, sys/stat.h)
// 2. C++ standard library headers (iostream, thread, chrono, string, cstdlib)
// 3. Third-party library headers (gpiod.h)
// 4. Local headers (if any)
```

Example:
```cpp
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <cstdlib>

#include <gpiod.h>
```

### Naming Conventions
- **Constants**: `SCREAMING_SNAKE_CASE` - e.g., `LED_GPIO_LINE`, `BUTTON_PRESS_DURATION_MS`
- **Variables**: `snake_case` - e.g., `led_line`, `button_value`, `running`
- **Functions**: `snake_case` - e.g., `cleanup_gpio()`, `signal_handler()`, `daemonize()`
- **Global variables**: `snake_case` - e.g., `chip`, `led_line` (prefer minimizing globals)
- **Types**: Use standard library types, prefer `std::chrono::milliseconds` over raw integers for durations

### GPIO Configuration
- Always check return values from libgpiod functions
- Release GPIO resources in cleanup_gpio() function
- Use descriptive consumer names: "led_status_daemon_led", "led_status_daemon_button"
- Request GPIO lines with appropriate flags (e.g., `GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP` for buttons)

### Daemon Pattern
Follow the established daemonize() pattern:
```cpp
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
```

### Signal Handling
- Use `volatile sig_atomic_t` for signal-safe flags (e.g., `running`)
- Keep signal handlers minimal - only set flags
- Handle SIGTERM, SIGINT, and SIGHUP appropriately
- Log signal reception with syslog before setting flags

### Logging
Use syslog for all logging:
```cpp
openlog("led_status_daemon", LOG_PID|LOG_CONS, LOG_DAEMON);
syslog(LOG_INFO, "LED status daemon starting.");
syslog(LOG_ERR, "Failed to open GPIO chip: %s", chip_name);
closelog();
```

- Log INFO for normal operation
- Log ERR for initialization failures
- Log WARNING for user-triggered actions (shutdown, reboot)
- Always call closelog() before exit

### Error Handling
- Check all libgpiod function return values
- Clean up resources on error (release lines, close chip, close log)
- Return non-zero on failure, zero on success
- Log errors before cleanup/exit

### Timing and Sleep
Use std::chrono for all timing:
```cpp
std::this_thread::sleep_for(std::chrono::milliseconds(500));
auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
```

### Comments and Documentation
- Comments may be in Traditional Chinese (existing codebase style)
- Use comments to explain GPIO pin assignments and BCM numbering
- Document button press durations and LED blink patterns
- Explain signal handling logic in signal handlers

### Memory Management
- Always null-check pointers before use (chip, led_line, button_line)
- Release resources in reverse order of acquisition
- Set pointers to nullptr after release (optional but good practice)

### Button Logic
- Implement debounce delay (currently 80ms) to prevent false triggers
- Track button state with boolean flags (button_was_pressed)
- Use steady_clock for accurate timing of long-press detection
- Log button press and release events

### Compiler Flags
Always compile with:
- `-std=c++17` - C++17 standard
- `-Wall` - Enable all warnings
- `-lgpiod` - Link libgpiod library
- `-lstdc++fs` - Link C++ filesystem library (if needed)

### File Structure
- Keep main daemon logic in led_status_daemon_button.cpp
- Simpler LED-only version in led_status_daemon.cpp
- Output binaries in `output/` directory
- Build script as `build.sh`

### Hardware Dependencies
- Raspberry Pi 4 (or compatible)
- GPIO chip: `gpiochip0`
- LED on GPIO 17 (BCM)
- Shutdown button on GPIO 18 (BCM), requires 10-second press
- Reboot button on GPIO 26 (BCM), requires 3-second press
- Buttons should connect to ground (active LOW, use internal pull-up)
