/*
 * Copyright (c) 2022 Brilliant Labs Limited
 * Licensed under the MIT License
 */

#ifndef IQS620_H
#define IQS620_H

#include <stdint.h>
#include <stdbool.h>

#include "nrfx_twi.h"

/**
 * Driver for the IQS620 hall effect, proximity sensor.
 * https://www.azoteq.com/images/stories/pdf/iqs620_datasheet.pdf
 * @defgroup iqs620
 * @ingroup driver_chip
 * @{
 */

typedef unsigned pin_t;

typedef enum {
    IQS620_BUTTON_B0,
    IQS620_BUTTON_B1
} iqs620_button_t;

typedef enum {
    IQS620_BUTTON_UP,
    IQS620_BUTTON_PROX,
    IQS620_BUTTON_DOWN,
} iqs620_event_t;

typedef void (*iqs620_callback_t)(void *iqs620, iqs620_button_t button, iqs620_event_t event);

typedef struct {
    //nrfx_twi_t       twi_drv;
    //pin_t               scl_pin;
    //pin_t               sda_pin;
    pin_t               rdy_pin;
    uint8_t             addr;
    iqs620_callback_t   callback;
    uint8_t             prox_threshold;         ///< set to 0 to use default (22)
    uint8_t             touch_threshold;        ///< set to 0 to use default (37)
    uint8_t             ati_target;             ///< 6-bit value, default is 0x10 = target of 512
    uint8_t             prox_touch_state;       ///< internal data
    uint16_t            button_status;          ///< internal data, added to mimic cy8cmbr3 interfece
} iqs620_t;

bool iqs620_init(iqs620_t *sensor);
bool iqs620_reset(iqs620_t *sensor);
bool iqs620_id(iqs620_t *sensor, uint32_t *id);
bool iqs620_get_button_status(iqs620_t *sensor, uint16_t *status); // added to mimic cy8cmbr3 interfece
bool iqs620_get_ch_count(iqs620_t *sensor, uint8_t channel, uint16_t *ch_data);

/** @} */
#endif
