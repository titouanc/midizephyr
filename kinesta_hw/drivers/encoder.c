#include "encoder.h"

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(rgb_encoder);

#define DT_DRV_COMPAT duppa_i2cencoderv21

#define COLOR_CHAN_RSHIFT (COLOR_CHAN_BITS - 8)

#define REG_GCONF    0x00
#define REG_INTCONF  0x04
#define REG_ESTATUS  0x05
#define REG_CVAL0    0x08
#define REG_CVAL1    0x09
#define REG_CVAL2    0x0A
#define REG_CVAL3    0x0B
#define REG_CMAX0    0x0C
#define REG_CMAX1    0x0D
#define REG_CMAX2    0x0E
#define REG_CMAX3    0x0F
#define REG_CMIN0    0x10
#define REG_CMIN1    0x11
#define REG_CMIN2    0x12
#define REG_CMIN3    0x13
#define REG_ISTEP0   0x14
#define REG_ISTEP1   0x15
#define REG_ISTEP2   0x16
#define REG_ISTEP3   0x17
#define REG_RLED     0x18
#define REG_DPPERIOD 0x1F
#define REG_IDCODE   0x70

#define BIT_GCONF_DTYPE (1 << 0)
#define BIT_GCONF_ETYPE (1 << 5)
#define BIT_GCONF_RESET (1 << 7)

/* Same bits defined in ESTATUS and INTCONF */
#define BIT_ESTATUS_PUSHR (1 << 0)
#define BIT_ESTATUS_PUSHP (1 << 1)
#define BIT_ESTATUS_PUSHD (1 << 2)
#define BIT_ESTATUS_IRINC (1 << 3)
#define BIT_ESTATUS_IRDEC (1 << 4)

struct encoder_config {
    struct i2c_dt_spec i2c;
    struct gpio_dt_spec interrupt;
};

struct encoder_data {
    const struct device *dev;
    struct gpio_callback int_callback;
    struct k_work handler_work;
    struct encoder_callback_t *user_callback;
};

static inline int encoder_i2c_read(const struct device *dev, uint8_t reg, uint8_t *data, size_t size)
{
    const struct encoder_config const *config = dev->config;
    int ret = i2c_burst_read(config->i2c.bus, config->i2c.addr, reg, data, size);
    if (ret){
        LOG_ERR("[%s] I2C read of register 0x%02X with size %d failed: %d",
                dev->name, (int) reg, size, ret);
    }
    return ret;
}

static inline int encoder_i2c_write(const struct device *dev, uint8_t reg, const uint8_t *data, size_t size)
{
    const struct encoder_config const *config = dev->config;
    int ret = i2c_burst_write(config->i2c.bus, config->i2c.addr, reg, data, size);
    if (ret){
        LOG_ERR("[%s] I2C write of register 0x%02X with size %d failed: %d",
                dev->name, (int) reg, size, ret);
    }
    return ret;
}

static inline int encoder_i2c_write_byte(const struct device *dev, uint8_t reg, uint8_t data)
{
    return encoder_i2c_write(dev, reg, &data, 1);
}

static inline int encoder_i2c_read_byte(const struct device *dev, uint8_t reg, uint8_t *data)
{
    return encoder_i2c_read(dev, reg, data, 1);
}

static inline int encoder_i2c_write_float(const struct device *dev, uint8_t reg, float data)
{
    uint32_t data_endian = sys_cpu_to_be32(*((uint32_t *) &data));
    return encoder_i2c_write(dev, reg, (const uint8_t *) &data_endian, 4);
}

static inline int encoder_i2c_read_float(const struct device *dev, uint8_t reg, float *data)
{
    uint32_t data_endian;
    uint32_t *data_aliased = (uint32_t *) data;
    int ret = encoder_i2c_read(dev, reg, (uint8_t *) &data_endian, 4);
    if (ret){
        return ret;
    }
    *data_aliased = sys_be32_to_cpu(data_endian);
    return 0;
}

static void encoder_call_user_handler(struct k_work *work)
{
    const struct encoder_data *drv_data = CONTAINER_OF(work, struct encoder_data, handler_work);
    int evt;
    int r = encoder_get_event(drv_data->dev, &evt);
    if (r){
        LOG_ERR("[%s] Error when fetching event in callback", drv_data->dev->name);
        return;
    }
    if (evt){
        drv_data->user_callback->func(drv_data->user_callback, evt);
    }
}

static void encoder_interrupt_handler(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
    struct encoder_data *drv_data = CONTAINER_OF(cb, struct encoder_data, int_callback);
    if (drv_data->user_callback){
        k_work_submit(&drv_data->handler_work);
    }
}

