cmake_minimum_required(VERSION 3.26)
set(PROGRAM_NAME pico_w_webserver)

message("PICO_SDK_PATH: $ENV{PICO_SDK_PATH}")
message("WIFI_SSID=${WIFI_SSID}")
message("CMAKE_CURRENT_LIST_DIR ${CMAKE_CURRENT_LIST_DIR}")

add_executable(${PROGRAM_NAME}
        main.c
        httpd_ws.c
        fs.c
        )


target_include_directories(${PROGRAM_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        $ENV{PICO_SDK_PATH}/lib/lwip/src/apps/http
        $ENV{PICO_SDK_PATH}/lib/lwip/src/include
        $ENV{PICO_SDK_PATH}/src/rp2_common/pico_cyw43_arch/include
        $ENV{PICO_SDK_PATH}/lib/cyw43-driver/src
        $ENV{PICO_SDK_PATH}/lib/lwip/src/include
        $ENV{PICO_SDK_PATH}/lib/lwip/src/include/lwip/apps
        $ENV{PICO_SDK_PATH}/lib/mbedtls/include
        $ENV{PICO_SDK_PATH}/lib/lwip/contrib/ports/unix/port/include
        $ENV{PICO_SDK_PATH}/lib/lwip/port
        $ENV{PICO_SDK_PATH}/src/rp2_common/hardware_rtc/include
        )
target_link_libraries(${PROGRAM_NAME}
        cyw43_driver
        pico_cyw43_arch_lwip_threadsafe_background
        #pico_lwip_http
        pico_stdlib
        #hardware_adc
        pico_mbedtls
        pico_lwip_mbedtls
        hardware_rtc
        )

pico_enable_stdio_usb(${PROGRAM_NAME} TRUE)
pico_enable_stdio_uart(${PROGRAM_NAME} TRUE)
suppress_tinyusb_warnings()

pico_add_extra_outputs(${PROGRAM_NAME})
