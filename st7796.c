/**
 * @file st7796.c
 * @brief ST7796 LCD 驱动实现
 * @note 基于 ST7796 芯片数据手册和 Pico SDK 实现
 * @author NIGHT
 * @date 2025-10-27
 */

/*********************
 *      INCLUDES
 *********************/
#include "st7796.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <string.h>

/**********************
 *      DEFINES
 **********************/
/* GPIO 控制宏定义 */
#define LCD_CS_LOW()    gpio_put(ST7796_PIN_CS, 0)
#define LCD_CS_HIGH()   gpio_put(ST7796_PIN_CS, 1)
#define LCD_DC_CMD()    gpio_put(ST7796_PIN_DC, 0)
#define LCD_DC_DATA()   gpio_put(ST7796_PIN_DC, 1)
#define LCD_RST_LOW()   gpio_put(ST7796_PIN_RST, 0)
#define LCD_RST_HIGH()  gpio_put(ST7796_PIN_RST, 1)

/**********************
 *      TYPEDEFS
 **********************/
/**
 * @brief LCD 初始化命令结构体
 */
typedef struct {
    uint8_t cmd;            // 命令字节
    uint8_t data[16];       // 数据字节（最多16个）
    uint8_t databytes;      // 数据字节数 (bit7=1表示需要延时)
} lcd_init_cmd_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void st7796_write_cmd(uint8_t cmd);
static void st7796_write_data(const uint8_t *data, uint16_t len);
static void st7796_hw_reset(void);
static void st7796_gpio_init(void);
static void st7796_spi_init(void);

/**********************
 *  STATIC VARIABLES
 **********************/
static st7796_orientation_t current_orientation = ST7796_PORTRAIT;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 初始化 ST7796 显示驱动
 */
