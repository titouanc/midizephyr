#include "encoder.h"
#include <drivers/i2c.h>
#include <sys/byteorder.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(rgb_encoder);

#define ENCODER_N_STEPS 16

#define COLOR_CHAN_RSHIFT (COLOR_CHAN_BITS - 8)

#define REG_GCONF  0x00
#define REG_CVAL0  0x08
#define REG_CVAL1  0x09
#define REG_CVAL2  0x0A
#define REG_CVAL3  0x0B
#define REG_CMAX0  0x0C
#define REG_CMAX1  0x0D
#define REG_CMAX2  0x0E
#define REG_CMAX3  0x0F
#define REG_CMIN0  0x10
#define REG_CMIN1  0x11
#define REG_CMIN2  0x12
#define REG_CMIN3  0x13
#define REG_ISTEP0 0x14
#define REG_ISTEP1 0x15
#define REG_ISTEP2 0x16
#define REG_ISTEP3 0x17
#define REG_RLED   0x18
#define REG_IDCODE 0x70

#define BIT_GCONF_DTYPE (1 << 0)
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

static inline int encoder_i2c_write_float(const encoder *enc, uint8_t reg, float data)
{
    uint32_t data_endian = sys_cpu_to_be32(*((uint32_t *) &data));
    return encoder_i2c_write(enc, reg, (const uint8_t *) &data_endian, 4);
}

static inline int encoder_i2c_read_float(const encoder *enc, uint8_t reg, float *data)
{
    uint32_t data_endian;
    uint32_t *data_aliased = (uint32_t *) data;
    int ret = encoder_i2c_read(enc, reg, (uint8_t *) &data_endian, 4);
    if (ret){
        return ret;
    }
    *data_aliased = sys_be32_to_cpu(data_endian);
    return 0;
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

    // 3. Configure as RGB illuminated encoder in float32
    ret = encoder_i2c_write_byte(enc, REG_GCONF, BIT_GCONF_ETYPE | BIT_GCONF_DTYPE);
    if (ret){
        return ret;
    }

    // 4. Set encoder range
    ret = encoder_i2c_write_float(enc, REG_CMAX0, 1.0f);
    if (ret){
        return ret;
    }
    ret = encoder_i2c_write_float(enc, REG_CMIN0, 0.0f);
    if (ret){
        return ret;
    }
    ret = encoder_i2c_write_float(enc, REG_ISTEP0, 1.0f / ENCODER_N_STEPS);
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

int encoder_get_value(const encoder *enc, float *value)
{
    return encoder_i2c_read_float(enc, REG_CVAL0, value);
}
