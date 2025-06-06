/**
 *
 * @license MIT License
 *
 * Copyright (c) 2025 lewis he
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * @file      i2c_driver.cpp
 * @author    Lewis He (lewishe@outlook.com)
 * @date      2025-01-19
 *
 */
#include "i2c_driver.h"

#if CONFIG_I2C_COMMUNICATION_METHOD_BUILTIN_RW || CONFIG_I2C_COMMUNICATION_METHOD_CALLBACK_RW

static const char *TAG = "I2C_DRIVER";

#define I2C_MASTER_NUM (i2c_port_t) CONFIG_I2C_MASTER_PORT_NUM
#define I2C_MASTER_SDA_IO (gpio_num_t) CONFIG_PMU_I2C_SDA // CONFIG_SENSOR_SDA
#define I2C_MASTER_SCL_IO (gpio_num_t) CONFIG_PMU_I2C_SCL // CONFIG_SENSOR_SCL
#define I2C_MASTER_FREQ_HZ CONFIG_I2C_MASTER_FREQUENCY    /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0                       /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0                       /*!< I2C master doesn't need buffer */

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)
#include "soc/clk_tree_defs.h"

i2c_master_dev_handle_t i2c_device;

// * Using the new API of esp-idf 5.x, you need to pass the I2C BUS handle,
// * which is useful when the bus shares multiple devices.
i2c_master_bus_handle_t bus_handle;
#endif

#if CONFIG_I2C_COMMUNICATION_METHOD_CALLBACK_RW

bool i2c_wr_function(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len, bool writeReg, bool isWrite)
{
    if (isWrite)
    {
        esp_err_t ret;
        if (buf == NULL)
        {
            return false;
        }

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)

        if (writeReg)
        {
            uint8_t *write_buffer = (uint8_t *)malloc(sizeof(uint8_t) * (len + 1));
            if (!write_buffer)
            {
                return -1;
            }
            write_buffer[0] = reg;
            memcpy(write_buffer + 1, buf, len);

            ret = i2c_master_transmit(
                i2c_device,
                write_buffer,
                len + 1,
                -1);
            free(write_buffer);
        }
        else
        {
            ret = i2c_master_transmit(i2c_device, buf, len, -1);
        }

#else

        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        if (writeReg)
        {
            i2c_master_write_byte(cmd, reg, true);
        }
        i2c_master_write(cmd, buf, len, true);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdTICKS_TO_MS(1000));
        i2c_cmd_link_delete(cmd);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "i2c_write_error.");
        }

#endif
        return ret == ESP_OK ? true : false;
    }
    else
    {
        esp_err_t ret;
        if (len == 0)
        {
            return true;
        }
        if (buf == NULL)
        {
            return false;
        }

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)

        ret = i2c_master_transmit_receive(i2c_device, (const uint8_t *)&reg, 1, buf, len, -1);

#else

        i2c_cmd_handle_t cmd;
        if (writeReg)
        {
            cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
            i2c_master_write_byte(cmd, reg, true);
            i2c_master_stop(cmd);
            ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdTICKS_TO_MS(1000));
            i2c_cmd_link_delete(cmd);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "i2c_master_cmd_begin failed!");
                return false;
            }
        }

        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
        if (len > 1)
        {
            i2c_master_read(cmd, buf, len - 1, I2C_MASTER_ACK);
        }
        i2c_master_read_byte(cmd, &buf[len - 1], I2C_MASTER_NACK);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdTICKS_TO_MS(1000));
        i2c_cmd_link_delete(cmd);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "i2c_read_error.");
        }
#endif
        return ret == ESP_OK ? true : false;
    }
}

/**
 * @brief Read a sequence of bytes from a pmu registers
 */
int i2c_read_callback(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
{
    esp_err_t ret;
    if (len == 0)
    {
        return ESP_OK;
    }
    if (data == NULL)
    {
        return ESP_FAIL;
    }

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)

    ret = i2c_master_transmit_receive(
        i2c_device,
        (const uint8_t *)&regAddr,
        1,
        data,
        len,
        -1);
#else

    i2c_cmd_handle_t cmd;

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, regAddr, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdTICKS_TO_MS(1000));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_master_cmd_begin failed!");
        return ESP_FAIL;
    }
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_READ, true);
    if (len > 1)
    {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &data[len - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdTICKS_TO_MS(1000));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_read_error.");
    }
