/**
 * @file lv_port_indev.h
 * @brief LVGL 输入设备驱动移植层接口
 * @author NIGHT
 * @date 2025-10-27
 */

#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/**
 * @brief 初始化输入设备驱动
 * @note 在使用 LVGL 触摸功能前必须调用
 */
void lv_port_indev_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_PORT_INDEV_H*/
