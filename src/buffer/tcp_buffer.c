#include "tcp_buffer.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint16_t buffer[depth][width] __attribute__((aligned(32)));
int wb = 0;
int rb = 0;
int in_use_index = 0;
int bytes_blocked_in_read = 0;
int wr_status = 0;
int written_bytes[depth] = {};
void buffer_reset() {
  wb = 0;
  rb = 0;
  in_use_index = 0;
  bytes_blocked_in_read = 0;
  wr_status = 0;
}

uint16_t *buffer_reserve_wr() {
  if (wr_status >0) {
      return NULL;
  }
  // todo it is possible to get second time same value;
  if ((wb + 1) % depth == in_use_index) {
    return NULL;
  }
  wr_status = 1;
  return buffer[wb];
}

uint16_t *buffer_done_wr(int bytes) {
  if ((wb + 1) % depth == in_use_index) {
    return NULL;
  }
  wr_status = 0;
  written_bytes[wb] = bytes;
  uint16_t *buf = buffer[wb];
  wb = (wb + 1) % depth;

  return buf;
}

buffer_read_result buffer_read() {
  buffer_read_result res = {NULL, 0};
  if (rb != wb) {
    res.buffer= buffer[rb];
    res.bytes = written_bytes[rb];
    rb = (rb + 1) % depth;
  }
  return res;
}
void buffer_release_no_blocked_update() { // TODO remove duplicate code
    if (in_use_index == rb) {
        return;
    }
    in_use_index = (in_use_index + 1) % depth;
}

void buffer_release() {
    if (in_use_index == rb) {
        return;
    }
    in_use_index = (in_use_index + 1) % depth;
}
void buffer_release_bytes(int bytes) {
    while(written_bytes[in_use_index] < bytes) {
        bytes -= written_bytes[in_use_index];
        written_bytes[in_use_index] = 0;
        buffer_release(); 
    }
    written_bytes[in_use_index] -= bytes;
    if (written_bytes[in_use_index] == 0) {
        buffer_release();
    } 
}

bool buffer_is_empty() { return wb == rb; }
bool buffer_is_full() { return (wb + 1) % depth == rb; }
int buffer_get_write_index() { return wb; }
int buffer_get_read_index() { return rb; }
int buffer_get_busy_index() { return in_use_index; }
int buffer_get_blocked_bytes() {
    int bytes_in_use = 0;
    int i = in_use_index;
    while (i != rb) {
        bytes_in_use += written_bytes[i];
        i = (i + 1) % depth;
    }
    return bytes_in_use;
}