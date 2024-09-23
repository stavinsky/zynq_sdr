#include <stdio.h>
#include "xparameters.h"
#include "netif/xadapter.h"
#include "platform.h"
#include "platform_config.h"
#include "xil_printf.h"

#include "lwip/tcp.h"
#include "xil_cache.h"
#include "lwip/dhcp.h"

int start_application();
int tcp_init_and_dhcp();
void tcp_fasttmr(void);
void tcp_slowtmr(void);


err_t dhcp_start(struct netif *netif);

extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;
extern struct netif *echo_netif;
int main()
{
	int status = 0;

	status = tcp_init_and_dhcp();
	if (status != 0 ) {
		return status;
	}

	start_application();

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
	}

	cleanup_platform();
	return 0;
}