void st7796_init(void)
{
    // 1. 初始化 SPI 接口
    st7796_spi_init();
    
    // 2. 初始化 GPIO 引脚
    st7796_gpio_init();
    
    // 3. 硬件复位
    st7796_hw_reset();
    
    // 4. 发送初始化命令序列
    // 这些命令来自 ST7796 数据手册和屏幕厂商推荐配置
    lcd_init_cmd_t init_cmds[] = {
        {0xCF, {0x00, 0x83, 0x30}, 3},
        {0xED, {0x64, 0x03, 0x12, 0x81}, 4},
        {0xE8, {0x85, 0x01, 0x79}, 3},
        {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
        {0xF7, {0x20}, 1},
        {0xEA, {0x00, 0x00}, 2},
        
        // 电源控制
        {0xC0, {0x26}, 1},          // Power Control 1
        {0xC1, {0x11}, 1},          // Power Control 2
        {0xC5, {0x35, 0x3E}, 2},    // VCOM Control 1
        {0xC7, {0xBE}, 1},          // VCOM Control 2
        
        // 显示设置
        {0x36, {0x28}, 1},          // Memory Access Control
        {0x3A, {0x05}, 1},          // Pixel Format Set (RGB565)
        
        // 帧率控制
        {0xB1, {0x00, 0x1B}, 2},
        {0xF2, {0x08}, 1},
        {0x26, {0x01}, 1},
        
        // Gamma 校正 - 影响显示色彩质量
        {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0x87, 
                0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},  // Positive Gamma
        {0xE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 
                0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},  // Negative Gamma
        
        // 设置显示区域
        {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},  // Column Address Set
        {0x2B, {0x00, 0x00, 0x01, 0x3F}, 4},  // Row Address Set
        {0x2C, {0}, 0},                        // Memory Write
        
        {0xB7, {0x07}, 1},
        {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},  // Display Function Control
        
        // 退出睡眠模式 (需要延时 100ms)
        {0x11, {0}, 0x80},  // Sleep Out (bit7=1 表示需要延时)
        
        // 开启显示 (需要延时 100ms)
        {0x29, {0}, 0x80},  // Display ON
        
        // 结束标志
        {0, {0}, 0xFF},
    };
    
    // 5. 依次发送初始化命令
    uint16_t cmd_idx = 0;
    while (init_cmds[cmd_idx].databytes != 0xFF) {
        st7796_write_cmd(init_cmds[cmd_idx].cmd);
        
        // 如果有数据，发送数据
        uint8_t data_len = init_cmds[cmd_idx].databytes & 0x1F;  // 低5位是数据长度
        if (data_len > 0) {
            st7796_write_data(init_cmds[cmd_idx].data, data_len);
        }
        
        // 如果需要延时 (bit7=1)
        if (init_cmds[cmd_idx].databytes & 0x80) {
            sleep_ms(100);  // 延时 100ms
        }
        
        cmd_idx++;
    }
    
    // 6. 设置默认显示方向
    st7796_set_orientation(ST7796_PORTRAIT);
    
    // 7. 开启颜色反转 (根据屏幕特性可能需要)
    st7796_write_cmd(0x21);  // Display Inversion ON
}

/**
 * @brief 设置显示方向
 * @param orientation 屏幕方向
 */
void st7796_set_orientation(st7796_orientation_t orientation)
{
    current_orientation = orientation;
    
    st7796_write_cmd(ST7796_CMD_MADCTL);  // 0x36
    
    uint8_t madctl_value;
    
    // 根据方向设置 MADCTL 寄存器
    // MADCTL 寄存器各位含义（参考数据手册）：
    // bit7: MY  - 行地址顺序
    // bit6: MX  - 列地址顺序
    // bit5: MV  - 行列交换
    // bit4: ML  - 垂直刷新顺序
    // bit3: BGR - RGB/BGR 顺序
    // bit2: MH  - 水平刷新顺序
    
    switch (orientation) {
        case ST7796_PORTRAIT:
            madctl_value = 0x48;  // MY=0, MX=1, MV=0, ML=0, BGR=1
            break;
        case ST7796_LANDSCAPE:
            madctl_value = 0x28;  // MY=0, MX=0, MV=1, ML=0, BGR=1
            break;
        case ST7796_PORTRAIT_INV:
            madctl_value = 0x88;  // MY=1, MX=0, MV=0, ML=0, BGR=1
            break;
        case ST7796_LANDSCAPE_INV:
            madctl_value = 0xE8;  // MY=1, MX=1, MV=1, ML=0, BGR=1
            break;
        default:
            madctl_value = 0x48;
            break;
    }
    
    LCD_CS_LOW();
    LCD_DC_DATA();
    spi_write_blocking(ST7796_SPI_PORT, &madctl_value, 1);
    LCD_CS_HIGH();
}

/**
 * @brief 设置显示窗口（绘制区域）
 * @param x1 起始X坐标
 * @param y1 起始Y坐标
 * @param x2 结束X坐标
 * @param y2 结束Y坐标
 */
void st7796_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint8_t data[4];
    
    // 设置列地址范围 (X坐标)
    st7796_write_cmd(ST7796_CMD_CASET);  // 0x2A
    data[0] = (x1 >> 8) & 0xFF;  // 起始X高字节
    data[1] = x1 & 0xFF;         // 起始X低字节
    data[2] = (x2 >> 8) & 0xFF;  // 结束X高字节
    data[3] = x2 & 0xFF;         // 结束X低字节
    st7796_write_data(data, 4);
    
    // 设置行地址范围 (Y坐标)
    st7796_write_cmd(ST7796_CMD_RASET);  // 0x2B
    data[0] = (y1 >> 8) & 0xFF;  // 起始Y高字节
    data[1] = y1 & 0xFF;         // 起始Y低字节
    data[2] = (y2 >> 8) & 0xFF;  // 结束Y高字节
    data[3] = y2 & 0xFF;         // 结束Y低字节
    st7796_write_data(data, 4);
    
    // 准备写入 GRAM
    st7796_write_cmd(ST7796_CMD_RAMWR);  // 0x2C
}

