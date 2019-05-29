cmake_minimum_required(VERSION 2.8)
project(innerproc)
SET(CMAKE_SYSTEM_NAME Linux)

set(CMAKE_CXX_STANDARD 11)
link_directories( /usr/local/lib /home/linaro4.9.4/arm-linux-gnueabi/libc/lib)  # amr-linux 编译需要
add_definitions(-DMACOS -D_MACOS   )
include_directories(/usr/local/include /usr/local/Cellar/boost/1.60.0_2/include)
link_libraries( boost_system  jsoncpp pthread serialport)
set(SRC_1 utils.h http-api.cpp version.h sensor_message.cpp sensor_message.h
        crc16.c
        event.cpp event.h )
add_executable(innerbox2
        http.cpp mongoose.c connection.cpp inner_device_mgr.cpp message.cpp
        call_manager.cpp device.cpp app.cpp InnerController.cpp
        config.cpp watchdog.cpp sensor.cpp call.cpp
        seczone.cpp
        error.cpp
        error.h server.h server.cpp
        md5.c md5.hpp base64.h utils.cpp
        ${SRC_1}
        )
