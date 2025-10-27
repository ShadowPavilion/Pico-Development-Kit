/**
 * @file lv_port_disp.c
 * @brief LVGL 显示驱动移植层
 * @note 调用 st7796.c 硬件驱动实现显示功能
 * @author NIGHT
 * @date 2025-10-27
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include "st7796.h"
#include <stdbool.h>

/*********************
 *      DEFINES
 *********************/
#define MY_DISP_HOR_RES    320
#define MY_DISP_VER_RES    480

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

/**********************
 *  STATIC VARIABLES
 **********************/
/* 控制显示刷新的开关 */
static volatile bool disp_flush_enabled = true;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 初始化 LVGL 显示驱动
 */
void lv_port_disp_init(void)
{
    /*-------------------------
     * 初始化显示硬件
     * -----------------------*/
    disp_init();

    /*-----------------------------
     * 创建 LVGL 绘制缓冲区
     *----------------------------*/

    /**
     * LVGL 需要一个缓冲区用于内部绘制 widget。
     * 稍后这个缓冲区会传递给显示驱动的 `flush_cb`，将内容复制到显示器。
     * 缓冲区必须大于 1 个显示行。
     *
     * 有 3 种缓冲配置：
     * 1. 单缓冲：
     *    LVGL 会在此绘制内容并写入显示器
     *
     * 2. 双缓冲：
     *    LVGL 将内容绘制到缓冲区并写入显示器。
     *    应使用 DMA 将缓冲区内容写入显示器。
     *    这样可以让 LVGL 在传输第一个缓冲区时绘制到第二个缓冲区，实现并行。
     *
     * 3. 全屏双缓冲：
     *    设置 2 个屏幕大小的缓冲区，并设置 disp_drv.full_refresh = 1。
     *    这样 LVGL 总会在 `flush_cb` 中提供完整的渲染屏幕，只需更改帧缓冲区地址。
     */

    /* 示例 1：单缓冲配置 (节省内存) */
    static lv_disp_draw_buf_t draw_buf_dsc_1;
    static lv_color_t buf_1[MY_DISP_HOR_RES * 10];  // 10 行的缓冲区
    lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, NULL, MY_DISP_HOR_RES * 10);

    /* 示例 2：双缓冲配置 (性能更好，但需要更多内存)
    static lv_disp_draw_buf_t draw_buf_dsc_2;
    static lv_color_t buf_2_1[MY_DISP_HOR_RES * 10];  // 第一个缓冲区
    static lv_color_t buf_2_2[MY_DISP_HOR_RES * 10];  // 第二个缓冲区
    lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 10);
    */

    /*-----------------------------------
     * 在 LVGL 中注册显示驱动
     *----------------------------------*/
    static lv_disp_drv_t disp_drv;              // 显示驱动描述符
    lv_disp_drv_init(&disp_drv);                // 基本初始化

    /* 设置显示分辨率 */
    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;
    
    /* 设置用于将缓冲区内容复制到显示器的回调函数 */
    disp_drv.flush_cb = disp_flush;

    /* 设置显示缓冲区 */
    disp_drv.draw_buf = &draw_buf_dsc_1;

    /* 如果使用示例 3 的全屏双缓冲，需要启用此选项
    disp_drv.full_refresh = 1;
    */

    /* GPU 填充回调（如果有硬件加速）
    disp_drv.gpu_fill_cb = gpu_fill;
    */

    /* 最后注册驱动 */
    lv_disp_drv_register(&disp_drv);
}

/**
 * @brief 启用屏幕刷新
 * @note 启用后，disp_flush() 会将数据写入显示器
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/**
 * @brief 禁用屏幕刷新
 * @note 禁用后，disp_flush() 不会写入显示器（用于截图等场景）
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 初始化显示硬件
 * @note 调用底层 ST7796 驱动进行初始化
 */
static void disp_init(void)
{
    // 调用 ST7796 硬件驱动的初始化函数
    // 这会完成：SPI 初始化、GPIO 配置、屏幕复位、发送初始化命令序列
    st7796_init();
}

/**
 * @brief 将内部缓冲区的内容刷新到显示器的指定区域
 * @param disp_drv 显示驱动指针
 * @param area 要刷新的区域
 * @param color_p 颜色数据指针 (RGB565 格式)
 * @note 可以使用 DMA 或硬件加速来执行此操作，但必须在完成后调用 lv_disp_flush_ready()
 */
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    // 检查是否允许刷新
    if (!disp_flush_enabled) {
        lv_disp_flush_ready(disp_drv);
        return;
    }
    
    // 1. 设置显示窗口（要绘制的矩形区域）
    st7796_set_window(area->x1, area->y1, area->x2, area->y2);
    
    // 2. 计算像素数量
    uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);
    
    // 3. 写入颜色数据
    // LVGL 的 lv_color_t 在 lv_conf.h 中配置为 RGB565 (16位)
    // 这与 ST7796 的 RGB565 格式兼容，可以直接传输
    st7796_write_color((uint16_t *)color_p, size);
    
    // 4. 通知 LVGL 刷新完成
    // 重要：必须调用此函数告诉 LVGL 可以继续渲染下一帧
    lv_disp_flush_ready(disp_drv);
}

/*
 * 以下是可选的 GPU 加速回调函数示例
 * 如果硬件支持，可以实现以提升性能
 */

#if 0  // 如果需要 GPU 填充，将此改为 1

/**
 * @brief GPU 填充回调（可选）
 * @note 如果有 GPU 或 DMA 支持，可以实现此函数加速填充操作
 */
static void gpu_fill(lv_disp_drv_t * disp_drv, lv_color_t * dest_buf, lv_coord_t dest_width,
        const lv_area_t * fill_area, lv_color_t color)
{
    /* 实现 GPU 填充逻辑 */
    /* 例如：使用 DMA 快速填充单一颜色 */
}

#endif

/*
 * 代码简化说明：
 * 
 * 旧版本 (原 lv_port_disp.c):
 *   - 400+ 行代码
 *   - 包含完整的 SPI 操作代码
 *   - 包含 ST7796 初始化序列
 *   - 直接操作硬件寄存器
 * 
 * 新版本 (当前文件):
 *   - ~180 行代码（包含详细注释）
 *   - 只调用 st7796.c 的 API
 *   - 硬件细节完全隔离
 *   - 易于维护和移植
 * 
 * 好处：
 *   1. 代码更清晰：LVGL 移植层专注于 LVGL 接口
 *   2. 易于测试：可以单独测试 st7796.c
 *   3. 易于移植：更换硬件只需修改 st7796.c
 *   4. 易于维护：分层清晰，职责明确
 */
