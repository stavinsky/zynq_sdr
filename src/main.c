#include "netif/xadapter.h"
#include "platform.h"
#include "platform_config.h"
#include "xil_printf.h"
#include "xparameters.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/_intsup.h>
#include <xil_types.h>
#include <xtimer_config.h>

#include "lwip/dhcp.h"
#include "lwip/tcp.h"
#include "tcp_setup.h"
#include "tcp_callbacks.h"
#include "xaxidma.h"
#include "dma_sg.h"
#include "xiltimer.h"
void tcp_fasttmr(void);
void tcp_slowtmr(void);
// int setup_dds_dma_and_interrupts(void);
err_t dhcp_start(struct netif *netif);
extern volatile u32 RxDone;
extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;
extern struct netif *echo_netif;
// int dma_transfer_start();

int Init_DMA(void);

int main() {
  int status = 0;
 XTime start_time;
 XTime stop_time;
 XTime diff;
///
    // Initialize DMA
    // if (Init_DMA() != XST_SUCCESS) {
    //     xil_printf("Failed to initialize DMA\n");
    //     return -1;
    // }

    // Setup scatter-gather RX
    // if (Setup_ScatterGather_Rx() != XST_SUCCESS) {
    //     xil_printf("Failed to set up scatter-gather RX\n");
    //     return -1;
    // }
    dma_sg_start();

//     unsigned long long  i  = 0;
//     const unsigned long long data_limit = (unsigned long long )1*1024*1024*1024; //1 gig
//     XTime_GetTime(&start_time);
//     while (i < data_limit) {
//         DMAPacket p = get_buff();
//         i += p.length;
        
//     }
//     XTime_GetTime(&stop_time);
//     diff = (stop_time - start_time);
//     uint32_t diff_seconds = (unsigned int) ((1.0 * diff) / (COUNTS_PER_SECOND));
//     xil_printf("time for operation is: %d\n", diff_seconds);
  status = tcp_init_and_dhcp();
  if (status != 0) {
    return status;
  }

  start_tcp_server();
//   status = setup_dds_dma_and_interrupts();
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
