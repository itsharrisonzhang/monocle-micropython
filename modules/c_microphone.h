#include <string.h>
#include "monocle.h"

#define RESULT_OK   0
#define RESULT_ERR  1

static inline void microphone_fpga_read(uint16_t address, uint8_t *buffer, size_t length);

static inline void microphone_fpga_write(uint16_t address, uint8_t *buffer, size_t length);

static size_t microphone_bytes_available(void);

static int microphone_init(void);

static int microphone_record(float seconds, uint16_t bit_depth, uint16_t sample_rate)

static int microphone_stop(void);

static int microphone_read(mp_obj_t samples);