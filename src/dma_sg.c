#include "dma_sg.h"
#include "assert.h"
#include "lwip/err.h"
#include "xaxidma.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xscugic.h"
#include <stdint.h>
#include <xaxidma_bd.h>
#include <xaxidma_bdring.h>
#include <xaxidma_hw.h>
#include <xil_types.h>
#define DMA_BASEADDR XPAR_AXI_DMA_0_BASEADDR // AXI DMA base address

XAxiDma AxiDma;
XAxiDma_BdRing *RxRing;

u32 RxBuffer[NUM_BUFFERS][RX_BUFFER_SIZE] __attribute__((aligned(32)));

BlockDescriptor sg_descriptors[NUM_BUFFERS];
BlockDescriptor *first_descriptor = &sg_descriptors[0];
BlockDescriptor *last_descriptor = &sg_descriptors[NUM_BUFFERS - 1];

XAxiDma_Bd BdTemplate;
XAxiDma_Bd *BdPtr, *BdCurPtr;
XAxiDma_Config *Config;

int Init_DMA() {
  int Status;
  Config = XAxiDma_LookupConfig(DMA_BASEADDR);
  if (!Config) {
    xil_printf("No config found for %d\r\n", DMA_BASEADDR);
    return XST_FAILURE;
  }
  Status = XAxiDma_CfgInitialize(&AxiDma, Config);
  if (Status != XST_SUCCESS) {
    xil_printf("Error: DMA initialization failed\n");
    return XST_FAILURE;
  }

  if (!XAxiDma_HasSg(&AxiDma)) {
    xil_printf("Error: DMA is not in scatter-gather mode\n");
    return XST_FAILURE;
  }
  return XST_SUCCESS;
}



int Setup_ScatterGather_Rx() {
  int Status;
  RxRing = XAxiDma_GetRxRing(&AxiDma);

  Status = XAxiDma_BdRingCreate(RxRing, (UINTPTR)sg_descriptors,
                                (UINTPTR)sg_descriptors,
                                XAXIDMA_BD_MINIMUM_ALIGNMENT, NUM_BUFFERS);
  if (Status != XST_SUCCESS) {
    xil_printf("Error: Failed to create Rx ring\n");
    return XST_FAILURE;
  }

  // Initialize buffer descriptor template
  XAxiDma_BdClear(&BdTemplate);
  Status = XAxiDma_BdRingClone(RxRing, &BdTemplate);
  if (Status != XST_SUCCESS) {
    xil_printf("Error: Failed to clone BD template\n");
    return XST_FAILURE;
  }
  int FreeBDCount = XAxiDma_BdRingGetFreeCnt(RxRing);
  xil_printf("Free BDs in the ring: %d\n", FreeBDCount);
  // Allocate BDs for the buffers
  Status = XAxiDma_BdRingAlloc(RxRing, NUM_BUFFERS, &BdPtr);
  if (Status != XST_SUCCESS) {
    xil_printf("Error: Failed to allocate BDs\n");
    return XST_FAILURE;
  }

  //   Set up each BD to point to a buffer
  BdCurPtr = BdPtr;
  for (int i = 0; i < NUM_BUFFERS; i++) {
    XAxiDma_BdSetBufAddr(BdCurPtr, (UINTPTR)&RxBuffer[i]);
    XAxiDma_BdSetLength(BdCurPtr, RX_BUFFER_SIZE, RxRing->MaxTransferLen);
    XAxiDma_BdSetCtrl(BdCurPtr, 0);
    XAxiDma_BdSetId(BdCurPtr, &RxBuffer[i]);
    BdCurPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(RxRing, BdCurPtr);
  }

  Status = XAxiDma_BdRingToHw(RxRing, NUM_BUFFERS, BdPtr);
  if (Status != XST_SUCCESS) {
    xil_printf("Error: Failed to submit BDs to HW\n");
    return XST_FAILURE;
  }

  // Start the RX DMA channel
  Status = XAxiDma_BdRingStart(RxRing);
  if (Status != XST_SUCCESS) {
    xil_printf("Error: Failed to start RX ring\n");
    return XST_FAILURE;
  }
  return Status;
}
#define bd_clear_complete_status(bd) bd->STATUS &= 0x7fffffff; // reset complete status

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
    packet.length = current_bd->STATUS & 0x1ffffff;
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
