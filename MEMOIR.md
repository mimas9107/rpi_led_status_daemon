---
name:          "MEMOIR.md"
description:   "Project evolution and architectural decisions"
created_date:  "2026/06/18 10:00:00"
modified_date: "2026/06/18 10:00:00"
project_version: "1.0.0"
document_version: "1.0.0"
agent_sign: ['gemini cli/current_agent']
---
# Project Memoir: RPi LED Status Daemon

## Architectural Evolution

### Phase 1: Initial Prototype (v0.0.1)
- Basic implementation focused on GPIO control.
- Used `Type=forking` in systemd, which caused some instability and incorrect exit codes.
- Hardcoded GPIO values.

### Phase 2: Refactoring and Standardization (v1.0.0)
- **Signal Handling**: Moved to a cleaner `volatile sig_atomic_t` approach for thread-safe signal detection.
- **Configuration**: Implemented a layered configuration system supporting both CLI flags and environment variables.
- **Systemd Optimization**: Switched to `Type=simple`. This simplified the service definition and improved reliability within the systemd lifecycle.
- **Code Structure**: Extracted core logic into `init_gpio()` and `parse_config()` to improve readability and testability.

## Key Decisions
- **libgpiod vs RPi.GPIO**: Chose `libgpiod` for better performance and future compatibility with the Linux kernel's character device GPIO interface, avoiding the deprecated `/sys/class/gpio`.
- **C++17**: Selected for modern filesystem support and cleaner syntax.
