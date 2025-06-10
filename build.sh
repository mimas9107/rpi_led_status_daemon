#!/usr/bin/bash
if [ ! -d output ]; then
	mkdir -p output
fi
echo "開機 LED閃爍+按鈕 5秒關機"
g++ led_status_daemon_button.cpp -o output/led_status_daemon_button -lgpiod -lstdc++fs -Wall -std=c++17
echo "開機 LED閃爍"
g++ led_status_daemon.cpp -o output/led_status_daemon -lgpiod -lstdc++fs -Wall -std=c++17
