#include <string.h>
#include <stdlib.h>

#include "httpd_ws.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "hardware/structs/systick.h"
#include "lwip/init.h"
#include "hardware/rtc.h"
#include "hardware/structs/rtc.h"

#include "lwipopts.h"
#include "cgi.h"
#include "ssi.h"
#include "hardware/watchdog.h"

#define PLL_SYS_KHZ (260 * 1000)

void sendToWebSocket(); // defined in handle_ws_frames.c

void set_clock_khz(void)
{
    // set a system clock frequency in khz
    set_sys_clock_khz(PLL_SYS_KHZ, true);

    // configure the specified clock
    clock_configure(
        clk_peri,
        0,                                                // No glitchless mux
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
        PLL_SYS_KHZ * 1000,                               // Input frequency
        PLL_SYS_KHZ * 1000                                // Output (must be same as no divider)
    );
}

void run_server() {
    httpd_init();
    //lwip_init();
    //ssi_init();
    //cgi_init();
    printf("Http server initialized.\n");
    // infinite loop for now
    for (;;) {
        //sendToWebSocket();
        // Update the watchdog timer to prevent reboot
        watchdog_update();
    }
}

int main() {
    
    //char * WIFI_SSID = "SSID";
    //char * WIFI_PASSWORD = "mypassword";
    #include "credentials.c" // put your real credential in here. it has been blocked in .gitignore

    vreg_set_voltage(VREG_VOLTAGE_1_15); // For overclocking
    stdio_init_all();
    set_clock_khz();
    setup_default_uart(); // must do this after changing the clock
    

    // Give us time to monitor the terminal output via usb

    //sleep_ms(20000);
    
    // Start the watchdog tick with a divider of 12
    // This assumes a 12MHz XOSC input and produces a 1MHz clock
    watchdog_start_tick(12);

    // Enable the watchdog with a timeout of 5 seconds
    // This sets a marker in the watchdog scratch register 4
    watchdog_enable(35000, true);

    printf("\n\n============================================\nhello\n");

    uint32_t cpu_speed = clock_get_hz(clk_sys);
    printf("CPU Speed: %lu Hz\n", cpu_speed);

    printf("\n\nHello\n");
    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();
    // this seems to be the best be can do using the predefined `cyw43_pm_value` macro:
    // cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);
    // however it doesn't use the `CYW43_NO_POWERSAVE_MODE` value, so we do this instead:
    cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));

    

    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect.\n");
        watchdog_update();
        // use sleep_ms instead of best_effort_wfe_or_timeout
        sleep_ms(1000);
    } 
    
    printf("Connected.\n");

    extern cyw43_t cyw43_state;
    int ip_addr = cyw43_state.netif[CYW43_ITF_STA].ip_addr.addr;
    printf("IP Address: %hhu.%hhu.%hhu.%hhu\n", (unsigned char)(ip_addr & 0xFF), (unsigned char)((ip_addr >> 8) & 0xFF), (unsigned char)((ip_addr >> 16) & 0xFF), (unsigned char)(ip_addr >> 24));
    
    int32_t rssi;
    cyw43_ioctl(&cyw43_state, 254, sizeof rssi, (uint8_t *)&rssi, CYW43_ITF_STA);
    printf("Signal strength: %ld\n", rssi);
    
    // turn on LED to signal connected
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);


    run_server();
}

