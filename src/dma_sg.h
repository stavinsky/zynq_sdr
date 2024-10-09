#pragma once

#include <stdint.h>
#include <xil_types.h>
#include "assert.h"


typedef struct {
    uint8_t *buffer_ptr;
    int length; 
    int status;
} DMAPacket ;

DMAPacket get_buff(void);

int Setup_ScatterGather_Rx(void);
int Init_DMA(void);
#define RX_BUFFER_SIZE (2048) // Size of each buffer in bytes
#define NUM_BUFFERS  6       // Number of buffers (scatter-gather)

typedef  struct {
    u32 NXTDESC;
    u32 NXTDESC_MSB;
    u32 BUFFER_ADDRESS;
    u32 BUFFER_ADDRESS_MSB;
    u32 _RESERVED1;
    u32 _RESERVED2;
    u32 CONTROL;
    u32 STATUS;
    u32 APP0;  
    u32 APP1;
    u32 APP2;
    u32 APP3;
    u32 APP4;
    u32 RESERVED3;
    u32 RESERVED4;
    u32 RESERVED5;

} __attribute__((aligned(0x40))) BlockDescriptor;
static_assert(sizeof(BlockDescriptor) == 64, "the size of Block Descriptor and alignment");
