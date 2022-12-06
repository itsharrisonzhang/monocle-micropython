/*
 * Copyright (c) 2022 Raj Nakarja - Silicon Witchery AB
 * Copyright (c) 2022 Brilliant Labs Limited
 * Licensed under the MIT License
 */

/**
 * Bluetooth Low Energy (BLE) driver with Nordic UART Service console.
 * @file monocle_ble.c
 * @author Raj Nakarja - Silicon Witchery AB
 * @author Josuah Demangeon - Panoramix Labs
 */

#include <stdint.h>
#include <string.h>
#include "monocle_ble.h"
#include "monocle_config.h"
#include "nrfx.h"
#include "nrfx_glue.h"
// From the SoftDevice include dir:
#include "ble.h"
#include "nrf_sdm.h"

#define BLE_ADV_MAX_SIZE 31
#define BLE_MAX_MTU_LENGTH          128
#define BLE_UUID_COUNT              2

/** Buffer sizes for REPL ring buffers; +45 allows a bytearray to be printed in one go. */
#define RING_BUFFER_LENGTH (1024 + 45)

/**
 * Holds the handles for the conenction and characteristics.
 * Convenient for use in interrupts, to get all service-specific data
 * we need to carry around.
 */
typedef struct {
    uint16_t handle;
    ble_gatts_char_handles_t rx_characteristic;
    ble_gatts_char_handles_t tx_characteristic;
} ble_service_t;

/** List of all services we might get a connection for. */
static ble_service_t ble_nus_service, ble_raw_service;

/** Identifier for the active connection with a single device. */
uint16_t ble_conn_handle = BLE_CONN_HANDLE_INVALID;

/** Advertising configured globally for all services. */
uint8_t ble_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;

/** This is the ram start pointer as set in the nrf52811.ld file. */
extern uint32_t _ram_start;

/** The `_ram_start` symbol's address often needs to be passed as an integer. */
static uint32_t ram_start = (uint32_t)&_ram_start;

/**
 * This is the negotiated MTU length. Not used for anything currently.
 */
static uint16_t negotiated_mtu;

// Ring buffer library

/**
 * Ring buffers for the repl rx and tx data which goes over BLE.
 */
typedef struct {
    uint8_t buffer[RING_BUFFER_LENGTH];
    uint16_t head;
    uint16_t tail;
} ring_buf_t;

ring_buf_t nus_rx, nus_tx;

static inline bool ring_full(ring_buf_t const *ring)
{
    uint16_t next = ring->tail + 1;
    if (next == sizeof(ring->buffer))
        next = 0;
    return next == ring->head;
}

static inline bool ring_empty(ring_buf_t const *ring)
{
    return ring->head == ring->tail;
}

static inline void ring_push(ring_buf_t *ring, uint8_t byte)
{
    ring->buffer[ring->tail++] = byte;
    if (ring->tail == sizeof(ring->buffer))
        ring->tail = 0;
}

static inline uint8_t ring_pop(ring_buf_t *ring)
{
    uint8_t byte = ring->buffer[ring->head++];
    if (ring->head == sizeof(ring->buffer))
            ring->head = 0;
    return byte;
}

// Nordic UART Service service functions

/**
 * Sends all buffered data in the tx ring buffer over BLE.
 */
void ble_nus_flush_tx(void)
{
    // Local buffer for sending data
    uint8_t out_buffer[BLE_MAX_MTU_LENGTH] = "";
    uint16_t out_len = 0;
    ble_service_t *service = &ble_nus_service;

    // If not connected, do not flush.
    if (ble_conn_handle == BLE_CONN_HANDLE_INVALID)
        return;

    // If there's no data to send, simply return
    if (ring_empty(&nus_tx))
        return;

    // For all the remaining characters, i.e until the heads come back together
    while (!ring_empty(&nus_tx))
    {
        // Copy over a character from the tail to the outgoing buffer
        out_buffer[out_len++] = ring_pop(&nus_tx);

        // Break if we over-run the negotiated MTU size, send the rest later
        if (out_len >= negotiated_mtu)
            break;
    }

    // Initialise the service value parameters
    ble_gatts_hvx_params_t hvx_params = {0};
    hvx_params.handle = service->tx_characteristic.value_handle;
    hvx_params.p_data = out_buffer;
    hvx_params.p_len = (uint16_t *)&out_len;
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;

    uint32_t err;
    do {
        NRFX_ASSERT(ble_conn_handle != BLE_CONN_HANDLE_INVALID);

        // Send the data
        err = sd_ble_gatts_hvx(ble_conn_handle, &hvx_params);

    // Retry if resources are unavailable.
    } while (err == NRF_ERROR_RESOURCES);

    // Ignore errors if not connected
    if (err == NRF_ERROR_INVALID_STATE || err == BLE_ERROR_INVALID_CONN_HANDLE)
        return;

    // Catch other errors
    NRFX_ASSERT(!err);
}

