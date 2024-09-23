#ifndef __PLATFORM_UTILS_H_
#define __PLATFORM_UTILS_H_
#include "lwip/tcp.h"

void print_ip(char *msg, ip_addr_t *ip);
void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)

#endif