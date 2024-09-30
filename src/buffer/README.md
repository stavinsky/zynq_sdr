
# Circular Buffer for DMA and lwIP Transmission

This project implements a circular buffer system to manage data flow between **AXI DMA** and **lwIP** for efficient data transmission. The buffer system ensures smooth data handling when sending and receiving data in chunks using the `tcp_write` function from lwIP and allows for simultaneous reading and writing operations without data overwriting.

## Features

- **Circular Buffer**: Allows continuous reading and writing of data between the DMA and TCP.
- **Buffer Reservation and Consumption**: Supports reserving buffer space for DMA transactions and committing the data once the transaction is complete.
- **TCP Integration**: Designed to work with lwIP for transmitting data in small chunks via TCP.
- **Efficient Buffer Management**: Handles partial buffer releases and ensures no overwriting of data during the DMA process.

## Buffer Structure

The circular buffer is implemented using a statically allocated 2D array:

```c
uint16_t buffer[depth][width] __attribute__((aligned(32)));
int wb = 0;  // Write index
int rb = 0;  // Read index
int in_use_index = 0;  // Track buffers waiting for data to be processed
```

- `wb` (Write Buffer): Tracks the index where new data will be written.
- `rb` (Read Buffer): Tracks the index from which data will be read.
- `in_use_index`: Tracks buffers currently being used for TCP transmission or other processing.

## Functions

### Buffer Control Functions

- **`buffer_reset()`**: Resets the buffer state to start from scratch.
- **`buffer_reserve_wr()`**: Reserves a buffer for a DMA write operation, returning a pointer to the buffer if available.
- **`buffer_done_wr()`**: Marks the buffer as ready after the DMA transaction has completed.
- **`buffer_read()`**: Returns a pointer to the buffer for reading, advancing the read index (`rb`).
- **`buffer_release()`**: Releases a buffer once data is processed, advancing the `in_use_index`.
- **`release_bytes(int byte_count)`**: Tracks the number of bytes sent over TCP and releases the buffer when all data is sent.
- **`buffer_is_empty()`**: Checks if the buffer is empty.
- **`buffer_is_full()`**: Checks if the buffer is full.

### Example Usage with lwIP TCP

1. **Start Transmission**:
   Prepare a buffer for DMA and start the TCP transmission using `tcp_write`:

   ```c
   uint16_t* buf = buffer_reserve_wr();
   if (buf != NULL) {
       start_dma_transaction(buf);  // Begin DMA transaction
   }
   ```

2. **Handle TCP Sent Callback**:
   After TCP sends a portion of the data, use the `tcp_sent` callback to track how many bytes have been sent and release the buffer when done:

   ```c
   err_t on_sent_callback(void *arg, struct tcp_pcb *pcb, u16_t len) {
       release_bytes(len);  // Release buffer space after sending len bytes
       send_next_chunk(pcb);  // Send the next chunk of data
       return ERR_OK;
   }
   ```

3. **Commit the Buffer After DMA Completion**:
   When the DMA finishes writing, commit the buffer and start sending the next one:

   ```c
   buffer_done_wr();  // Mark the buffer as fully written
   ```


### Further Improvements

- **Concurrency**: This implementation assumes a single thread or interrupt-based system. If multiple threads are accessing the buffer, ensure that access to `wb`, `rb`, and `in_use_index` is synchronized.
- **Buffer Size Configuration**: Modify the `depth` and `width` constants to adjust the buffer size based on your application requirements.
