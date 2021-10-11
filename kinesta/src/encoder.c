#include "encoder.h"
#include <drivers/i2c.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(rgb_encoder);

#define COLOR_CHAN_RSHIFT (COLOR_CHAN_BITS - 8)

#define REG_GCONF  0x00
#define REG_RLED   0x18
#define REG_IDCODE 0x70

#define BIT_GCONF_ETYPE (1 << 5)
#define BIT_GCONF_RESET (1 << 7)

static inline int encoder_i2c_read(const encoder *enc, uint8_t reg, uint8_t *data, size_t size)
{
    int ret = i2c_burst_read(enc->i2c, enc->addr, reg, data, size);
    if (ret){
        LOG_ERR("[%02X] I2C read of register %02X with size %d failed: %d",
                (int) enc->addr, (int) reg, size, ret);
    }
    return ret;
}

static inline int encoder_i2c_write(const encoder *enc, uint8_t reg, const uint8_t *data, size_t size)
{
    int ret = i2c_burst_write(enc->i2c, enc->addr, reg, data, size);
    if (ret){
        LOG_ERR("[%02X] I2C read of register %02X with size %d failed: %d",
                (int) enc->addr, (int) reg, size, ret);
    }
    return ret;
}

static inline int encoder_i2c_write_byte(const encoder *enc, uint8_t reg, uint8_t data)
{
    return encoder_i2c_write(enc, reg, &data, 1);
}

int encoder_init(const encoder *enc)
{
    // 1. Check device ID
    uint8_t idcode[2];
    int ret = encoder_i2c_read(enc, REG_IDCODE, idcode, 2);
    if (ret){
        return ret;
    }
    if (idcode[0] != 0x53 || idcode[1] != 0x23){
        LOG_ERR("Invalid RGB I2C encoder device identifier: %02X, %02X\n",
                (int) idcode[0], (int) idcode[1]);
        return -1;
    }

    // 2. Reset
    ret = encoder_i2c_write_byte(enc, REG_GCONF, BIT_GCONF_RESET);
    if (ret){
        return ret;
    }

    k_sleep(K_MSEC(1));

    // 3. Configure as RGB illuminated encoder
    ret = encoder_i2c_write_byte(enc, REG_GCONF, BIT_GCONF_ETYPE);
    if (ret){
        return ret;
    }

    return 0;
}

int encoder_set_color(const encoder *enc, color_t color)
{
    uint8_t rgb_value[3] = {
        COLOR_GET_R(color) >> COLOR_CHAN_RSHIFT,
        COLOR_GET_G(color) >> COLOR_CHAN_RSHIFT,
        COLOR_GET_B(color) >> COLOR_CHAN_RSHIFT,
    };
    return encoder_i2c_write(enc, REG_RLED, rgb_value, 3);
}
