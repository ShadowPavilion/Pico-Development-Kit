/**
 * @file lv_port_indev.c
 * @brief LVGL 输入设备驱动移植层
 * @note 调用 gt911.c 硬件驱动实现触摸功能
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "lvgl.h"
#include "gt911.h"

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t *indev_touchpad;

/* 用于保存上次的触摸坐标 */
static int16_t last_x = 0;
static int16_t last_y = 0;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 初始化 LVGL 输入设备驱动
 */
void lv_port_indev_init(void)
{
    /**
     * LVGL 支持以下输入设备：
     *  - 触摸板 (Touchpad)
     *  - 鼠标 (Mouse) - 带光标支持
     *  - 键盘 (Keypad) - 仅支持 GUI 导航键
     *  - 编码器 (Encoder) - 支持左、右、按下
     *  - 按钮 (Button) - 外部按钮映射到屏幕上的点
     *
     * 本项目使用触摸板 (GT911 电容触摸屏)
     */

    static lv_indev_drv_t indev_drv;

    /*------------------
     * 触摸板
     * -----------------*/

    /* 初始化触摸硬件 */
    touchpad_init();

    /* 注册触摸板输入设备 */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;  // 指针类型设备（触摸/鼠标）
    indev_drv.read_cb = touchpad_read;        // 读取回调函数
    indev_touchpad = lv_indev_drv_register(&indev_drv);

    /*------------------
     * 其他输入设备
     * -----------------*/
    
    /* 如果需要添加鼠标、键盘等，可以在这里注册 */
    
    /*
    // 鼠标示例
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read;
    lv_indev_drv_register(&indev_drv);
    */
    
    /*
    // 键盘示例
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keypad_read;
    lv_indev_drv_register(&indev_drv);
    */
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 初始化触摸板硬件
 * @note 调用底层 GT911 驱动进行初始化
 */
static void touchpad_init(void)
{
    // 调用 GT911 硬件驱动的初始化函数
    // 这会完成：I2C 初始化、GPIO 配置、读取触摸屏配置
    if (!gt911_init()) {
        // 初始化失败
        // 可以在这里添加错误处理，例如：
        // printf("GT911 初始化失败！\n");
    }
}

/**
 * @brief 读取触摸板数据
 * @param indev_drv 输入设备驱动指针
 * @param data 输出数据结构
 * @note LVGL 会周期性调用此函数读取触摸状态
 */
static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    uint16_t x, y;
    bool pressed;
    
    // 告诉 LVGL 不需要继续读取（单点触摸）
    data->continue_reading = false;
    
    // 从 GT911 驱动读取触摸数据
    if (gt911_read_touch(&x, &y, &pressed)) {
        if (pressed) {
            // 有触摸：更新坐标和状态
            data->point.x = x;
            data->point.y = y;
            data->state = LV_INDEV_STATE_PR;  // Pressed (按下)
            
            // 保存坐标供下次使用
            last_x = x;
            last_y = y;
        } else {
            // 无触摸：返回上次坐标，状态为释放
            data->point.x = last_x;
            data->point.y = last_y;
            data->state = LV_INDEV_STATE_REL;  // Released (释放)
        }
    } else {
        // 读取失败：返回上次坐标，状态为释放
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_REL;
    }
    
    /*
     * 坐标变换说明：
     * 
     * 如果触摸坐标与显示不匹配，可以在这里进行变换：
     * 
     * 1. 反转 X 坐标：
     *    data->point.x = LCD_WIDTH - data->point.x;
     * 
     * 2. 反转 Y 坐标：
     *    data->point.y = LCD_HEIGHT - data->point.y;
     * 
     * 3. 交换 X 和 Y（横屏/竖屏切换）：
     *    int16_t temp = data->point.x;
     *    data->point.x = data->point.y;
     *    data->point.y = temp;
     * 
     * 4. 缩放坐标（如果触摸分辨率与显示分辨率不同）：
     *    data->point.x = (data->point.x * LCD_WIDTH) / TOUCH_MAX_X;
     *    data->point.y = (data->point.y * LCD_HEIGHT) / TOUCH_MAX_Y;
     */
}