int ble_nus_rx(void)
{
    while (ring_empty(&nus_rx))
    {
        // While waiting for incoming data, we can push outgoing data
        ble_nus_flush_tx();

        // If there's nothing to do
        if (ring_empty(&nus_tx) && ring_empty(&nus_rx))
            // Wait for events to save power
            sd_app_evt_wait();
    }

    // Return next character from the RX buffer.
    return ring_pop(&nus_rx);
}

void ble_nus_tx(char const *buf, size_t sz)
{
    for (; sz > 0; buf++, sz--) {
        while (ring_full(&nus_tx))
            ble_nus_flush_tx();
        ring_push(&nus_tx, *buf);
    }
}

bool ble_nus_is_rx_pending(void)
{
    return ring_empty(&nus_rx);
}

// Global Bluetooth Low Energy setup

// Advertising data which needs to stay in scope between connections.
uint8_t ble_adv_len;
uint8_t ble_adv_buf[BLE_ADV_MAX_SIZE];

static inline void ble_adv_add_device_name(const char *name)
{
    ble_adv_buf[ble_adv_len++] = 1 + strlen(name);
    ble_adv_buf[ble_adv_len++] = BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME;
    memcpy(&ble_adv_buf[ble_adv_len], name, strlen(name));
    ble_adv_len += strlen(name);
}

static inline void ble_adv_add_discovery_mode(void)
{
    ble_adv_buf[ble_adv_len++] = 2;
    ble_adv_buf[ble_adv_len++] = BLE_GAP_AD_TYPE_FLAGS;
    ble_adv_buf[ble_adv_len++] = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
}

static inline void ble_adv_add_uuid(ble_uuid_t *uuid)
{
    uint32_t err;
    uint8_t len;
    uint8_t *p_adv_size;

    p_adv_size = &ble_adv_buf[ble_adv_len];
    ble_adv_buf[ble_adv_len++] = 1;
    ble_adv_buf[ble_adv_len++] = BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE;

    err = sd_ble_uuid_encode(uuid, &len, &ble_adv_buf[ble_adv_len]);
    NRFX_ASSERT(!err);
    ble_adv_len += len;
    *p_adv_size += len;
}

static inline void ble_adv_start(void)
{
    uint32_t err;

    ble_gap_adv_data_t adv_data = {
        .adv_data.p_data = ble_adv_buf,
        .adv_data.len = ble_adv_len,
    };

    // Set up advertising parameters
    ble_gap_adv_params_t adv_params = {0};
    adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    adv_params.primary_phy = BLE_GAP_PHY_AUTO;
    adv_params.secondary_phy = BLE_GAP_PHY_AUTO;
    adv_params.interval = (20 * 1000) / 625;

    // Configure the advertising set
    err = sd_ble_gap_adv_set_configure(&ble_adv_handle, &adv_data, &adv_params);
    NRFX_ASSERT(!err);

    // Start the configured BLE advertisement
    err = sd_ble_gap_adv_start(ble_adv_handle, 1);
    NRFX_ASSERT(!err);
}

/**
 * Add rx characteristic to the advertisement.
 */
