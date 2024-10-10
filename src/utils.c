#include "lwip/tcp.h"
#include "xil_printf.h"
#include "xiltimer.h"
#include "dma_sg.h"
#include "stdint.h"
void print_ip(char *msg, ip_addr_t *ip)
{
	print(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip),
		   ip4_addr3(ip), ip4_addr4(ip));
}

void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{

	print_ip("Board IP: ", ip);
	print_ip("Netmask : ", mask);
	print_ip("Gateway : ", gw);
}

void test_dma_speed(){
    XTime start_time;
    XTime stop_time;
    XTime diff;
    dma_sg_start();
    unsigned long long  i  = 0;
    const unsigned long long data_limit = (unsigned long long )1*1024*1024*1024; //1 gig
    XTime_GetTime(&start_time);
    while (i < data_limit) {
        DMAPacket p = get_buff();
        i += p.length;
     
    }
    XTime_GetTime(&stop_time);
    diff = (stop_time - start_time);
    uint32_t diff_seconds = (unsigned int) ((1.0 * diff) / (COUNTS_PER_SECOND));
    xil_printf("time for operation is: %d\n", diff_seconds);
}