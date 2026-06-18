---
name:          "SPEC.md"
description:   "Technical specification for rpi-led-status-daemon"
created_date:  "2026/06/18 10:00:00"
modified_date: "2026/06/18 10:00:00"
project_version: "1.0.0"
document_version: "1.0.0"
agent_sign: ['gemini cli/current_agent']
---
# Technical Specification: RPi LED Status Daemon

## 1. Overview
This daemon provides a hardware-level status indication and control interface for Raspberry Pi systems using C++ and `libgpiod`. It manages an LED for heart-beat status and monitors physical buttons for system power management (shutdown/reboot).

## 2. Hardware Interface
- **Library**: `libgpiod` (v1.x/v2.x compatible).
- **Default GPIOs (BCM)**:
    - LED: 17 (Output)
    - Shutdown Button: 18 (Input, Pull-up)
    - Reboot Button: 26 (Input, Pull-up)

## 3. Functional Requirements
### 3.1 LED Behavior
- **Heartbeat**: 1 second interval (500ms ON, 500ms OFF).
- **Shutdown Sequence**: 10 fast blinks (100ms interval).
- **Reboot Sequence**: 3 fast blinks (200ms interval).

### 3.2 Button Logic
- **Shutdown**: Triggered if GPIO 18 is LOW for 10,000ms.
- **Reboot**: Triggered if GPIO 26 is LOW for 3,000ms.
- **Debounce**: 80ms software debounce.

## 4. Configuration
- **Priority**: CLI Arguments > Environment Variables > Defaults.
- **Environment Variables**:
    - `LED_GPIO`
    - `BUTTON_SHUTDOWN`, `BUTTON_REBOOT`
    - `SHUTDOWN_HOLD_MS`, `REBOOT_HOLD_MS`

## 5. System Integration
- **Logging**: `syslog`.
- **Process Management**: `systemd` (Type=simple).
- **Signals**: Graceful exit on `SIGTERM` and `SIGINT`.
