cmake_minimum_required(VERSION 3.10)
project(innerproc)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-DMACOS)
#add_executable(innerproc main.cpp app.cpp logger.cpp logger.h codec.cpp config.cpp concurrent.h)


#include_directories
#link_directories
link_directories("/usr/local/Cellar/boost/1.66.0/lib" "/usr/local/lib")
link_libraries( boost_system jsoncpp)
#target_link_libraries(innerproc /usr/local/Cellar/boost/1.66.0/lib)
#target_link_libraries(innerproc /usr/local/Cellar/boost/1.66.0/lib)
#add_library()

#add_executable(innerproc main.cpp mongoose.c connection.cpp call.cpp service.cpp inner_device_mgr.cpp inner_device_mgr.h call_manager.cpp call_manager.h)
#add_executable(innerproc main.cpp sensor.cpp watchdog.cpp )
#add_executable(innerproc main.cpp http.cpp sensor.cpp watchdog.cpp  )
add_executable(innerproc main.cpp http.cpp mongoose.c connection.cpp)