static void ble_service_add_characteristic_rx(ble_service_t *service, ble_uuid_t *uuid)
{
    uint32_t err;

    ble_gatts_char_md_t rx_char_md = {0};
    rx_char_md.char_props.write = 1;
    rx_char_md.char_props.write_wo_resp = 1;

    ble_gatts_attr_md_t rx_attr_md = {0};
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&rx_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&rx_attr_md.write_perm);
    rx_attr_md.vloc = BLE_GATTS_VLOC_STACK;
    rx_attr_md.vlen = 1;

    ble_gatts_attr_t rx_attr = {0};
    rx_attr.p_uuid = uuid;
    rx_attr.p_attr_md = &rx_attr_md;
    rx_attr.init_len = sizeof(uint8_t);
    rx_attr.max_len = BLE_MAX_MTU_LENGTH - 3;

    err = sd_ble_gatts_characteristic_add(service->handle, &rx_char_md, &rx_attr,
        &service->rx_characteristic);
    NRFX_ASSERT(!err);
}

/**
 * Add tx characteristic to the advertisement.
 */
static void ble_service_add_characteristic_tx(ble_service_t *service, ble_uuid_t *uuid)
{
    uint32_t err;

    ble_gatts_char_md_t tx_char_md = {0};
    tx_char_md.char_props.notify = 1;

    ble_gatts_attr_md_t tx_attr_md = {0};
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&tx_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&tx_attr_md.write_perm);
    tx_attr_md.vloc = BLE_GATTS_VLOC_STACK;
    tx_attr_md.vlen = 1;

    ble_gatts_attr_t tx_attr = {0};
    tx_attr.p_uuid = uuid;
    tx_attr.p_attr_md = &tx_attr_md;
    tx_attr.init_len = sizeof(uint8_t);
    tx_attr.max_len = BLE_MAX_MTU_LENGTH - 3;

    err = sd_ble_gatts_characteristic_add(service->handle, &tx_char_md, &tx_attr,
        &service->tx_characteristic);
    NRFX_ASSERT(!err);
}

static void ble_configure_nus_service(ble_uuid_t *service_uuid)
{
    uint32_t err;
    ble_service_t *service = &ble_nus_service;
    ble_uuid128_t uuid128 = { .uuid128 = {
        // Reverse byte endianess to the string representation.
        0x9E,0xCA,0xDC,0x24,0x0E,0xE5, 0xA9,0xE0, 0x93,0xF3, 0xA3,0xB5, 0x00,0x00,0x40,0x6E
    } };

    // Set the 16 bit UUIDs for the service and characteristics
    service_uuid->uuid = 0x0001;
    ble_uuid_t rx_uuid = { .uuid = 0x0002 };
    ble_uuid_t tx_uuid = { .uuid = 0x0003 };

    err = sd_ble_uuid_vs_add(&uuid128, &service_uuid->type);
    NRFX_ASSERT(!err);

    err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
        service_uuid, &service->handle);

    // Copy the service UUID type to both rx and tx UUID
    rx_uuid.type = service_uuid->type;
    tx_uuid.type = service_uuid->type;

    // Add tx and rx characteristics to the advertisement.
    ble_service_add_characteristic_rx(service, &rx_uuid);
    ble_service_add_characteristic_tx(service, &tx_uuid);
}

void ble_configure_raw_service(ble_uuid_t *service_uuid)
{
    uint32_t err;
    ble_service_t *service = &ble_raw_service;

    ble_uuid128_t uuid128 = { .uuid128 = {
        // Reverse byte endianess to the string representation.
        0xFF,0xCA,0xDC,0x24,0x0E,0xE5, 0xA9,0xE0, 0x93,0xF3, 0xA3,0xB5, 0x00,0x00,0x40,0x6E
    } };

    // Set the 16 bit UUIDs for the service and characteristics
    service_uuid->uuid = 0x0001;
    ble_uuid_t rx_uuid = { .uuid = 0x3002 };
    ble_uuid_t tx_uuid = { .uuid = 0x3003 };

    err = sd_ble_uuid_vs_add(&uuid128, &service_uuid->type);
    NRFX_ASSERT(!err);

    err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
        service_uuid, &service->handle);

    // Copy the service UUID type to both rx and tx UUID
    rx_uuid.type = service_uuid->type;
    tx_uuid.type = service_uuid->type;

    // Add tx and rx characteristics to the advertisement.
    ble_service_add_characteristic_rx(service, &rx_uuid);
    ble_service_add_characteristic_tx(service, &tx_uuid);
}

/**
 * Setup BLE parameters adapted to this driver.
 */