#endif
    return ret == ESP_OK ? 0 : -1;
}

/**
 * @brief Write a byte to a pmu register
 */
int i2c_write_callback(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
{
    esp_err_t ret;
    if (data == NULL)
    {
        return ESP_FAIL;
    }

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)
    uint8_t *write_buffer = (uint8_t *)malloc(sizeof(uint8_t) * (len + 1));
    if (!write_buffer)
    {
        return -1;
    }
    write_buffer[0] = regAddr;
    memcpy(write_buffer + 1, data, len);

    ret = i2c_master_transmit(
        i2c_device,
        write_buffer,
        len + 1,
        -1);
    free(write_buffer);
#else
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, regAddr, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdTICKS_TO_MS(1000));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_write_error.");
    }
#endif
    return ret == ESP_OK ? 0 : -1;
}

esp_err_t i2c_drv_device_init(uint8_t address)
{
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)
    i2c_device_config_t i2c_dev_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    return i2c_master_bus_add_device(bus_handle, &i2c_dev_conf, &i2c_device);
#else
    return ESP_OK;
#endif
}

#endif // CONFIG_I2C_COMMUNICATION_METHOD_CALLBACK_RW

/**
 * @brief i2c master initialization
 */
esp_err_t i2c_drv_init(void)
{
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)

    ESP_LOGI(TAG, "Implemented using read and write callback methods (Use higher version >= 5.0 API)");

    i2c_master_bus_config_t i2c_bus_config;
    memset(&i2c_bus_config, 0, sizeof(i2c_bus_config));
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_config.i2c_port = I2C_MASTER_NUM;
    i2c_bus_config.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    i2c_bus_config.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    i2c_bus_config.glitch_ignore_cnt = 7;
    i2c_bus_config.flags.enable_internal_pullup = true;

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    return ESP_OK;
#else

    ESP_LOGI(TAG, "Implemented using read and write callback methods (Use lower version < 5.0 API)");

    i2c_config_t i2c_conf;
    memset(&i2c_conf, 0, sizeof(i2c_conf));
    i2c_conf.mode = I2C_MODE_MASTER;
    i2c_conf.sda_io_num = I2C_MASTER_SDA_IO;
    i2c_conf.scl_io_num = I2C_MASTER_SCL_IO;
    i2c_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(I2C_MASTER_NUM, &i2c_conf);
    return i2c_driver_install(I2C_MASTER_NUM, i2c_conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
#endif
}

void i2c_drv_scan()
{
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)

    esp_err_t err = ESP_OK;
    uint8_t address = 0x00;
    printf("Scan I2C Devices:\n");
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    for (int i = 0; i < 128; i += 16)
    {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j++)
        {
            fflush(stdout);
            address = i + j;
            err = i2c_master_probe(bus_handle, address, 100);
            if (err == ESP_OK)
            {
                printf("%02x ", address);
            }
            else if (err == ESP_ERR_TIMEOUT)
            {
                printf("UU ");
            }
            else
            {
                printf("-- ");
            }
        }
        printf("\r\n");
    }
    printf("\n\n\n");
#else
    uint8_t address;
    printf("Scan I2C Devices:\n");
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    for (int i = 0; i < 128; i += 16)
    {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j++)
        {
            fflush(stdout);
            address = i + j;
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
            i2c_master_stop(cmd);
            esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd);
            if (ret == ESP_OK)
            {
                printf("%02x ", address);
            }
            else if (ret == ESP_ERR_TIMEOUT)
            {
                printf("UU ");
            }
            else
            {
                printf("-- ");
            }
        }
        printf("\r\n");
    }
#endif
}

bool i2c_drv_probe(uint8_t devAddr)
{
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)
    return ESP_OK == i2c_master_probe(bus_handle, devAddr, 1000);
#else
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK);
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
}

#endif
