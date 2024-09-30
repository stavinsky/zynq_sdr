#include "xaxidma.h"
#include "xdebug.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_util.h"
#include "xinterrupt_wrap.h"
#include "xparameters.h"
#include <xil_io.h>
#include <xil_types.h>
#include "buffer/tcp_buffer.h"

#define MEM_BASE_ADDR 0x01000000
#define RX_BUFFER_BASE (MEM_BASE_ADDR + 0x00300000)
#define RESET_TIMEOUT_COUNTER 10000
#define MAX_PKT_LEN (20000)
#define POLL_TIMEOUT_COUNTER 1000000U
#define NUMBER_OF_EVENTS 1

static void RxIntrHandler(void *Callback);

static XAxiDma AxiDma; /* Instance of the XAxiDma */
volatile u32 RxDone;
volatile u32 Error;
volatile u32 transfered_bytes = 0;
// u16 *current_buffer;
// u16 *RxBufferPtr1[MAX_PKT_LEN] __attribute__((aligned(32)));
// u16 *RxBufferPtr2[MAX_PKT_LEN] __attribute__((aligned(32)));

XAxiDma_Config *Config;
int setup_dds_dma_and_interrupts() {
    buffer_reset();
    // Xil_DCacheDisable();
  int Status;
//   RxBufferPtr1 = (u16 *)malloc(MAX_PKT_LEN);
//   RxBufferPtr2 = (u16 *)malloc(MAX_PKT_LEN);
  Config = XAxiDma_LookupConfig(XPAR_XAXIDMA_0_BASEADDR);
  if (!Config) {
    xil_printf("No config found for %d\r\n", XPAR_XAXIDMA_0_BASEADDR);
    return XST_FAILURE;
  }
  Status = XAxiDma_CfgInitialize(&AxiDma, Config);

  if (Status != XST_SUCCESS) {
    xil_printf("Initialization failed %d\r\n", Status);
    return XST_FAILURE;
  }

  if (XAxiDma_HasSg(&AxiDma)) {
    xil_printf("Device configured as SG mode \r\n");
    return XST_FAILURE;
  }

  Status =
      XSetupInterruptSystem(&AxiDma, &RxIntrHandler, Config->IntrId[0],
                            Config->IntrParent, XINTERRUPT_DEFAULT_PRIORITY);
  if (Status != XST_SUCCESS) {
    return XST_FAILURE;
  }
  XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
  XAxiDma_IntrEnable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
  return 0;
}

// int reset_irq() {
//   XDisconnectInterruptCntrl(Config->IntrId[0], Config->IntrParent);
// }
int dma_transfer_start() {

  int status;
  transfered_bytes = 0;
  u16 * current_buffer = buffer_reserve_wr();
  if (!current_buffer) {
      return 0;
  }
// Xil_DCacheFlushRange((UINTPTR)current_buffer, MAX_PKT_LEN );
  /* Initialize flags before start transfer test  */
  RxDone = 0;
  Error = 0;
  status = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)current_buffer, MAX_PKT_LEN,
                                  XAXIDMA_DEVICE_TO_DMA);
  if (status != XST_SUCCESS) {
    return XST_FAILURE;
  }
  // Xil_DCacheFlushRange((UINTPTR)RxBufferPtr, MAX_PKT_LEN);
  return 0;
}

static void RxIntrHandler(void *Callback) {
  u32 IrqStatus;
  int TimeOut;
  XAxiDma *AxiDmaInst = (XAxiDma *)Callback;
  //   transfered_bytes = Xil_In32(XPAR_XAXIDMA_0_BASEADDR + 0x58);
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
    // Xil_DCacheFlushRange((UINTPTR)RxBufferPtr, transfered_bytes);
    //  Xil_DCacheInvalidateRange((UINTPTR)current_buffer, MAX_PKT_LEN);

    transfered_bytes = Xil_In32(XPAR_XAXIDMA_0_BASEADDR + 0x58);
    RxDone = 1;
    buffer_done_wr(transfered_bytes);
  }
}