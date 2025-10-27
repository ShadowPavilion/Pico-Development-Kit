/**
 * @file gt911.c
 * @brief GT911 电容触摸屏驱动实现
 * @note 基于 GT911 芯片数据手册和 Pico SDK 实现
 * @author NIGHT
 * @date 2025-01-27
 */

/*********************
 *      INCLUDES
 *********************/
#include "gt911.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <string.h>

/**********************
 *  STATIC PROTOTYPES
 **********************/
static bool gt911_i2c_init(void);
static bool gt911_i2c_read_reg(uint16_t reg, uint8_t *data, uint8_t len);
static bool gt911_i2c_write_reg(uint16_t reg, uint8_t *data, uint8_t len);
static void gt911_clear_status(void);

/**********************
 *  STATIC VARIABLES
 **********************/
static gt911_dev_t gt911_dev = {
    .initialized = false,
    .product_id = {0},
    .max_x = 0,
    .max_y = 0,
    .i2c_addr = GT911_I2C_ADDR
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 初始化 GT911 触摸驱动
 * @return true 成功, false 失败
 */
bool gt911_init(void)
{
    uint8_t data;
    
    // 防止重复初始化
    if (gt911_dev.initialized) {
        return true;
    }
    
    // 1. 初始化 I2C 接口
    if (!gt911_i2c_init()) {
        return false;
    }
    
    // 2. 尝试读取产品ID，验证通信是否正常
    if (!gt911_i2c_read_reg(GT911_REG_PRODUCT_ID1, &data, 1)) {
        return false;  // I2C 通信失败
    }
    
    // 3. 读取完整的产品ID (4个字节的ASCII码)
    // 例如：GT911 会返回 "911" (0x39, 0x31, 0x31)
    for (int i = 0; i < GT911_PRODUCT_ID_LEN; i++) {
        if (!gt911_i2c_read_reg(GT911_REG_PRODUCT_ID1 + i, (uint8_t *)&gt911_dev.product_id[i], 1)) {
            return false;
        }
    }
    gt911_dev.product_id[GT911_PRODUCT_ID_LEN] = '\0';  // 字符串结束符
    
    // 4. 读取供应商ID（可选，用于验证）
    if (!gt911_i2c_read_reg(GT911_REG_VENDOR_ID, &data, 1)) {
        return false;
    }
    
    // 5. 读取触摸屏分辨率配置
    // X 分辨率（16位，低字节在前）
    if (!gt911_i2c_read_reg(GT911_REG_X_RES_L, &data, 1)) {
        return false;
    }
    gt911_dev.max_x = data;
    
    if (!gt911_i2c_read_reg(GT911_REG_X_RES_H, &data, 1)) {
        return false;
    }
    gt911_dev.max_x |= ((uint16_t)data << 8);
    
    // Y 分辨率（16位，低字节在前）
    if (!gt911_i2c_read_reg(GT911_REG_Y_RES_L, &data, 1)) {
        return false;
    }
    gt911_dev.max_y = data;
    
    if (!gt911_i2c_read_reg(GT911_REG_Y_RES_H, &data, 1)) {
        return false;
    }
    gt911_dev.max_y |= ((uint16_t)data << 8);
    
    // 6. 初始化完成
    gt911_dev.initialized = true;
    
    return true;
}

/**
 * @brief 读取触摸数据
 * @param x 输出参数：X坐标
 * @param y 输出参数：Y坐标
 * @param pressed 输出参数：是否按下
 * @return true 读取成功, false 读取失败
 */
bool gt911_read_touch(uint16_t *x, uint16_t *y, bool *pressed)
{
    uint8_t status_reg;
    uint8_t data;
    static uint16_t last_x = 0;  // 保存上次的坐标
    static uint16_t last_y = 0;
    
    // 检查是否已初始化
    if (!gt911_dev.initialized) {
        return false;
    }
    
    // 1. 读取状态寄存器
    if (!gt911_i2c_read_reg(GT911_REG_STATUS, &status_reg, 1)) {
        return false;
    }
    
    // 2. 获取触点数量（低4位）
    uint8_t touch_count = status_reg & GT911_STATUS_PT_MASK;
    
    // 3. 检查数据是否准备好，并清除状态寄存器
    // bit7=1 表示有新的触摸数据，读取后需要清零状态寄存器
    if ((status_reg & GT911_STATUS_BUF_READY) || (touch_count < 6)) {
        gt911_clear_status();  // 清除状态，告诉GT911我们已经读取了数据
    }
    
    // 4. 处理触摸数据
    if (touch_count == 1) {
        // 单点触摸：读取第一个触点的坐标
        
        // 读取 X 坐标（16位，低字节在前）
        if (!gt911_i2c_read_reg(GT911_REG_PT1_X_L, &data, 1)) {
            return false;
        }
        last_x = data;
        
        if (!gt911_i2c_read_reg(GT911_REG_PT1_X_H, &data, 1)) {
            return false;
        }
        last_x |= ((uint16_t)data << 8);
        
        // 读取 Y 坐标（16位，低字节在前）
        if (!gt911_i2c_read_reg(GT911_REG_PT1_Y_L, &data, 1)) {
            return false;
        }
        last_y = data;
        
        if (!gt911_i2c_read_reg(GT911_REG_PT1_Y_H, &data, 1)) {
            return false;
        }
        last_y |= ((uint16_t)data << 8);
        
        // 设置输出参数
        *x = last_x;
        *y = last_y;
        *pressed = true;
        
    } else {
        // 没有触摸或多点触摸（暂不支持多点）
        // 返回上次的坐标，但状态为释放
        *x = last_x;
        *y = last_y;
        *pressed = false;
    }
    
    return true;
}

/**
 * @brief 获取设备信息
 * @return 设备信息结构体指针
 */
gt911_dev_t* gt911_get_dev_info(void)
{
    return &gt911_dev;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 初始化 I2C 接口
 * @return true 成功, false 失败
 */
static bool gt911_i2c_init(void)
{
    // 1. 初始化 I2C 外设，设置波特率为 100kHz
    uint32_t actual_baudrate = i2c_init(GT911_I2C_PORT, GT911_I2C_BAUDRATE);
    
    if (actual_baudrate == 0) {
        return false;  // I2C 初始化失败
    }
    
    // 2. 配置 GPIO 引脚为 I2C 功能
    gpio_set_function(GT911_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(GT911_PIN_SCL, GPIO_FUNC_I2C);
    
    // 3. 启用内部上拉电阻
    // I2C 总线需要上拉电阻才能正常工作
    gpio_pull_up(GT911_PIN_SDA);
    gpio_pull_up(GT911_PIN_SCL);
    
    // 短暂延时，等待 I2C 总线稳定
    sleep_ms(10);
    
    return true;
}

/**
 * @brief 从 GT911 寄存器读取数据
 * @param reg 寄存器地址（16位）
 * @param data 数据缓冲区指针
 * @param len 要读取的字节数
 * @return true 成功, false 失败
 */
static bool gt911_i2c_read_reg(uint16_t reg, uint8_t *data, uint8_t len)
{
    if (data == NULL || len == 0) {
        return false;
    }
    
    // GT911 使用 16位寄存器地址，需要发送高字节和低字节
    uint8_t reg_addr[2] = {
        (reg >> 8) & 0xFF,  // 寄存器地址高字节
        reg & 0xFF          // 寄存器地址低字节
    };
    
    // I2C 读取操作分两步：
    // 1. 写入寄存器地址（保持总线，不发送STOP）
    int ret = i2c_write_blocking(GT911_I2C_PORT, gt911_dev.i2c_addr, reg_addr, 2, true);
    if (ret < 0) {
        return false;  // 写入失败
    }
    
    // 2. 读取数据
    ret = i2c_read_blocking(GT911_I2C_PORT, gt911_dev.i2c_addr, data, len, false);
    if (ret < 0) {
        return false;  // 读取失败
    }
    
    return true;
}

/**
 * @brief 向 GT911 寄存器写入数据
 * @param reg 寄存器地址（16位）
 * @param data 数据缓冲区指针
 * @param len 要写入的字节数
 * @return true 成功, false 失败
 */
static bool gt911_i2c_write_reg(uint16_t reg, uint8_t *data, uint8_t len)
{
    if (data == NULL || len == 0) {
        return false;
    }
    
    // 组合寄存器地址和数据
    uint8_t buffer[32];  // 临时缓冲区
    
    if (len + 2 > sizeof(buffer)) {
        return false;  // 数据太长
    }
    
    // 寄存器地址（高字节在前）
    buffer[0] = (reg >> 8) & 0xFF;
    buffer[1] = reg & 0xFF;
    
    // 复制数据
    memcpy(&buffer[2], data, len);
    
    // 发送数据
    int ret = i2c_write_blocking(GT911_I2C_PORT, gt911_dev.i2c_addr, buffer, len + 2, false);
    
    return (ret > 0);
}

/**
 * @brief 清除 GT911 状态寄存器
 * @note 读取触摸数据后必须清除状态寄存器，否则GT911不会更新新的触摸数据
 */
static void gt911_clear_status(void)
{
    // 向状态寄存器 (0x814E) 写入 0x00
    // 这告诉 GT911 芯片："我已经读取了触摸数据，可以准备下一帧数据了"
    uint8_t clear_data = 0x00;
    
    // 方法1：使用封装的写寄存器函数
    // gt911_i2c_write_reg(GT911_REG_STATUS, &clear_data, 1);
    
    // 方法2：直接发送（更高效）
    uint8_t buffer[3] = {
        0x81,       // 寄存器地址高字节 (0x814E >> 8)
        0x4E,       // 寄存器地址低字节 (0x814E & 0xFF)
        0x00        // 清零数据
    };
    
    i2c_write_blocking(GT911_I2C_PORT, gt911_dev.i2c_addr, buffer, 3, false);
}

