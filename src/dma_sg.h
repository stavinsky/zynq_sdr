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

void dma_sg_start(void);
#define RX_BUFFER_SIZE (8192) // Size of each buffer in bytes
#define NUM_BUFFERS  256       // Number of buffers (scatter-gather)

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

typedef struct {
    uint32_t MM2S_DMACR;          // 0x00 - MM2S DMA Control register
    uint32_t MM2S_DMASR;          // 0x04 - MM2S DMA Status register
    uint32_t MM2S_CURDESC;        // 0x08 - MM2S Current Descriptor Pointer (lower 32 bits)
    uint32_t MM2S_CURDESC_MSB;    // 0x0C - MM2S Current Descriptor Pointer (upper 32 bits)
    uint32_t MM2S_TAILDESC;       // 0x10 - MM2S Tail Descriptor Pointer (lower 32 bits)
    uint32_t MM2S_TAILDESC_MSB;   // 0x14 - MM2S Tail Descriptor Pointer (upper 32 bits)
    uint32_t reserved_1[5];       // 0x18 - 0x2C (padding or reserved space up to 0x2C)
    uint32_t SG_CTL;              // 0x2C - Scatter/Gather User and Cache Control register
    uint32_t S2MM_DMACR;          // 0x30 - S2MM DMA Control register
    uint32_t S2MM_DMASR;          // 0x34 - S2MM DMA Status register
    uint32_t S2MM_CURDESC;        // 0x38 - S2MM Current Descriptor Pointer (lower 32 bits)
    uint32_t S2MM_CURDESC_MSB;    // 0x3C - S2MM Current Descriptor Pointer (upper 32 bits)
    uint32_t S2MM_TAILDESC;       // 0x40 - S2MM Tail Descriptor Pointer (lower 32 bits)
    uint32_t S2MM_TAILDESC_MSB;   // 0x44 - S2MM Tail Descriptor Pointer (upper 32 bits)
} AxiDmaRegisters;