void ble_configure_softdevice(void)
{
    uint32_t err;

    // Add GAP configuration to the BLE stack
    ble_cfg_t cfg;
    cfg.conn_cfg.conn_cfg_tag = 1;
    cfg.conn_cfg.params.gap_conn_cfg.conn_count = 1;
    cfg.conn_cfg.params.gap_conn_cfg.event_length = 3;
    err = sd_ble_cfg_set(BLE_CONN_CFG_GAP, &cfg, ram_start);
    NRFX_ASSERT(!err);

    // Set BLE role to peripheral only
    memset(&cfg, 0, sizeof(cfg));
    cfg.gap_cfg.role_count_cfg.periph_role_count = 1;
    err = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &cfg, ram_start);
    NRFX_ASSERT(!err);

    // Set max MTU size
    memset(&cfg, 0, sizeof(cfg));
    cfg.conn_cfg.conn_cfg_tag = 1;
    cfg.conn_cfg.params.gatt_conn_cfg.att_mtu = BLE_MAX_MTU_LENGTH;
    err = sd_ble_cfg_set(BLE_CONN_CFG_GATT, &cfg, ram_start);
    NRFX_ASSERT(!err);

    // Configure a single queued transfer
    memset(&cfg, 0, sizeof(cfg));
    cfg.conn_cfg.conn_cfg_tag = 1;
    cfg.conn_cfg.params.gatts_conn_cfg.hvn_tx_queue_size = 1;
    err = sd_ble_cfg_set(BLE_CONN_CFG_GATTS, &cfg, ram_start);
    NRFX_ASSERT(!err);

    // Configure number of custom UUIDs
    memset(&cfg, 0, sizeof(cfg));
    cfg.common_cfg.vs_uuid_cfg.vs_uuid_count = 2;
    err = sd_ble_cfg_set(BLE_COMMON_CFG_VS_UUID, &cfg, ram_start);
    NRFX_ASSERT(!err);

    // Configure GATTS attribute table
    memset(&cfg, 0, sizeof(cfg));
    cfg.gatts_cfg.attr_tab_size.attr_tab_size = 1408;
    err = sd_ble_cfg_set(BLE_GATTS_CFG_ATTR_TAB_SIZE, &cfg, ram_start);
    NRFX_ASSERT(!err);

    // No service changed attribute needed
    memset(&cfg, 0, sizeof(cfg));
    cfg.gatts_cfg.service_changed.service_changed = 0;
    err = sd_ble_cfg_set(BLE_GATTS_CFG_SERVICE_CHANGED, &cfg, ram_start);
    NRFX_ASSERT(!err);
}

/**
 * Softdevice assert handler. Called whenever softdevice crashes.
 */
static void softdevice_assert_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    assert(id == 0);
}

/**
 * Initialise the bluetooth low energy driver.
 * Initialises the softdevice and Bluetooth functionality.
 * It features a single GATT profile for UART communication, used by the REPL.
 */
void ble_init(void)
{
    // Error code variable
    uint32_t err;

    // Init LF clock
    nrf_clock_lf_cfg_t clock_config = {
        .source = NRF_CLOCK_LF_SRC_XTAL,
        .rc_ctiv = 0,
        .rc_temp_ctiv = 0,
        .accuracy = NRF_CLOCK_LF_ACCURACY_10_PPM
    };

    // Enable the softdevice
    err = sd_softdevice_enable(&clock_config, softdevice_assert_handler);
    NRFX_ASSERT(!err);

    // Enable softdevice interrupt
    err = sd_nvic_EnableIRQ((IRQn_Type)SD_EVT_IRQn);
    NRFX_ASSERT(!err);

    // Enable the DC-DC convertor
    err = sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
    NRFX_ASSERT(!err);

    // Set configuration parameters for the SoftDevice suitable for this code.
    ble_configure_softdevice();

    // Start bluetooth. `ram_start` is the address of a variable containing an address, defined in the linker script.
    // It updates that address with another one planning ahead the RAM needed by the softdevice.
    err = sd_ble_enable(&ram_start);
    NRFX_ASSERT(!err);

    // Set security to open
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    // Set device name. Last four characters are taken from MAC address ;
    err = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)BLE_DEVICE_NAME, sizeof BLE_DEVICE_NAME - 1);
    NRFX_ASSERT(!err);

    // Set connection parameters
    ble_gap_conn_params_t gap_conn_params = {0};
    gap_conn_params.min_conn_interval = (15 * 1000) / 1250;
    gap_conn_params.max_conn_interval = (15 * 1000) / 1250;
    gap_conn_params.slave_latency = 3;
    gap_conn_params.conn_sup_timeout = (2000 * 1000) / 10000;
    err = sd_ble_gap_ppcp_set(&gap_conn_params);
    NRFX_ASSERT(!err);

    // Add name to advertising payload
    ble_adv_add_device_name(BLE_DEVICE_NAME);

    // Set discovery mode flag
    ble_adv_add_discovery_mode();

    ble_uuid_t nus_service_uuid, raw_service_uuid;

    // Configure the Nordic UART Service (NUS) and custom "raw" service.
    ble_configure_nus_service(&nus_service_uuid);
    ble_configure_raw_service(&raw_service_uuid);

    // Add only the Nordic UART Service to the advertisement.
    ble_adv_add_uuid(&nus_service_uuid);

    // Submit the adv now that it is complete.
    ble_adv_start();
}

