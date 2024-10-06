#include "xaxidma.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xscugic.h"
#include <xaxidma_bd.h>
#include <xaxidma_bdring.h>
#include <xaxidma_hw.h>
#include <xil_types.h>

#define DMA_BASEADDR XPAR_AXI_DMA_0_BASEADDR // AXI DMA base address
#define DMA_S2MM_BASEADDR                                                      \
  XPAR_AXIDMA_0_S2MM_BASEADDR // AXI DMA S2MM base address

#define RX_BUFFER_SIZE 8192 // Size of each buffer in bytes
#define NUM_BUFFERS 64        // Number of buffers (scatter-gather)

XAxiDma AxiDma;
XAxiDma_BdRing *RxRing;
#define MEM_BASE_ADDR 0x01000000
#define RX_BUFFER_BASE (MEM_BASE_ADDR + 0x00300000)
#define RX_BUFFER_HIGH (MEM_BASE_ADDR + 0x004FFFFF)
#define RX_BD_SPACE_BASE (MEM_BASE_ADDR + 0x00001000)
#define RX_BD_SPACE_HIGH (MEM_BASE_ADDR + 0x00001FFF)
u32 RxBuffer = RX_BUFFER_BASE;

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

int restart_dma() {
  int status = 0;

   status = XAxiDma_BdRingAlloc(RxRing, NUM_BUFFERS, &BdPtr);
  if (status != XST_SUCCESS) {
    xil_printf("Error: Failed to allocate BDs\n");
    return XST_FAILURE;
  }
  // Submit the BDs to the RX channel
  status = XAxiDma_BdRingToHw(RxRing, NUM_BUFFERS, BdPtr);
  if (status != XST_SUCCESS) {
    xil_printf("Error: Failed to submit BDs to HW\n");
    return XST_FAILURE;
  }

  // Start the RX DMA channel
  status = XAxiDma_BdRingStart(RxRing);
  if (status != XST_SUCCESS) {
    xil_printf("Error: Failed to start RX ring\n");
    return XST_FAILURE;
  }

  return XST_SUCCESS;
}

int Setup_ScatterGather_Rx() {
  int Status;
  RxRing = XAxiDma_GetRxRing(&AxiDma);
  int bdcount;
  UINTPTR RxBufferPtr;
  // Create the RX BD ring
  /* Set delay and coalescing */
  // XAxiDma_BdRingSetCoalesce(RxRingPtr, Coalesce, Delay);
  bdcount = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT,
                                  RX_BD_SPACE_HIGH - RX_BD_SPACE_BASE + 1);
  Status = XAxiDma_BdRingCreate(RxRing, (UINTPTR)RX_BD_SPACE_BASE,
                                (UINTPTR)RX_BD_SPACE_BASE,
                                XAXIDMA_BD_MINIMUM_ALIGNMENT, bdcount);
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

  // Set up each BD to point to a buffer
  //   BdCurPtr = BdPtr;
  //   for (int i = 0; i < NUM_BUFFERS; i++) {
  //     XAxiDma_BdSetBufAddr(BdCurPtr, (UINTPTR)&RxBuffer[i]);
  //     XAxiDma_BdSetLength(BdCurPtr, RX_BUFFER_SIZE, RxRing->MaxTransferLen);
  //     XAxiDma_BdSetCtrl(BdCurPtr, 0);
  //     XAxiDma_BdSetId(BdCurPtr, &RxBuffer[i]);
  //     BdCurPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(RxRing, BdCurPtr);
  //   }

  BdCurPtr = BdPtr;
  RxBufferPtr = RxBuffer;
  for (int i = 0; i < FreeBDCount; i++) {
    Status = XAxiDma_BdSetBufAddr(BdCurPtr, RxBufferPtr);

    if (Status != XST_SUCCESS) {
      xil_printf("Set buffer addr %x on BD %x failed %d\r\n",
                 (unsigned int)RxBufferPtr, (UINTPTR)BdCurPtr, Status);

      return XST_FAILURE;
    }

    Status =
        XAxiDma_BdSetLength(BdCurPtr, RX_BUFFER_SIZE, RxRing->MaxTransferLen);
    if (Status != XST_SUCCESS) {
      xil_printf("Rx set length %d on BD %x failed %d\r\n", RX_BUFFER_SIZE,
                 (UINTPTR)BdCurPtr, Status);

      return XST_FAILURE;
    }

    /* Receive BDs do not need to set anything for the control
     * The hardware will set the SOF/EOF bits per stream status
     */
    XAxiDma_BdSetCtrl(BdCurPtr, 0);
    XAxiDma_BdSetId(BdCurPtr, RxBufferPtr);

    RxBufferPtr += RX_BUFFER_SIZE;
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
// Polling function to check for received data
void PollRxRing() {
  XAxiDma_BdRing *RxRingPtr = XAxiDma_GetRxRing(&AxiDma);
  XAxiDma_Bd *BdRxPtr, *BdRxPtrHead;
  int NumBdProcessed;
  int counter = 0;
  // Poll RX Ring for completed BDs
  while (1) {
    NumBdProcessed = XAxiDma_BdRingFromHw(RxRingPtr, XAXIDMA_ALL_BDS, &BdRxPtr);
    BdRxPtrHead = BdRxPtr;
    if (NumBdProcessed > 0) {
        xil_printf("number of processed %d\n", NumBdProcessed);
      // Process received buffers
      for (int i = 0; i < NumBdProcessed; i++) {
        int bytes =
            XAxiDma_BdGetActualLength(BdRxPtr, RxRingPtr->MaxTransferLen);
        // xil_printf("Data received in buffer %d, %d bytes transferred to buffer\n", counter, bytes);
        u32 ctrl_status = XAxiDma_BdGetSts(BdRxPtr);
        if (ctrl_status & XAXIDMA_BD_STS_ALL_ERR_MASK) {
          xil_printf("\nError detected in BD %d\n", i);
        }
        BdRxPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(RxRingPtr, BdRxPtr);
        counter += 1;
      }

      u32 status = XAxiDma_BdRingFree(RxRingPtr, NumBdProcessed, BdRxPtrHead);
      if (status != XST_SUCCESS) {
          xil_printf("\n\nerror status is %d\n\n", status);
      }
      int free_bds = XAxiDma_BdRingGetFreeCnt(RxRingPtr);
      u32 dma_status = XAxiDma_ReadReg(DMA_BASEADDR, 0x34);
      xil_printf("free bds %d, dma_idle is %d counter = %d\n", free_bds, dma_status & 0b10, counter );
    }

    int free_bds = XAxiDma_BdRingGetFreeCnt(RxRingPtr);
    u32 dma_status = XAxiDma_ReadReg(DMA_BASEADDR, 0x34);
    if ((dma_status & 0b10) > 0 && free_bds == NUM_BUFFERS) {
    //  xil_printf("dma finished\n");
      restart_dma();
    }
  }
}
