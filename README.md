### The goal
This is very slow attempt to implement SDR from scratch on Zynq 7020.
I have 65Msample 8bit ADC and Zynq board. The goal is to gather the signal from ADC, extract IQ signals and send to PC

### structure
 - dma_sg.h/dma_sg.c - this is pure implementation of scatter gather dma S2MM mode. - works fine so far 
 - tcp* - this is for tcp. still very raw. Achieved speed is 58.7Mbytes/sec. It is equals to 60 000 000 bytes per second which is desired rate for ADC (30Mhz 16Bit 8bit for I and 8Bit for Q)


### platform settings
#### lwip changes parameters
 - lwip220_dhcp = true
 - lwip220_memp_n_pbuf = 500 
 - lwip220_memp_n_tcp_seg = 1024
 - lwip220_pbuf_pool_size = 512
 - lwip220_tcp_snd_buf = 65535
 - 
 - lwip220_tcp_wnd = 65535
 - lwip220_temac_tcp_ip_rx_checksum_offload = true
 - lwip220_temac_tcp_ip_tx_checksum_offload = true
modules enabled: 
 - xil timer 
 - lwip