/**
 * @brief 向显示区域写入颜色数据
 * @param color 颜色数据指针 (RGB565格式，每像素2字节)
 * @param len 像素数量
 * @note 调用此函数前必须先调用 st7796_set_window() 设置显示区域
 */
void st7796_write_color(const uint16_t *color, uint32_t len)
{
    if (len == 0 || color == NULL) {
        return;
    }
    
    LCD_CS_LOW();
    LCD_DC_DATA();
    
    // 写入颜色数据
    // RGB565 格式：每个像素 2 字节
    spi_write_blocking(ST7796_SPI_PORT, (const uint8_t *)color, len * 2);
    
    LCD_CS_HIGH();
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 向 ST7796 发送命令
 * @param cmd 命令字节
 */
static void st7796_write_cmd(uint8_t cmd)
{
    LCD_CS_LOW();
    LCD_DC_CMD();       // DC=0 表示发送命令
    sleep_us(1);        // 短暂延时确保信号稳定
    
    spi_write_blocking(ST7796_SPI_PORT, &cmd, 1);
    
    sleep_us(1);
    LCD_CS_HIGH();
}

/**
 * @brief 向 ST7796 发送数据
 * @param data 数据缓冲区指针
 * @param len 数据长度（字节）
 */
static void st7796_write_data(const uint8_t *data, uint16_t len)
{
    if (len == 0 || data == NULL) {
        return;
    }
    
    LCD_CS_LOW();
    LCD_DC_DATA();      // DC=1 表示发送数据
    sleep_us(1);
    
    spi_write_blocking(ST7796_SPI_PORT, data, len);
    
    sleep_us(1);
    LCD_CS_HIGH();
}

/**
 * @brief ST7796 硬件复位
 * @note 根据数据手册，复位时序为：RST拉低至少10us，然后拉高，延时120ms
 */
static void st7796_hw_reset(void)
{
    LCD_RST_HIGH();
    sleep_ms(100);
    
    LCD_RST_LOW();
    sleep_ms(100);
    
    LCD_RST_HIGH();
    sleep_ms(100);  // 等待芯片复位完成
}

/**
 * @brief 初始化 GPIO 引脚
 */
static void st7796_gpio_init(void)
{
    // 初始化 CS (片选) 引脚
    gpio_init(ST7796_PIN_CS);
    gpio_set_dir(ST7796_PIN_CS, GPIO_OUT);
    gpio_put(ST7796_PIN_CS, 1);  // 默认高电平（未选中）
    
    // 初始化 DC (数据/命令选择) 引脚
    gpio_init(ST7796_PIN_DC);
    gpio_set_dir(ST7796_PIN_DC, GPIO_OUT);
    gpio_put(ST7796_PIN_DC, 1);  // 默认高电平（数据模式）
    
    // 初始化 RST (复位) 引脚
    gpio_init(ST7796_PIN_RST);
    gpio_set_dir(ST7796_PIN_RST, GPIO_OUT);
    gpio_put(ST7796_PIN_RST, 1);  // 默认高电平（不复位）
}

/**
 * @brief 初始化 SPI 接口
 */
static void st7796_spi_init(void)
{
    // 初始化 SPI 外设，设置波特率
    spi_init(ST7796_SPI_PORT, ST7796_SPI_BAUDRATE);
    
    // 设置 SPI 格式
    // 参数：8位数据，CPOL=0（时钟空闲为低），CPHA=0（第一个边沿采样），MSB先传输
    spi_set_format(ST7796_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    // 配置 GPIO 引脚为 SPI 功能
    gpio_set_function(ST7796_PIN_MOSI, GPIO_FUNC_SPI);  // MOSI (数据输出)
    gpio_set_function(ST7796_PIN_CLK, GPIO_FUNC_SPI);   // CLK (时钟)
}

