# CHANGELOG - LED Status Daemon

All notable changes to this project will be documented in this file.

## [1.0.0] - 2026-05-07

### Added
- 命令列參數支援 (`-l`, `-s`, `-r`, `-S`, `-R`, `-c`)
- 環境變數支援 (`LED_GPIO`, `BUTTON_SHUTDOWN`, `BUTTON_REBOOT`, `SHUTDOWN_HOLD_MS`, `REBOOT_HOLD_MS`)
- 設定檔優先順序：CLI args > 環境變數 > 預設值

### Changed
- 程式重構：提取 init_gpio()、parse_config() 函式，簡化 main()
- systemd service 改用 Type=simple（移除 daemonize）
- 修正關機/重開機序列邏輯錯誤

### Fixed
- 修正 Type=forking 導致 systemd 誤判 exit-code=1 問題
- 修正關機按鈕也執行 reboot_sequence() 的 bug

### Documentation
- README.md 新增命令列用法說明
- README.md 更新 systemd service 設定範例
- README.md 新增問答集說明環境變數配置

---

## [0.0.1] - Initial Release (before refactoring)

- LED 閃爍功能（GPIO 17）
- 關機按鈕（GPIO 18，按住 10 秒）
- 重開機按鈕（GPIO 26，按住 3 秒）
- systemd service 支援（Type=forking）