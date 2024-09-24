#include "platform_config.h"
#include <stdio.h>
#include <string.h>

#include "lwip/err.h"
#include "lwip/tcp.h"
#include "xil_printf.h"

void lwip_init();

static struct netif server_netif;
struct netif *echo_netif;
static struct tcp_pcb *current_pcb;
extern volatile int dhcp_timoutcntr;
extern volatile int tcp_data_sent = 0;
extern u16 *RxBufferPtr;
extern volatile u32 transfered_bytes;
int dma_transfer_start(void);
void tcp_transfer() {
  if (!current_pcb) {
    return;
  }
  if (transfered_bytes == 0) {
    return;
  }
  if (tcp_sndbuf(current_pcb) < transfered_bytes ) {
    tcp_output(current_pcb);
  } 
    tcp_write(current_pcb, (const char *)RxBufferPtr, transfered_bytes , 0x1 |0x2 );
    dma_transfer_start();
}
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

  /* Create a new DHCP client for this interface.
   * Note: you must call dhcp_fine_tmr() and dhcp_coarse_tmr() at
   * the predefined regular intervals after starting the client.
   */
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

err_t recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p,
                    err_t err) {
  /* do not read the packet if we are not in ESTABLISHED state */
  if (!p) {
    tcp_close(tpcb);
    tcp_recv(tpcb, NULL);
    current_pcb = NULL;
    return ERR_OK;
  }

  /* indicate that the packet has been received */
  tcp_recved(tpcb, p->len);
  if (tcp_sndbuf(tpcb) > p->len) {
    err = tcp_write(tpcb, p->payload, p->len, 1);
  } else
    xil_printf("no space in tcp_sndbuf\n\r");

  /* free the received pbuf */
  pbuf_free(p);

  return ERR_OK;
}

err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
  static int connection = 1;
  current_pcb = newpcb;
  /* set the receive callback for this connection */
  tcp_recv(newpcb, recv_callback);

  /* just use an integer number indicating the connection id as the
     callback argument */
  tcp_arg(newpcb, (void *)(UINTPTR)connection);

  /* increment for subsequent accepted connections */
  connection++;
  dma_transfer_start();
  return ERR_OK;
}

int start_application() {
  struct tcp_pcb *pcb;
  err_t err;
  unsigned port = 7;

  /* create new TCP PCB structure */
  pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  if (!pcb) {
    xil_printf("Error creating PCB. Out of Memory\n\r");
    return -1;
  }

  /* bind to specified @port */
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
