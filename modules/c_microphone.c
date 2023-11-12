#include "c_microphone.h"

static uint8_t microphone_bit_depth = 16;

static inline void microphone_fpga_read(uint16_t address, uint8_t *buffer, size_t length)
{
    uint8_t address_bytes[2] = {(uint8_t)(address >> 8), (uint8_t)address};

    monocle_spi_write(FPGA, address_bytes, 2, true);

    // Dump the data into a dummy buffer if a buffer isn't provided
    if (buffer == NULL)
    {
        uint8_t dummy_buffer[254];
        monocle_spi_read(FPGA, dummy_buffer, length, false);
        return;
    }

    monocle_spi_read(FPGA, buffer, length, false);
}

static inline void microphone_fpga_write(uint16_t address, uint8_t *buffer, size_t length)
{
    uint8_t address_bytes[2] = {(uint8_t)(address >> 8), (uint8_t)address};

    if (buffer == NULL || length == 0)
    {
        monocle_spi_write(FPGA, address_bytes, 2, false);
        return;
    }

    monocle_spi_write(FPGA, address_bytes, 2, true);
    monocle_spi_write(FPGA, buffer, length, false);
}

static size_t microphone_bytes_available(void)
{
    uint8_t available_bytes[2] = {0, 0};
    microphone_fpga_read(0x5801, available_bytes, sizeof(available_bytes));
    size_t available = (available_bytes[0] << 8 | available_bytes[1]) * 2;

    // Cap to 254 due to SPI DMA limit
    if (available > 254)
    {
        available = 254;
    }

    return available;
}

static int microphone_init(void)
{
    uint8_t fpga_image[4];
    uint8_t module_status[2];

    microphone_fpga_read(0x0001, fpga_image, sizeof(fpga_image));
    microphone_fpga_read(0x5800, module_status, sizeof(module_status));

    if (((module_status[0] & 0x10) != 16) ||
        memcmp(fpga_image, "Mncl", sizeof(fpga_image)))
    {
        return RESULT_ERR;
    }

    return RESULT_OK;
}

static int microphone_record(float seconds, uint16_t bit_depth, uint16_t sample_rate)
{
    // Flush existing data
    while (true)
    {
        size_t available = microphone_bytes_available();

        if (available == 0)
        {
            break;
        }

        microphone_fpga_read(0x5807, NULL, available);
    }

    // Check the given sample rate
    if (sample_rate != 16000 && sample_rate != 8000)
    {
        return RESULT_ERR;
    }

    // Check the currently set sample rate on the FPGA
    uint8_t status_byte;
    microphone_fpga_read(0x0800, &status_byte, sizeof(status_byte));

    // Toggle the sample rate if required
    if (((status_byte & 0x04) == 0x00 && sample_rate == 8000) ||
        ((status_byte & 0x04) == 0x04 && sample_rate == 16000))
    {
        microphone_fpga_write(0x0808, NULL, 0);
    }

    // Check and set bit depth to the global variable
    if (bit_depth != 16 && bit_depth != 8)
    {
        return RESULT_ERR;
    }
    microphone_bit_depth = bit_depth;

    // Set the block size and request a number of blocks corresponding to seconds
    float block_size;
    sample_rate == 16000 ? (block_size = 0.02) : (block_size = 0.04);

    uint16_t blocks = (uint16_t)(seconds / block_size);
    uint8_t blocks_bytes[] = {blocks >> 8, blocks};
    microphone_fpga_write(0x0802, blocks_bytes, sizeof(blocks));

    // Trigger capture
    microphone_fpga_write(0x0803, NULL, 0);

    return RESULT_OK;
}

static int microphone_stop(void)
{
    return RESULT_ERR;
}

static int microphone_read(int samples, uint8_t* buffer, size_t buffer_size)
{
    if (samples > 127)
    {
        return RESULT_ERR;
    }

    size_t available = microphone_bytes_available();

    if (available == 0)
    {
        return RESULT_OK;
    }

    if (samples * 2 < available)
    {
        available = samples * 2;
    }

    size_t const max_size = (available < buffer_size) ? available : buffer_size;
    microphone_fpga_read(0x5807, buffer, max_size); // FPGA only stores uint8_t data - reconfigure?

    // OK if 16 bit data
    if (microphone_bit_depth == 16)
    {
        return RESULT_OK;
    }

    // ERR if 8 bit data. Support for 8-bit scaled version not implemented
    return RESULT_ERR;
}