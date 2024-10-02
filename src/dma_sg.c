#include "xaxidma.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xscugic.h"
#include <xaxidma_bd.h>
#include <xil_types.h>

#define DMA_BASEADDR XPAR_AXI_DMA_0_BASEADDR // AXI DMA base address
#define DMA_S2MM_BASEADDR                                                      \
  XPAR_AXIDMA_0_S2MM_BASEADDR // AXI DMA S2MM base address

#define RX_BUFFER_SIZE 8192 // Size of each buffer in bytes
#define NUM_BUFFERS 16      // Number of buffers (scatter-gather)

XAxiDma AxiDma;
XAxiDma_BdRing *RxRing;

u8 RxBuffer[NUM_BUFFERS][RX_BUFFER_SIZE]
    __attribute__((aligned(XAXIDMA_BD_MINIMUM_ALIGNMENT)));

XAxiDma_Bd BdTemplate;
XAxiDma_Bd *BdPtr, *BdCurPtr;
XAxiDma_Config *Config;

int Init_DMA() {
  int Status;
  Xil_DCacheDisable();
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

  // Ensure the DMA is in scatter-gather mode
  if (!XAxiDma_HasSg(&AxiDma)) {
    xil_printf("Error: DMA is not in scatter-gather mode\n");
    return XST_FAILURE;
  }

  return XST_SUCCESS;
}

int Setup_ScatterGather_Rx() {
  int Status;
  RxRing = XAxiDma_GetRxRing(&AxiDma);

  // Create the RX BD ring
  Status = XAxiDma_BdRingCreate(RxRing, (UINTPTR)RxBuffer, (UINTPTR)RxBuffer,
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

  xil_printf("Free BDs in the ring: %d\n", XAxiDma_BdRingGetFreeCnt(RxRing));
  // Allocate BDs for the buffers
  Status = XAxiDma_BdRingAlloc(RxRing, NUM_BUFFERS, &BdPtr);
  if (Status != XST_SUCCESS) {
    xil_printf("Error: Failed to allocate BDs\n");
    return XST_FAILURE;
  }

  // Set up each BD to point to a buffer
  BdCurPtr = BdPtr;
  for (int i = 0; i < NUM_BUFFERS; i++) {
    XAxiDma_BdSetBufAddr(BdCurPtr, (UINTPTR)&RxBuffer[i]);
    XAxiDma_BdSetLength(BdCurPtr, RX_BUFFER_SIZE, RxRing->MaxTransferLen);
    XAxiDma_BdSetCtrl(BdCurPtr, 0);
    XAxiDma_BdSetId(BdCurPtr, &RxBuffer[i]);
    BdCurPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(RxRing, BdCurPtr);
  }

  // Submit the BDs to the RX channel
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

  return XST_SUCCESS;
}
// Polling function to check for received data
void PollRxRing() {
  XAxiDma_BdRing *RxRingPtr = XAxiDma_GetRxRing(&AxiDma);
  XAxiDma_Bd *BdRxPtr;
  int NumBdProcessed;

  // Poll RX Ring for completed BDs
  while (1) {
    NumBdProcessed = XAxiDma_BdRingFromHw(RxRingPtr, XAXIDMA_ALL_BDS, &BdRxPtr);
    if (NumBdProcessed > 0) {
      // Process received buffers
      for (int i = 0; i < NumBdProcessed; i++) {
        int bytes =
            XAxiDma_BdGetActualLength(BdRxPtr, RxRingPtr->MaxTransferLen);
        xil_printf(
            "Data received in buffer %d, %d bytes transferred to buffer\n", i,
            bytes);
        u32 ctrl_status = XAxiDma_BdGetCtrl(BdPtr);
        if (ctrl_status & XAXIDMA_BD_STS_ALL_ERR_MASK) {
          xil_printf("Error detected in BD %d\n", i);
        }
        if (ctrl_status & XAXIDMA_BD_CTRL_TXEOF_MASK) {
          xil_printf("End of Frame (EOF) in buffer %d\n", i);
        }
        BdRxPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(RxRingPtr, BdRxPtr);
      }
      XAxiDma_BdRingFree(RxRingPtr, NumBdProcessed, BdRxPtr);
    }
  }
}
