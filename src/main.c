#include "netif/xadapter.h"
#include "platform.h"
#include "platform_config.h"
#include "xil_printf.h"
#include "xparameters.h"
#include <stdio.h>
#include <xil_types.h>

#include "lwip/dhcp.h"
#include "lwip/tcp.h"
#include "xil_cache.h"

int start_application(void);
int tcp_init_and_dhcp(void);
void tcp_fasttmr(void);
void tcp_slowtmr(void);
int setup_dds_dma_and_interrupts(void);
void tcp_transfer(void);
err_t dhcp_start(struct netif *netif);
extern volatile u32 RxDone;
extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;
extern struct netif *echo_netif;

int main() {
  int status = 0;

  status = tcp_init_and_dhcp();
  if (status != 0) {
    return status;
  }

  start_application();
  status = setup_dds_dma_and_interrupts();
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
