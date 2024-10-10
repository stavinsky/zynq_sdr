### The goal
This is very slow attempt to implement SDR from scratch on Zynq 7020.
I have 65Msample 8bit ADC and Zynq board. The goal is to gather the signal from ADC, extract IQ signals and send to PC

### structure
 - dma_sg.h/dma_sg.c - this is pure implementation of scatter gather dma S2MM mode. - works fine so far 
 - tcp* - this is for tcp. still very raw. Achieved speed is 58.7Mbytes/sec. It is equals to 60 000 000 bytes per second which is desired rate for ADC (30Mhz 16Bit 8bit for I and 8Bit for Q)
