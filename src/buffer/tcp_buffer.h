#ifndef TCP_BUFFER_H
#define TCP_BUFFER_H
#include <stdbool.h>
#include <stdint.h>
#define depth 30
#define width 20000
typedef struct {
    uint16_t *buffer;
    int bytes; 
} buffer_read_result ;
void buffer_reset(void);
uint16_t *buffer_done_wr(int);
buffer_read_result buffer_read(void);
bool buffer_is_empty(void);
bool buffer_is_full(void);
int buffer_get_write_index(void);
int buffer_get_read_index(void);
uint16_t *buffer_reserve_wr(void);
int buffer_get_busy_index(void);
void buffer_release(void);
int buffer_get_blocked_bytes(void);
void buffer_release_bytes(int);
#endif