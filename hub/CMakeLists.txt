cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hub)

target_sources(app PRIVATE
  src/main.cpp
  src/utilities.cpp
  src/network.cpp
  src/token_settings.c
  src/ble.cpp
  src/serial.cpp
  src/alarm.c
  src/location.cpp
  src/network_requests.cpp
  src/conf.cpp
  src/battery.cpp
  src/diagnostic.cpp
)
