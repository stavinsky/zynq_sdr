#include "tcp_callbacks.h"
#include "buffer/tcp_buffer.h"
#include "lwip/tcpbase.h"
#include "utils.h"
#include "xil_printf.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <xil_cache.h>
#include <xil_types.h>

static struct tcp_pcb *current_pcb;

// int dma_transfer_start();
#define psize (width)
uint16_t buff[psize] = {};

void tcp_transfer() {
  if (!current_pcb) {
    return;
  }
}
err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
  UNUSED(arg);
  UNUSED(tpcb);

  buffer_release_bytes(len);
//   dma_transfer_start();
  if (tcp_sndbuf(current_pcb) < width) {
    return 0;
  }
  buffer_read_result res = buffer_read();

  if (!res.buffer) {
    return 0;
  }

  Xil_DCacheInvalidateRange((UINTPTR)res.buffer, res.bytes);
  tcp_write(current_pcb, res.buffer, res.bytes, TCP_WRITE_FLAG_COPY);

  return ERR_OK;
  if (tcp_sndbuf(current_pcb) < width) {
    return 0;
  }
  res = buffer_read();

  if (!res.buffer) {
    return 0;
  }

  Xil_DCacheInvalidateRange((UINTPTR)res.buffer, res.bytes);
  tcp_write(current_pcb, res.buffer, res.bytes, TCP_WRITE_FLAG_COPY);
}

err_t recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  UNUSED(arg);
  UNUSED(err);
  if (!p) {
    tcp_close(tpcb);
    tcp_recv(tpcb, NULL);
    current_pcb = NULL;
    buffer_reset();
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
  buffer_read_result res = buffer_read();
  tcp_write(newpcb, res.buffer, res.bytes, TCP_WRITE_FLAG_COPY);
  return ERR_OK;
}
