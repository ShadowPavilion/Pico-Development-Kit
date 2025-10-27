/**
 * @file st7796.h
 * @brief ST7796 LCD 驱动头文件
 * @note 基于 ST7796 芯片数据手册实现
 * @author NIGHT
 * @date 2025-10-27
 */

 #ifndef ST7796_H
 #define ST7796_H
 
 #include <stdint.h>
 #include <stdbool.h>
 
 /**********************
  *      DEFINES
  **********************/
 /* 屏幕分辨率 */
 #define ST7796_WIDTH    320
 #define ST7796_HEIGHT   480

 /* 硬件引脚配置 */
#define ST7796_SPI_PORT     spi0
#define ST7796_PIN_CLK      2
#define ST7796_PIN_MOSI     3
#define ST7796_PIN_CS       5
#define ST7796_PIN_DC       6
#define ST7796_PIN_RST      7

/* SPI 时钟频率 (Hz) */
#define ST7796_SPI_BAUDRATE (62500000)  // 62.5MHz

/* ST7796 命令定义 - 来自数据手册 */
#define ST7796_CMD_SWRESET      0x01
#define ST7796_CMD_SLPIN        0x10
#define ST7796_CMD_SLPOUT       0x11
#define ST7796_CMD_INVOFF       0x20
#define ST7796_CMD_INVON        0x21
#define ST7796_CMD_DISPOFF      0x28
#define ST7796_CMD_DISPON       0x29
#define ST7796_CMD_CASET        0x2A  // 列地址设置
#define ST7796_CMD_RASET        0x2B  // 行地址设置
#define ST7796_CMD_RAMWR        0x2C  // 写GRAM
#define ST7796_CMD_MADCTL       0x36  // 显示方向控制
#define ST7796_CMD_COLMOD       0x3A  // 像素格式设置

/**********************
 *      TYPEDEFS
 **********************/
/* 屏幕方向定义 */
typedef enum {
    ST7796_PORTRAIT         = 0,  // 竖屏
    ST7796_LANDSCAPE        = 1,  // 横屏
    ST7796_PORTRAIT_INV     = 2,  // 竖屏翻转
    ST7796_LANDSCAPE_INV    = 3   // 横屏翻转
} st7796_orientation_t;

/**********************
 * FUNCTION PROTOTYPES
 **********************/
/**
 * @brief 初始化 ST7796 显示驱动
 * @note 必须在使用其他函数前调用
 */
void st7796_init(void);

/**
 * @brief 设置显示方向
 * @param orientation 屏幕方向
 */
void st7796_set_orientation(st7796_orientation_t orientation);

/**
 * @brief 设置显示窗口（绘制区域）
 * @param x1 起始X坐标
 * @param y1 起始Y坐标
 * @param x2 结束X坐标
 * @param y2 结束Y坐标
 */
void st7796_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/**
 * @brief 向显示区域写入颜色数据
 * @param color 颜色数据指针 (RGB565格式)
 * @param len 像素数量
 */
void st7796_write_color(const uint16_t *color, uint32_t len);

#endif /* ST7796_H */