#include "dma_sg.h"
#include "assert.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xscugic.h"
#include <stdint.h>
#include <xil_types.h>
#define DMA_BASEADDR XPAR_AXI_DMA_0_BASEADDR // AXI DMA base address

u8 RxBuffer[NUM_BUFFERS][RX_BUFFER_SIZE] __attribute__((aligned(32)));

BlockDescriptor sg_descriptors[NUM_BUFFERS];
BlockDescriptor *first_descriptor = &sg_descriptors[0];
BlockDescriptor *last_descriptor = &sg_descriptors[NUM_BUFFERS - 1];

void setup_block_descriptors(){
    int i;
    for (i=0; i<NUM_BUFFERS; i++){
        BlockDescriptor *bd = &sg_descriptors[i];
        bd->BUFFER_ADDRESS = (UINTPTR)RxBuffer[i];
        bd->CONTROL = 0;
        bd->CONTROL = RX_BUFFER_SIZE;
        bd->STATUS = 0;
        if (i < NUM_BUFFERS - 1 ) {
            bd->NXTDESC = (UINTPTR)&sg_descriptors[i+1];
        }
        else {
            bd->NXTDESC = (UINTPTR)&sg_descriptors[0];
        }
    }
}
void dma_sg_start(){
    setup_block_descriptors();
    Xil_DCacheFlushRange((UINTPTR)sg_descriptors, sizeof(BlockDescriptor) * NUM_BUFFERS);
    AxiDmaRegisters *DMA = (AxiDmaRegisters *)DMA_BASEADDR;
    DMA->S2MM_CURDESC = ((UINTPTR)&sg_descriptors[0]); // check address
    DMA->S2MM_DMACR |= 0x1; // start dma
    DMA->S2MM_TAILDESC = ((UINTPTR)&sg_descriptors[NUM_BUFFERS-1]); // check address
    Xil_DCacheFlushRange((UINTPTR)DMA, sizeof(AxiDmaRegisters));
}

#define bd_clear_complete_status(bd) bd->STATUS &= 0x7fffffff // reset complete status
#define bd_get_lengh(bd) (bd->STATUS & 0x1ffffff)
DMAPacket get_buff() {
  DMAPacket packet;
  packet.length = 0;
  packet.buffer_ptr = NULL;
  packet.status = XST_SUCCESS;
  AxiDmaRegisters *DMA = (AxiDmaRegisters *)DMA_BASEADDR;
  static BlockDescriptor *current_bd = NULL;
  static int free_num = 0;
  if (current_bd == NULL) {
    current_bd = first_descriptor;
  }
  // TODO check if dma is halted and return an error;
  Xil_DCacheInvalidateRange((UINTPTR)current_bd, sizeof(BlockDescriptor));
  Xil_DCacheInvalidateRange((UINTPTR)DMA, sizeof(AxiDmaRegisters));

  while  ((current_bd->STATUS & (1 << 31)) == 0) {
    Xil_DCacheInvalidateRange((UINTPTR)current_bd, sizeof(BlockDescriptor));
      Xil_DCacheInvalidateRange((UINTPTR)DMA, sizeof(AxiDmaRegisters));
  };
  if ((UINTPTR)current_bd != DMA->S2MM_CURDESC) {
    packet.buffer_ptr = (uint8_t *)(uintptr_t)current_bd->BUFFER_ADDRESS;
    packet.length = bd_get_lengh(current_bd);
    bd_clear_complete_status(current_bd);
    u32 cache_bd = (UINTPTR) current_bd;
    Xil_DCacheFlushRange((UINTPTR)current_bd, sizeof(BlockDescriptor));
    current_bd = (BlockDescriptor *)(uintptr_t)current_bd->NXTDESC;
    Xil_DCacheInvalidateRange((UINTPTR)packet.buffer_ptr, packet.length);
    if (++free_num >=100 ) {
        DMA->S2MM_TAILDESC = cache_bd;
        Xil_DCacheFlushRange((UINTPTR)DMA, sizeof(AxiDmaRegisters));
        free_num = 0;     
    } 
    return packet;
  }
  // normally this code is not accessable because we moving the tail while reading 
  // needs to be additionaly tested to be sure
  if ((DMA->S2MM_CURDESC == DMA->S2MM_TAILDESC) && (DMA->S2MM_DMASR & 1<<1)) { // curr = tail and status is idle 
    packet.buffer_ptr = (uint8_t *)(uintptr_t)current_bd->BUFFER_ADDRESS;
    packet.length = current_bd->STATUS & 0x1ffffff;
    current_bd -> STATUS &= 0x7fffffff; // reset complete status

    Xil_DCacheFlushRange((UINTPTR)sg_descriptors, sizeof(BlockDescriptor) * NUM_BUFFERS);
    DMA->S2MM_CURDESC = (UINTPTR)first_descriptor;
    DMA->S2MM_TAILDESC = (UINTPTR)last_descriptor;
    Xil_DCacheFlushRange((UINTPTR)DMA, sizeof(AxiDmaRegisters));
    current_bd = first_descriptor;
    Xil_DCacheInvalidateRange((UINTPTR)packet.buffer_ptr, packet.length);
    free_num = 0;
    return packet;
    
  }

  return packet;
}