/**
 * BLE event handler.
 */
void SWI2_IRQHandler(void)
{
    uint32_t evt_id;
    uint8_t ble_evt_buffer[sizeof(ble_evt_t) + BLE_MAX_MTU_LENGTH];

    // While any softdevice events are pending, service flash operations
    while (sd_evt_get(&evt_id) != NRF_ERROR_NOT_FOUND)
    {
        switch (evt_id)
        {
        case NRF_EVT_FLASH_OPERATION_SUCCESS:
            // TODO In case we add a filesystem in the future
            break;

        case NRF_EVT_FLASH_OPERATION_ERROR:
            // TODO In case we add a filesystem in the future
            break;

        default:
            break;
        }
    }

    // While any BLE events are pending
    while (1)
    {
        uint32_t err;

        // Pull an event from the queue
        uint16_t buffer_len = sizeof(ble_evt_buffer);
        uint32_t status = sd_ble_evt_get(ble_evt_buffer, &buffer_len);

        // If we get the done status, we can exit the handler
        if (status == NRF_ERROR_NOT_FOUND)
            break;

        // Check for other errors
        NRFX_ASSERT(status == 0);

        // Make a pointer from the buffer which we can use to find the event
        ble_evt_t *ble_evt = (ble_evt_t *)ble_evt_buffer;

        // Otherwise on NRF_SUCCESS, we service the new event
        volatile uint16_t ble_evt_id = ble_evt->header.evt_id;
        switch (ble_evt_id)
        {

        // When connected
        case BLE_GAP_EVT_CONNECTED:
        {
            NRFX_ASSERT(ble_conn_handle == BLE_CONN_HANDLE_INVALID);

            // Set the connection service
            ble_conn_handle = ble_evt->evt.gap_evt.conn_handle;

            // Update connection parameters
            ble_gap_conn_params_t conn_params;

            err = sd_ble_gap_ppcp_get(&conn_params);
            NRFX_ASSERT(!err);

            err = sd_ble_gap_conn_param_update(ble_conn_handle, &conn_params);
            NRFX_ASSERT(!err);

            err = sd_ble_gatts_sys_attr_set(ble_conn_handle, NULL, 0, 0);
            NRFX_ASSERT(!err);
            break;
        }

        // When disconnected
        case BLE_GAP_EVT_DISCONNECTED:
        {
            NRFX_ASSERT(ble_evt->evt.gap_evt.conn_handle == ble_conn_handle);

            // Clear the connection service
            ble_conn_handle = BLE_CONN_HANDLE_INVALID;

            // Start advertising
            err = sd_ble_gap_adv_start(ble_adv_handle, 1);
            NRFX_ASSERT(!err);
            break;
        }

        // On a phy update request, set the phy speed automatically
        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRFX_ASSERT(ble_evt->evt.gap_evt.conn_handle == ble_conn_handle);

            ble_gap_phys_t const phys = {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err = sd_ble_gap_phy_update(ble_evt->evt.gap_evt.conn_handle, &phys);
            NRFX_ASSERT(!err);
            break;
        }

        // Handle requests for changing MTU length
        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
        {
            NRFX_ASSERT(ble_evt->evt.gap_evt.conn_handle == ble_conn_handle);

            // The client's desired MTU size
            uint16_t client_mtu =
                ble_evt->evt.gatts_evt.params.exchange_mtu_request.client_rx_mtu;

            // Respond with our max MTU size
            err = sd_ble_gatts_exchange_mtu_reply(ble_conn_handle, BLE_MAX_MTU_LENGTH);
            NRFX_ASSERT(!err);

            // Choose the smaller MTU as the final length we'll use
            // -3 bytes to accommodate for Op-code and attribute service
            negotiated_mtu = BLE_MAX_MTU_LENGTH < client_mtu
                                 ? BLE_MAX_MTU_LENGTH - 3
                                 : client_mtu - 3;
            break;
        }

        // When data arrives, we can write it to the buffer
        case BLE_GATTS_EVT_WRITE:
        {
            NRFX_ASSERT(ble_evt->evt.gap_evt.conn_handle == ble_conn_handle);
            // For the entire incoming string
            for (uint16_t length = 0;
                 length < ble_evt->evt.gatts_evt.params.write.len;
                 length++)
            {
                // Break if the ring buffer is full, we can't write more
                if (ring_full(&nus_rx))
                    break;

                // Copy a character into the ring buffer
                ring_push(&nus_rx, ble_evt->evt.gatts_evt.params.write.data[length]);
            }
            break;
        }

        // Disconnect on GATT Client timeout
        case BLE_GATTC_EVT_TIMEOUT:
        {
            NRFX_ASSERT(!"not reached");
            break;
        }

        // Disconnect on GATT Server timeout
        case BLE_GATTS_EVT_TIMEOUT:
        {
            NRFX_ASSERT(ble_evt->evt.gap_evt.conn_handle == ble_conn_handle);
            err = sd_ble_gap_disconnect(ble_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            NRFX_ASSERT(!err);
            break;
        }

        // Updates system attributes after a new connection event
        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
        {
            NRFX_ASSERT(ble_evt->evt.gap_evt.conn_handle == ble_conn_handle);
            err = sd_ble_gatts_sys_attr_set(ble_conn_handle, NULL, 0, 0);
            NRFX_ASSERT(!err);
            break;
        }

        // We don't support pairing, so reply with that message
        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
        {
            NRFX_ASSERT(ble_evt->evt.gap_evt.conn_handle == ble_conn_handle);
            err = sd_ble_gap_sec_params_reply(ble_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            NRFX_ASSERT(!err);
            break;
        }

        case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
        {
            NRFX_ASSERT(ble_evt->evt.gap_evt.conn_handle == ble_conn_handle);
            err = sd_ble_gap_data_length_update(ble_conn_handle, NULL, NULL);
            NRFX_ASSERT(!err);
            break;
        }

        case BLE_GAP_EVT_SEC_INFO_REQUEST:
        {
            NRFX_ASSERT(ble_evt->evt.gap_evt.conn_handle == ble_conn_handle);
            err = sd_ble_gap_sec_info_reply(ble_conn_handle, NULL, NULL, NULL);
            NRFX_ASSERT(!err);
            break;
        }

        case BLE_GAP_EVT_SEC_REQUEST:
        {
            NRFX_ASSERT(ble_evt->evt.gap_evt.conn_handle == ble_conn_handle);
            err = sd_ble_gap_authenticate(ble_conn_handle, NULL);
            NRFX_ASSERT(!err);
            break;
        }

        case BLE_GAP_EVT_AUTH_KEY_REQUEST:
        {
            NRFX_ASSERT(ble_evt->evt.gap_evt.conn_handle == ble_conn_handle);
            err = sd_ble_gap_auth_key_reply(ble_conn_handle, BLE_GAP_AUTH_KEY_TYPE_NONE, NULL);
            NRFX_ASSERT(!err);
            break;
        }

        case BLE_EVT_USER_MEM_REQUEST:
        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
        {
            NRFX_ASSERT(!"only expected on Bluetooth Centrals, not on Peripherals");
            break;
        }

        default:
        {
            // ignore unused events
            break;
        }
        }
    }
}
