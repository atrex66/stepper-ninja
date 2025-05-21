#ifndef CONFIG_H
#define CONFIG_H


// only one page of flash is used for configuration max 4096 bytes
#pragma pack(push, 1)
typedef struct {
    uint8_t mac[6]; // MAC address
    uint8_t ip[4];  // IP address
    uint8_t sn[4];  // Subnet mask
    uint8_t gw[4];  // Gateway
    uint8_t dns[4]; // DNS server
    uint8_t dhcp;   // DHCP mode (0: Static, 1: DHCP)
    uint16_t port; // UDP port
    uint32_t timeout; // Timeout in microseconds
    uint8_t checksum;
} configuration_t;
#pragma pack(pop)

void clear_flash();
void restore_default_config();
void save_config_to_flash();
void load_config_from_flash();

#endif // CONFIG_H