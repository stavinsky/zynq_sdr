#include "xaxidma.h"
#include "xdebug.h"
#include "xil_exception.h"
#include "xil_util.h"
#include "xinterrupt_wrap.h"
#include "xparameters.h"
#include <xil_io.h>


#define MEM_BASE_ADDR 0x01000000

#define RX_BUFFER_BASE (MEM_BASE_ADDR + 0x00300000)

#define RESET_TIMEOUT_COUNTER 10000

#define MAX_PKT_LEN 256

#define POLL_TIMEOUT_COUNTER 1000000U
#define NUMBER_OF_EVENTS 1

// extern void xil_printf(const char *format, ...);

static void RxIntrHandler(void *Callback);

static XAxiDma AxiDma; /* Instance of the XAxiDma */
volatile u32 RxDone;
volatile u32 Error;
volatile u32 transfered_bytes = 0;
int main1(void) {
  int Status;
  XAxiDma_Config *Config;

  u32 *RxBufferPtr;

  RxBufferPtr = (u32 *)RX_BUFFER_BASE;

  xil_printf("\r\n--- Entering main() --- \r\n");

  Config = XAxiDma_LookupConfig(XPAR_XAXIDMA_0_BASEADDR);
  if (!Config) {
    xil_printf("No config found for %d\r\n", XPAR_XAXIDMA_0_BASEADDR);

    return XST_FAILURE;
  }

  /* Initialize DMA engine */
  Status = XAxiDma_CfgInitialize(&AxiDma, Config);
  

  if (Status != XST_SUCCESS) {
    xil_printf("Initialization failed %d\r\n", Status);
    return XST_FAILURE;
  }

  if (XAxiDma_HasSg(&AxiDma)) {
    xil_printf("Device configured as SG mode \r\n");
    return XST_FAILURE;
  }
 
  XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)RxBufferPtr, MAX_PKT_LEN,
                                  XAXIDMA_DEVICE_TO_DMA);
  while((Xil_In32(XPAR_XAXIDMA_0_BASEADDR+0x34) & 1<<1) == 0) {} 
  /* Set up Interrupt system  */

  Status =
      XSetupInterruptSystem(&AxiDma, &RxIntrHandler, Config->IntrId[0],
                            Config->IntrParent, XINTERRUPT_DEFAULT_PRIORITY);
  if (Status != XST_SUCCESS) {
    return XST_FAILURE;
  }

  /* Disable all interrupts before setup */

  XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

  /* Enable all interrupts */

  XAxiDma_IntrEnable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
 transfered_bytes = 0;
 for (int i=0; i<=3; i++) {
      Xil_DCacheFlushRange((UINTPTR)RxBufferPtr, MAX_PKT_LEN);
  /* Initialize flags before start transfer test  */
  RxDone = 0;
  Error = 0;
  Status = XAxiDma_SimpleTransfer(
      &AxiDma, 
      (UINTPTR)(RxBufferPtr + (64  * i)), 
      MAX_PKT_LEN,                       
      XAXIDMA_DEVICE_TO_DMA);

  if (Status != XST_SUCCESS) { return XST_FAILURE; }

  Status = Xil_WaitForEventSet(POLL_TIMEOUT_COUNTER, NUMBER_OF_EVENTS, &Error);

  if (Status == XST_SUCCESS && !RxDone) {
    xil_printf("Receive error %d\r\n", Status);
    goto Done;
  }
  Status = Xil_WaitForEventSet(POLL_TIMEOUT_COUNTER, NUMBER_OF_EVENTS, &RxDone);
  if (Status != XST_SUCCESS) {
    xil_printf("Receive failed %d\r\n", Status);
    goto Done;
  }
 }
  Xil_DCacheFlushRange((UINTPTR)RxBufferPtr, MAX_PKT_LEN);
  xil_printf("Successfully ran AXI DMA interrupt Example\r\n");

  /* Disable TX and RX Ring interrupts and return success */
  XDisconnectInterruptCntrl(Config->IntrId[0], Config->IntrParent);

Done:
  xil_printf("--- Exiting main() --- \r\n");
  for (int i = 0; i <= 256; i++) {
    xil_printf("%d, %u\n", i, RxBufferPtr[i]);
  }
  if (Status != XST_SUCCESS) {
    return XST_FAILURE;
  }

  return XST_SUCCESS;
}

static void RxIntrHandler(void *Callback) {
  u32 IrqStatus;
  int TimeOut;
  XAxiDma *AxiDmaInst = (XAxiDma *)Callback;
  transfered_bytes = transfered_bytes + Xil_In32(XPAR_XAXIDMA_0_BASEADDR+0x58);
  /* Read pending interrupts */
  IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst, XAXIDMA_DEVICE_TO_DMA);

  /* Acknowledge pending interrupts */
  XAxiDma_IntrAckIrq(AxiDmaInst, IrqStatus, XAXIDMA_DEVICE_TO_DMA);

  if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {
    return;
  }

  if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

    Error = 1;

    /* Reset could fail and hang
     * NEED a way to handle this or do not call it??
     */
    XAxiDma_Reset(AxiDmaInst);

    TimeOut = RESET_TIMEOUT_COUNTER;

    while (TimeOut) {
      if (XAxiDma_ResetIsDone(AxiDmaInst)) {
        break;
      }

      TimeOut -= 1;
    }

    return;
  }

  /*
   * If completion interrupt is asserted, then set RxDone flag
   */
  if ((IrqStatus & XAXIDMA_IRQ_IOC_MASK)) {

    RxDone = 1;
  }
}