/*
 * 以下是其他输入设备的示例代码
 * 如果需要支持鼠标、键盘、编码器等，可以参考实现
 */

#if 0  // 如果需要鼠标支持，将此改为 1

/**
 * @brief 初始化鼠标
 */
static void mouse_init(void)
{
    /* 实现鼠标初始化 */
}

/**
 * @brief 读取鼠标数据
 */
static void mouse_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    /* 获取鼠标坐标 */
    // mouse_get_xy(&data->point.x, &data->point.y);
    
    /* 获取鼠标按键状态 */
    // data->state = mouse_is_pressed() ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

#endif

#if 0  // 如果需要键盘支持，将此改为 1

/**
 * @brief 初始化键盘
 */
static void keypad_init(void)
{
    /* 实现键盘初始化 */
}

/**
 * @brief 读取键盘数据
 */
static void keypad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static uint32_t last_key = 0;
    
    /* 获取按键 */
    uint32_t act_key = keypad_get_key();
    
    if (act_key != 0) {
        data->state = LV_INDEV_STATE_PR;
        last_key = act_key;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
    
    data->key = last_key;
}

/**
 * @brief 获取按下的键值
 * @return 键值，0 表示无按键
 */
static uint32_t keypad_get_key(void)
{
    /* 实现键盘扫描 */
    /* 返回 LVGL 定义的键值，如：
     * LV_KEY_UP, LV_KEY_DOWN, LV_KEY_LEFT, LV_KEY_RIGHT
     * LV_KEY_ENTER, LV_KEY_ESC, LV_KEY_BACKSPACE, etc.
     */
    return 0;
}

#endif

#if 0  // 如果需要编码器支持，将此改为 1

static int32_t encoder_diff;
static lv_indev_state_t encoder_state;

/**
 * @brief 初始化编码器
 */
static void encoder_init(void)
{
    /* 实现编码器初始化 */
}

/**
 * @brief 读取编码器数据
 */
static void encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    data->enc_diff = encoder_diff;
    data->state = encoder_state;
    
    /* 重置差值 */
    encoder_diff = 0;
}

/**
 * @brief 编码器中断处理（在中断中调用）
 */
static void encoder_handler(void)
{
    /* 根据编码器方向更新 encoder_diff */
    /* encoder_diff++; 或 encoder_diff--; */
    
    /* 更新按键状态 */
    /* encoder_state = LV_INDEV_STATE_PR 或 LV_INDEV_STATE_REL; */
}

#endif

#if 0  // 如果需要按钮支持，将此改为 1

/**
 * @brief 初始化按钮
 */
static void button_init(void)
{
    /* 实现按钮初始化 */
}

/**
 * @brief 读取按钮数据
 */
static void button_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static uint8_t last_btn = 0;
    
    /* 获取当前按下的按钮 ID */
    int8_t btn_act = button_get_pressed_id();
    
    if (btn_act >= 0) {
        data->state = LV_INDEV_STATE_PR;
        last_btn = btn_act;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
    
    data->btn_id = last_btn;
}

/**
 * @brief 获取按下的按钮 ID
 * @return 按钮 ID (0, 1, 2, ...) 或 -1 表示无按键
 */
static int8_t button_get_pressed_id(void)
{
    /* 扫描按钮 */
    /* 返回第一个按下的按钮 ID */
    return -1;
}

#endif

/*
 * 代码简化说明：
 * 
 * 旧版本 (原 lv_port_indev.c):
 *   - 220+ 行代码
 *   - 包含完整的 I2C 操作代码
 *   - 包含 GT911 寄存器操作
 *   - 直接操作硬件
 * 
 * 新版本 (当前文件):
 *   - ~280 行代码（包含详细注释和其他输入设备示例）
 *   - 实际触摸代码仅 ~100 行
 *   - 只调用 gt911.c 的 API
 *   - 硬件细节完全隔离
 * 
 * 好处：
 *   1. 代码更清晰：只关注 LVGL 接口适配
 *   2. 易于扩展：可以轻松添加其他输入设备
 *   3. 易于调试：触摸问题在 gt911.c 中排查
 *   4. 易于移植：更换触摸芯片只需修改底层驱动
 */
