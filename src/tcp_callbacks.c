#include "tcp_callbacks.h"
#include "buffer/tcp_buffer.h"
#include "dma_sg.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/tcpbase.h"
#include "lwip/timeouts.h"
#include "lwip/udp.h"
#include "sleep.h"
#include "utils.h"
#include "xil_printf.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <xil_cache.h>
#include <xil_types.h>
static struct tcp_pcb *current_pcb;
extern struct netif echo_netif;
// int dma_transfer_start();
// #define psize (width)
// uint16_t buff[psize] = {};
int sent_bytes = 0;
void udp_transfer() {
  static struct udp_pcb *udp_client;
  struct pbuf *p;
  static ip_addr_t server_ip;

  // Convert IP address string to ip_addr_t
  if (udp_client == NULL) {
    IP4_ADDR(&server_ip, 192, 168, 88, 222);
  

  // Create new UDP control block
  udp_client = udp_new();
  if (!udp_client) {
    printf("Error creating UDP client\n");
    return;
  }
  udp_bind(udp_client, IP_ADDR_ANY, 5000);
  udp_connect(udp_client, &server_ip, 8100);
  }

    DMAPacket packet = get_buff();
    if (packet.length < 0) {
      return;
    }
    p = pbuf_alloc(PBUF_TRANSPORT, packet.length, PBUF_POOL);
    if (!p) {
      printf("Failed to allocate pbuf\n");
      udp_remove(udp_client);
      return;
    }
    memcpy(p->payload, packet.buffer_ptr, packet.length);
    int err = udp_send(udp_client, p);

    if (err != ERR_OK) {
      printf("UDP send failed: %d\n", err);
      // Free the pbuf
      pbuf_free(p);
    }
    // usleep(2);
    pbuf_free(p);
  
}
void tcp_transfer() {

  while (1) {
    if (!current_pcb) {
      return;
    }
    if (tcp_sndbuf(current_pcb) < RX_BUFFER_SIZE) {
      return;
    }
    DMAPacket packet = get_buff();
    if (packet.length <= 0) {
      return;
    }
    tcp_write(current_pcb, packet.buffer_ptr, packet.length,
              TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
    tcp_output(current_pcb);
  }
  //   tcp_output(current_pcb);
}
err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
  UNUSED(arg);
  UNUSED(tpcb);
  return ERR_OK;
}

err_t recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p,
                    err_t err) {
  UNUSED(arg);
  UNUSED(err);
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
  UNUSED(arg);
  UNUSED(err);
  static int connection = 1;
  current_pcb = newpcb;
  /* set the receive callback for this connection */
  tcp_recv(newpcb, recv_callback);
  tcp_sent(newpcb, sent_callback);
  /* just use an integer number indicating the connection id as the
     callback argument */
  tcp_arg(newpcb, (void *)(UINTPTR)connection);
  /* increment for subsequent accepted connections */
  connection++;
  sent_bytes = 0;
  return ERR_OK;
}
