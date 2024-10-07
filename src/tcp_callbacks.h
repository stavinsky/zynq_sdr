#ifndef TCP_CALLBACK_H
#define TCP_CALLBACK_H

#include "lwip/err.h"
#include "lwip/tcp.h"

err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len);
err_t recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err); 
void tcp_transfer(void);
void udp_transfer(void);
#endif