#!/usr/bin/bash
if [ ! -d output ]; then
	mkdir -p output
fi
echo "開機 LED閃爍+按鈕A 10秒關機+按鈕B 3秒重開機"
g++ led_status_daemon_button.cpp -o output/led_status_daemon_button -lgpiod -lstdc++fs -Wall -std=c++17

if [ $? = 0 ]; then
    echo "成功編譯, 請將 led_status_daemon_button 複製到 /usr/loca/bin/"
    echo "請將 按鈕A 安裝於 GPIO 18, 按鈕B 安裝於 GPIO 26. "
fi


#echo "開機 LED閃爍"
#g++ led_status_daemon.cpp -o output/led_status_daemon -lgpiod -lstdc++fs -Wall -std=c++17

