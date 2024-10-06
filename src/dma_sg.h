#pragma once

#include <stdint.h>



typedef struct {
    uint8_t *buffer_ptr;
    int length; 
} DMAPacket ;

DMAPacket get_buff(void);

int Setup_ScatterGather_Rx(void);
int Init_DMA(void);
#define RX_BUFFER_SIZE 8192 // Size of each buffer in bytes
#define NUM_BUFFERS 64        // Number of buffers (scatter-gather)

