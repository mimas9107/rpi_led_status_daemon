---
name:          "AGENTS.md"
description:   "樹莓派 GPIO 狀態燈守護程序開發規範"
created_date:  "2026/05/29 13:25:00"
modified_date: "2026/06/18 10:45:00"
project_version: "1.0.0"
document_version: "1.1.0"
agent_sign: ['human/mimas', 'gemini cli/gemini-cli']
---

# RPi LED Status Daemon (AGENTS.md)

本文件定義此專案的特化開發行為。Agent 必須同時遵循工作區全域規範 (../AGENTS.md)。

## 1. 開發慣例
- **語言**: C++17，使用 gpiod 函式庫。
- **守護模式**: 遵循標準 daemonize() 模式，使用 syslog 進行日誌記錄。
- **GPIO**: LED (GPIO 17), Shutdown (GPIO 18, 10s press), Reboot (GPIO 26, 3s press)。

---
*註：本文件專注於專案業務與技術細節，通用環境指令與 Token 節約準則請查閱全域規範。*
