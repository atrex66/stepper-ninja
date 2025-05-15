#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "wizchip_conf.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "config.h"

char buffer[64];
int buffer_pos = 0;
extern stepgens;
extern wiz_NetInfo net_info;
extern uint32_t TIMEOUT_US;
extern configuration_t *flash_config;
extern uint16_t port;
extern void reset_with_watchdog();
extern uint8_t *src_ip;
extern uint16_t adc_min;
extern uint16_t adc_max;
extern uint8_t timeout_error;
extern uint32_t time_constant;
extern int32_t *position;

bool enable_serial = true;

void save_configuration(){
    flash_config->dhcp = net_info.dhcp;
    memcpy(flash_config->mac, &net_info.mac, 6);
    memcpy(flash_config->ip, &net_info.ip, 4);
    memcpy(flash_config->sn, &net_info.sn, 4);
    memcpy(flash_config->gw, &net_info.gw, 4);
    memcpy(flash_config->dns, &net_info.dns, 4);
    flash_config->port = port;
    flash_config->timeout = TIMEOUT_US;
    flash_config->time_constant = time_constant;
}

void load_configuration(){
    flash_config = (configuration_t *)malloc(sizeof(configuration_t));
    load_config_from_flash(flash_config);
    memcpy(net_info.mac, flash_config->mac, 6);
    memcpy(net_info.ip, flash_config->ip, 4);
    memcpy(net_info.sn, flash_config->sn, 4);
    memcpy(net_info.gw, flash_config->gw, 4);
    memcpy(net_info.dns, flash_config->dns, 4);
    net_info.dhcp = flash_config->dhcp;
    port = flash_config->port;
    TIMEOUT_US = flash_config->timeout;
    time_constant = flash_config->time_constant;
}

