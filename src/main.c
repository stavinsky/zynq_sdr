#include "netif/xadapter.h"
#include "platform.h"
#include "platform_config.h"
#include "xil_printf.h"
#include "xparameters.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/_intsup.h>
#include <xil_types.h>

#include "lwip/dhcp.h"
#include "lwip/tcp.h"
#include "tcp_setup.h"
#include "tcp_callbacks.h"
#include "xaxidma.h"
#include "dma_sg.h"
void tcp_fasttmr(void);
void tcp_slowtmr(void);
err_t dhcp_start(struct netif *netif);
extern volatile u32 RxDone;
extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;
extern struct netif *echo_netif;

int Init_DMA(void);

int main() {
  int status = 0;
dma_sg_start();
  status = tcp_init_and_dhcp();
  if (status != 0) {
    return status;
  }

  start_tcp_server();
  if (status != 0) {
    return status;
  }

  while (1) {
    if (TcpFastTmrFlag) {
      tcp_fasttmr();
      TcpFastTmrFlag = 0;
    }
    if (TcpSlowTmrFlag) {
      tcp_slowtmr();
      TcpSlowTmrFlag = 0;
    }
    xemacif_input(echo_netif);
    tcp_transfer();
  }

  cleanup_platform();
  return 0;
}
