/**
 * @file lv_port_disp.h
 * @brief ST7796 显示驱动 LVGL 移植层接口
 * @author NIGHT
 * @date 2025-10-27
 */
#if 1

#ifndef LV_PORT_DISP_TEMPL_H
#define LV_PORT_DISP_TEMPL_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/**
 * @brief 初始化显示驱动
 * @note 在使用 LVGL 之前必须调用此函数
 */
void lv_port_disp_init(void);

/**
 * @brief 启用屏幕刷新
 */
void disp_enable_update(void);

/**
 * @brief 禁用屏幕刷新
 */
void disp_disable_update(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_PORT_DISP_TEMPL_H*/

#endif /*Disable/Enable content*/
