#include "tcp_setup.h"
#include "lwip/err.h"
#include "lwip/tcp.h"
#include "lwip/dhcp.h"
#include "platform_config.h"
#include "platform.h"
#include "netif/xadapter.h"
#include "xil_printf.h"
#include "utils.h"
#include "tcp_callbacks.h"

struct netif *echo_netif;
static struct netif server_netif;
extern volatile int dhcp_timoutcntr;

void lwip_init(void);

int tcp_init_and_dhcp() {
  ip_addr_t ipaddr, netmask, gw;
  unsigned char mac_ethernet_address[] = {0x00, 0x0a, 0x35, 0x00, 0x01, 0x02};
  echo_netif = &server_netif;
  init_platform();
  ipaddr.addr = 0;
  gw.addr = 0;
  netmask.addr = 0;
  lwip_init();
  /* Add network interface to the netif_list, and set it as default */
  if (!xemac_add(echo_netif, &ipaddr, &netmask, &gw, mac_ethernet_address,
                 PLATFORM_EMAC_BASEADDR)) {
    xil_printf("Error adding N/W interface\n\r");
    return -1;
  }
  netif_set_default(echo_netif);
  netif_set_up(echo_netif);

  dhcp_start(echo_netif);
  dhcp_timoutcntr = 240;

  while (((echo_netif->ip_addr.addr) == 0) && (dhcp_timoutcntr > 0)) {
    xemacif_input(echo_netif);
  }

  if (dhcp_timoutcntr <= 0) {
    if ((echo_netif->ip_addr.addr) == 0) {
      xil_printf("DHCP Timeout\r\n");
      xil_printf("Configuring default IP of 192.168.1.10\r\n");
      IP4_ADDR(&(echo_netif->ip_addr), 192, 168, 1, 10);
      IP4_ADDR(&(echo_netif->netmask), 255, 255, 255, 0);
      IP4_ADDR(&(echo_netif->gw), 192, 168, 1, 1);
    }
  }

  ipaddr.addr = echo_netif->ip_addr.addr;
  gw.addr = echo_netif->gw.addr;
  netmask.addr = echo_netif->netmask.addr;

  print_ip_settings(&ipaddr, &netmask, &gw);
  return 0;
}
int start_application() {
  struct tcp_pcb *pcb;
  err_t err;
  unsigned port = 7;

  pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  if (!pcb) {
    xil_printf("Error creating PCB. Out of Memory\n\r");
    return -1;
  }

  err = tcp_bind(pcb, IP_ANY_TYPE, port);
  if (err != ERR_OK) {
    xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
    return -2;
  }

  /* we do not need any arguments to callback functions */
  tcp_arg(pcb, NULL);

  /* listen for connections */
  pcb = tcp_listen(pcb);
  if (!pcb) {
    xil_printf("Out of memory while tcp_listen\n\r");
    return -3;
  }

  /* specify callback to use for incoming connections */
  tcp_accept(pcb, accept_callback);

  xil_printf("TCP echo server started @ port %d\n\r", port);

  return 0;
}
