/**
 * @file gt911.h
 * @brief GT911 电容触摸屏驱动头文件
 * @note 基于 GT911 芯片数据手册实现
 * @author NIGHT
 * @date 2025-10-27
 */

 #ifndef GT911_H
 #define GT911_H
 
 #include <stdint.h>
 #include <stdbool.h>
 
 /**********************
  *      DEFINES
  **********************/
 /* I2C 从设备地址 */
 #define GT911_I2C_ADDR          0x5D
 
 /* 产品ID长度 */
 #define GT911_PRODUCT_ID_LEN    4
 
 /* 硬件引脚配置 */
 #define GT911_I2C_PORT          i2c0
 #define GT911_PIN_SDA           8
 #define GT911_PIN_SCL           9
 #define GT911_I2C_BAUDRATE      100000  // 100kHz
 
 /* GT911 寄存器地址 - 来自芯片数据手册 */
 #define GT911_REG_PRODUCT_ID1       0x8140
 #define GT911_REG_PRODUCT_ID2       0x8141
 #define GT911_REG_PRODUCT_ID3       0x8142
 #define GT911_REG_PRODUCT_ID4       0x8143
 #define GT911_REG_FIRMWARE_VER_L    0x8144
 #define GT911_REG_FIRMWARE_VER_H    0x8145
 #define GT911_REG_X_RES_L           0x8146  // X分辨率低字节
 #define GT911_REG_X_RES_H           0x8147  // X分辨率高字节
 #define GT911_REG_Y_RES_L           0x8148  // Y分辨率低字节
 #define GT911_REG_Y_RES_H           0x8149  // Y分辨率高字节
 #define GT911_REG_VENDOR_ID         0x814A
 
 #define GT911_REG_STATUS            0x814E  // 触摸状态寄存器
 #define GT911_REG_TRACK_ID1         0x814F
 #define GT911_REG_PT1_X_L           0x8150  // 触点1 X坐标低字节
 #define GT911_REG_PT1_X_H           0x8151  // 触点1 X坐标高字节
 #define GT911_REG_PT1_Y_L           0x8152  // 触点1 Y坐标低字节
 #define GT911_REG_PT1_Y_H           0x8153  // 触点1 Y坐标高字节
 #define GT911_REG_PT1_SIZE_L        0x8154
 #define GT911_REG_PT1_SIZE_H        0x8155
 
 /* 状态寄存器位定义 */
 #define GT911_STATUS_BUF_READY      0x80  // 数据准备好标志
 #define GT911_STATUS_LARGE          0x40
 #define GT911_STATUS_PROX_VALID     0x20
 #define GT911_STATUS_HAVE_KEY       0x10
 #define GT911_STATUS_PT_MASK        0x0F  // 触点数量掩码
 
 /**********************
  *      TYPEDEFS
  **********************/
 /**
  * @brief GT911 设备状态结构体
  */
 typedef struct {
     bool initialized;               // 初始化标志
     char product_id[GT911_PRODUCT_ID_LEN + 1];  // 产品ID (字符串)
     uint16_t max_x;                 // X轴最大坐标
     uint16_t max_y;                 // Y轴最大坐标
     uint8_t i2c_addr;               // I2C地址
 } gt911_dev_t;
 
 /**********************
  * FUNCTION PROTOTYPES
  **********************/
 /**
  * @brief 初始化 GT911 触摸驱动
  * @return true 成功, false 失败
  */
 bool gt911_init(void);
 
 /**
  * @brief 读取触摸数据
  * @param x 输出参数：X坐标
  * @param y 输出参数：Y坐标
  * @param pressed 输出参数：是否按下
  * @return true 读取成功, false 读取失败
  */
 bool gt911_read_touch(uint16_t *x, uint16_t *y, bool *pressed);
 
 /**
  * @brief 获取设备信息（可选）
  * @return 设备信息结构体指针
  */
 gt911_dev_t* gt911_get_dev_info(void);
 
 #endif /* GT911_H */