static int encoder_init(const struct device *dev)
{
    const struct encoder_config *const config = dev->config;
    struct encoder_data *drv_data = dev->data;
    drv_data->dev = dev;

    // 1. Check device ID
    uint8_t idcode[2];
    int ret = encoder_i2c_read(dev, REG_IDCODE, idcode, 2);
    if (ret){
        return ret;
    }
    if (idcode[0] != 0x53 || idcode[1] != 0x23){
        LOG_ERR("Invalid RGB I2C encoder device identifier: %02X, %02X\n",
                (int) idcode[0], (int) idcode[1]);
        return -1;
    }

    // 2. Reset
    ret = encoder_i2c_write_byte(dev, REG_GCONF, BIT_GCONF_RESET);
    if (ret){
        return ret;
    }

    k_sleep(K_MSEC(1));

    // 3. Configure as RGB illuminated encoder in float32
    ret = encoder_i2c_write_byte(dev, REG_GCONF, BIT_GCONF_ETYPE | BIT_GCONF_DTYPE);
    if (ret){
        return ret;
    }

    // 4. Set encoder range
    ret = encoder_i2c_write_float(dev, REG_CMAX0, 1.0f);
    if (ret){
        return ret;
    }
    ret = encoder_i2c_write_float(dev, REG_CMIN0, 0.0f);
    if (ret){
        return ret;
    }
    ret = encoder_i2c_write_float(dev, REG_ISTEP0, 1.0f / CONFIG_KINESTA_HW_ENCODER_STEPS);
    if (ret){
        return ret;
    }

    // 5. Configure events (click and double-click)
    ret = encoder_i2c_write_byte(dev, REG_INTCONF, BIT_ESTATUS_PUSHP | BIT_ESTATUS_PUSHR | BIT_ESTATUS_PUSHD | BIT_ESTATUS_IRINC | BIT_ESTATUS_IRDEC);
    if (ret){
        return ret;
    }

    // 6. Configure interrupt if defined in the device tree
    if (config->interrupt.port){
        k_work_init(&drv_data->handler_work, encoder_call_user_handler);
        gpio_pin_configure_dt(&config->interrupt, GPIO_INPUT);
        gpio_pin_interrupt_configure_dt(&config->interrupt, GPIO_INT_EDGE_TO_ACTIVE);
        gpio_init_callback(&drv_data->int_callback, encoder_interrupt_handler, BIT(config->interrupt.pin));
        gpio_add_callback(config->interrupt.port, &drv_data->int_callback);
    }

    return 0;
}

int encoder_set_color(const struct device *dev, color_t color)
{
    uint8_t rgb_value[3];
    color_get_u8(color, &rgb_value[0], &rgb_value[1], &rgb_value[2]);
    return encoder_i2c_write(dev, REG_RLED, rgb_value, 3);
}

int encoder_get_value(const struct device *dev, float *value)
{
    return encoder_i2c_read_float(dev, REG_CVAL0, value);
}

int encoder_set_value(const struct device *dev, float value)
{
    return encoder_i2c_write_float(dev, REG_CVAL0, value);
}

int encoder_get_event(const struct device *dev, int *evt)
{
    int evt_value = 0;
    uint8_t estatus;
    int ret = encoder_i2c_read_byte(dev, REG_ESTATUS, &estatus);
    if (ret){
        return ret;
    }

    if (estatus & BIT_ESTATUS_PUSHD){
        evt_value |= ENCODER_EVT_DOUBLE_CLICK;
    }
    if (estatus & BIT_ESTATUS_PUSHR){
        evt_value |= ENCODER_EVT_RELEASE;
    }
    if (estatus & BIT_ESTATUS_PUSHP){
        evt_value |= ENCODER_EVT_PRESS;
    }
    if ((estatus & BIT_ESTATUS_IRDEC) || (estatus & BIT_ESTATUS_IRINC)){
        evt_value |= ENCODER_EVT_VALUE_CHANGED;
    }
    *evt = evt_value;
    return 0;
}

void encoder_set_callback(const struct device *dev, struct encoder_callback_t *cb)
{
    struct encoder_data *const drv_data = dev->data;
    drv_data->user_callback = cb;
}

#define ENCODER_INIT(inst)                                              \
    static const struct encoder_config encoder_##inst##_config = {      \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                              \
        .interrupt = GPIO_DT_SPEC_INST_GET(inst, interrupt_gpios)       \
    };                                                                  \
                                                                        \
    static struct encoder_data encoder_##inst##_data;                   \
                                                                        \
    DEVICE_DT_INST_DEFINE(inst, encoder_init, NULL,                     \
                  &encoder_##inst##_data,                               \
                  &encoder_##inst##_config,                             \
                  POST_KERNEL,                                          \
                  CONFIG_I2C_INIT_PRIORITY,                             \
                  NULL);

DT_INST_FOREACH_STATUS_OKAY(ENCODER_INIT)