// Function: process_command
// Description: Processes commands received via serial input for configuring the io-samurai. 
// Supports commands to display help, check current configuration, set IP, port, MAC, timeout, 
// restore defaults, reset the device, and save settings to flash. 
// Parses commands with parameters (e.g., "ip x.x.x.x") using sscanf, validates formats, 
// updates settings, and saves changes. 
// Invalid commands or formats trigger error messages. 
// Clears the command buffer after processing.
void process_command(char* command) {
    if (strcmp(command, "help") == 0) {
        printf("Available commands:\n");
        printf("help - Show this help message\n");
        printf("check - Show current configuration\n");
        printf("ip <x.x.x.x> - Set IP address\n");
        printf("port <port> - Set port\n");
        printf("mac <xx:xx:xx:xx:xx:xx> - Set MAC address\n");
        printf("timeout <value> - Set timeout in microseconds\n");
        printf("defaults - Restore default configuration\n");
        printf("reset - Reset the device\n");
        printf("save - Save configuration to flash\n");
        printf("\n");
        command[0] = '\0'; // Clear the command buffer
        return;
    }
    else if (strcmp(command, "save") == 0) {
        save_configuration();
        save_config_to_flash();
    }
    else if (strcmp(command, "check") == 0) {
        printf("\n");
        printf("Current configuration:\n");
        printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", net_info.mac[0], net_info.mac[1], net_info.mac[2], net_info.mac[3], net_info.mac[4], net_info.mac[5]);
        printf("IP: %d.%d.%d.%d\n", net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3]);
        printf("Subnet: %d.%d.%d.%d\n", net_info.sn[0], net_info.sn[1], net_info.sn[2], net_info.sn[3]);
        printf("Gateway: %d.%d.%d.%d\n", net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3]);
        printf("DNS: %d.%d.%d.%d\n", net_info.dns[0], net_info.dns[1], net_info.dns[2], net_info.dns[3]);
        printf("DHCP: %d   (1-Static, 2-Dinamic)\n", net_info.dhcp);
        printf("PORT: %d\n", port);
        printf("*******************PHY status**************\n");
        uint8_t phyconf = getPHYSR();
        uint8_t speed = phyconf >> 1 & 0x01;
        uint8_t duplex = phyconf >> 2 & 0x01;
        printf("PHY Duplex: %s\n", speed ? "Full" : "Half");
        printf("PHY Speed: %s\n", duplex ? "100Mbps" : "10Mbps");
        printf("*******************************************\n");
        printf("Timeout: %d\n", TIMEOUT_US);
        printf("Ready.\n");
    }
    else if (strcmp(command, "reboot") == 0) {
        reset_with_watchdog();
    }
    else if (strcmp(command, "ip") == 0) {
        printf("IP: %d.%d.%d.%d\n", net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3]);
    }
    else if (strcmp(command, "port") == 0){
        printf("Port: %d\n", port);
    } 
    else if (strcmp(command, "defaults") == 0){
        restore_default_config();
    }
    else if (strcmp(command, "timeout") == 0){
        printf("Timeout: %d\n", TIMEOUT_US);
    }
    else if (strcmp(command, "mac") == 0) {
        printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", net_info.mac[0], net_info.mac[1], net_info.mac[2], net_info.mac[3], net_info.mac[4], net_info.mac[5]);
    }
    else if (strncmp(command, "timeout ", 8) == 0) {
        int timeout;
        if (sscanf(command, "timeout %d", &timeout) == 1) {
            TIMEOUT_US = timeout;
            save_configuration();
            printf("Timeout changed to %d\n", timeout);
        }
        else {
            printf("Invalid timeout format\n");
        }
    }
    else if (strncmp(command, "tim ", 4) == 0) {
        int tim;
        if (sscanf(command, "tim %d", &tim) == 1) {
            time_constant = tim;
            save_configuration();
            printf("Time const changed to %d\n", tim);
        }
        else {
            printf("Invalid time const format\n");
        }
    }

     else if (strncmp(command, "port ", 5) == 0) {
        int new_port;
        if (sscanf(command, "port %d", &new_port) == 1) {
            port = new_port;
            save_configuration();
            printf("Port changed to %d\n", new_port);
        }
        else {
            printf("Invalid port format\n");
        }
    }
    else if (strncmp(command, "ip ", 3) == 0) {
        int ip0, ip1, ip2, ip3;
        if (sscanf(command, "ip %d.%d.%d.%d", &ip0, &ip1, &ip2, &ip3) == 4) {
            net_info.ip[0] = ip0;
            net_info.ip[1] = ip1;
            net_info.ip[2] = ip2;
            net_info.ip[3] = ip3;
            save_configuration();
            printf("IP changed to %d.%d.%d.%d\n", ip0, ip1, ip2, ip3);
        }
        else {
            printf("Invalid IP format\n");
        }
    } 
    else if (strncmp(command, "mac ", 4) == 0) {
        int mac0, mac1, mac2, mac3, mac4, mac5;
        if (sscanf(command, "mac %x:%x:%x:%x:%x:%x", &mac0, &mac1, &mac2, &mac3, &mac4, &mac5) == 6) {
            net_info.mac[0] = mac0;
            net_info.mac[1] = mac1;
            net_info.mac[2] = mac2;
            net_info.mac[3] = mac3;
            net_info.mac[4] = mac4;
            net_info.mac[5] = mac5;
            save_configuration();
            printf("MAC changed to %02X:%02X:%02X:%02X:%02X:%02X\n", mac0, mac1, mac2, mac3, mac4, mac5);
        }
        else {
            printf("Invalid MAC format\n");
        }
    } else if (strcmp(command, "reset") == 0) {
        reset_with_watchdog();
    } else {
        printf("Unknown command\n");
    }
    command[0] = '\0';
}

// Function: handle_serial_input
// Description: Manages serial input for a terminal interface. 
// Locks/unlocks input based on src_ip[0] (0 enables, non-0 disables). 
// Reads characters non-blocking via getchar_timeout_us, ignores non-printable ASCII (except '\r'), 
// and echoes valid input. Stores characters in a buffer (up to 63) until '\r' or full, 
// then null-terminates and processes the command via process_command, resetting the buffer.
void handle_serial_input() {
    if (!enable_serial) {
        if (timeout_error == 1) {
            enable_serial = true;
            printf("Terminal unlocked.\n");
            printf("Ready.\n");
        }
        return;
    }
    if (timeout_error == 0 && enable_serial) {
        enable_serial = false;
        printf("Terminal locked.\n");
    }
    char inByte = getchar_timeout_us(0);
    if (inByte == PICO_ERROR_TIMEOUT) {
        return;
    }
    if (inByte != '\r'){
        if (inByte < 31 || inByte > 126 ) {
            return;
        }
    }
    printf("%c", inByte);
    if ( inByte != '\r' && (buffer_pos < 63) )
    {
        buffer[buffer_pos] = inByte;
        buffer_pos++;
    }
    else
    {
        printf("\n");
        buffer[buffer_pos] = '\0';
        process_command(buffer);
        buffer_pos = 0;
    }
}