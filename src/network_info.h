#ifndef __NETWORK_INFO_H__
#define __NETWORK_INFO_H__
typedef struct {
    unsigned int link_quality;
    unsigned int signal_level;
    unsigned int noise_level;
} wifi_info_t;

int get_wifi_info(wifi_info_t *wifi);
char *get_ip_address(char *interface);

#endif
