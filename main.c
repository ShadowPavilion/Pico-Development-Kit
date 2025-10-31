#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/mutex.h"
#include "pico/sem.h"

#include "FreeRTOS.h" /* Must come first. */
#include "task.h"     /* RTOS task related API prototypes. */
#include "queue.h"    /* RTOS queue related API prototypes. */
#include "timers.h"   /* Software timer related API prototypes. */
#include "semphr.h"   /* Semaphore related API prototypes. */

#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"

#include "ws2812.pio.h"

// LVGL Mutex - Ensures thread safety (required by LVGL official documentation)
SemaphoreHandle_t lvgl_mutex = NULL;

void vApplicationTickHook(void)
{
    lv_tick_inc(1);
}

lv_obj_t *img1 = NULL;  // Startup splash image

lv_obj_t *led1 = NULL;
lv_obj_t *led2 = NULL;

lv_obj_t *jy_label = NULL;
lv_obj_t *joystick_circle = NULL;  // Joystick outer circle
lv_obj_t *joystick_ball = NULL;    // Joystick inner ball

uint8_t adc_en = 0;

// Forward function declarations
static void reboot_handler(lv_event_t *e);

// Calculator related variables
lv_obj_t *calc_display = NULL;
char calc_buffer[32] = "0";
double calc_num1 = 0;
double calc_num2 = 0;
char calc_operator = 0;
uint8_t calc_new_number = 1;

// Calculator button event handler
static void calc_btn_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *btn = lv_event_get_target(e);
        const char *txt = lv_label_get_text(lv_obj_get_child(btn, 0));
        
        if (txt[0] >= '0' && txt[0] <= '9') {
            // Number button
            if (calc_new_number) {
                calc_buffer[0] = txt[0];
                calc_buffer[1] = '\0';
                calc_new_number = 0;
            } else {
                if (strlen(calc_buffer) < 15) {
                    strcat(calc_buffer, txt);
                }
            }
        } else if (txt[0] == '.') {
            // Decimal point
            if (strchr(calc_buffer, '.') == NULL && strlen(calc_buffer) < 15) {
                strcat(calc_buffer, ".");
            }
        } else if (txt[0] == 'C') {
            // Clear
            strcpy(calc_buffer, "0");
            calc_num1 = 0;
            calc_num2 = 0;
            calc_operator = 0;
            calc_new_number = 1;
        } else if (txt[0] == '=') {
            // Equals
            if (calc_operator) {
                calc_num2 = atof(calc_buffer);
                switch (calc_operator) {
                    case '+': calc_num1 = calc_num1 + calc_num2; break;
                    case '-': calc_num1 = calc_num1 - calc_num2; break;
                    case '*': calc_num1 = calc_num1 * calc_num2; break;
                    case '/': if (calc_num2 != 0) calc_num1 = calc_num1 / calc_num2; break;
                }
                // Smart display: max 2 decimal places, auto-remove trailing zeros
                snprintf(calc_buffer, 32, "%.2f", calc_num1);
                // Remove trailing zeros
                char *p = calc_buffer + strlen(calc_buffer) - 1;
                while (*p == '0' && p > calc_buffer) *p-- = '\0';
                // Remove decimal point if no digits after it
                if (*p == '.') *p = '\0';
                
                calc_operator = 0;
                calc_new_number = 1;
            }
        } else {
            // Operator
            if (calc_operator && !calc_new_number) {
                calc_num2 = atof(calc_buffer);
                switch (calc_operator) {
                    case '+': calc_num1 = calc_num1 + calc_num2; break;
                    case '-': calc_num1 = calc_num1 - calc_num2; break;
                    case '*': calc_num1 = calc_num1 * calc_num2; break;
                    case '/': if (calc_num2 != 0) calc_num1 = calc_num1 / calc_num2; break;
                }
                // Smart display: max 2 decimal places, auto-remove trailing zeros
                snprintf(calc_buffer, 32, "%.2f", calc_num1);
                // Remove trailing zeros
                char *p = calc_buffer + strlen(calc_buffer) - 1;
                while (*p == '0' && p > calc_buffer) *p-- = '\0';
                // Remove decimal point if no digits after it
                if (*p == '.') *p = '\0';
            } else {
                calc_num1 = atof(calc_buffer);
            }
            calc_operator = txt[0];
            calc_new_number = 1;
        }
        
        lv_label_set_text(calc_display, calc_buffer);
    }
}

static void calculator_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {  // Prevent duplicate deletion of img1
        if (img1 != NULL){
            lv_obj_del(img1);
            img1 = NULL;
        }
        lv_obj_clean(lv_scr_act());
        // vTaskDelay(100 / portTICK_PERIOD_MS);
        
        // Create display screen
        calc_display = lv_label_create(lv_scr_act());
        lv_label_set_text(calc_display, "0");
        lv_obj_set_style_text_font(calc_display, &lv_font_montserrat_16, 0);  // Use 16pt font (enabled in config)
        //lv_obj_set_style_text_color(calc_display, lv_color_white(), 0);  // White text
        lv_obj_set_style_text_align(calc_display, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_width(calc_display, 300);
        lv_obj_align(calc_display, LV_ALIGN_TOP_MID, 0, 20);
        
        // Button layout: 4x4 grid + bottom equals button
        const char *btnm_map[] = {
            "7", "8", "9", "/",
            "4", "5", "6", "*",
            "1", "2", "3", "-",
            "C", "0", ".", "+"
        };
        
        int btn_w = 70;
        int btn_h = 60;
        int start_x = 10;
        int start_y = 80;
        int gap = 10;
        
        // Create 4x4 button grid
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 4; col++) {
                int idx = row * 4 + col;
                
                lv_obj_t *btn = lv_btn_create(lv_scr_act());
                lv_obj_set_size(btn, btn_w, btn_h);
                lv_obj_set_pos(btn, start_x + col * (btn_w + gap), start_y + row * (btn_h + gap));
                lv_obj_add_event_cb(btn, calc_btn_event_handler, LV_EVENT_ALL, NULL);
                
                lv_obj_t *label = lv_label_create(btn);
                lv_label_set_text(label, btnm_map[idx]);
                lv_obj_center(label);
                
                // Number buttons: white background + black text
                if (btnm_map[idx][0] >= '0' && btnm_map[idx][0] <= '9') {
                    lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
                    lv_obj_set_style_text_color(label, lv_color_black(), 0);
                } 
                // Decimal point: white background + black text
                else if (btnm_map[idx][0] == '.') {
                    lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
                    lv_obj_set_style_text_color(label, lv_color_black(), 0);
                } 
                // Operators and clear: black background + white text
                else {
                    lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
                    lv_obj_set_style_text_color(label, lv_color_white(), 0);
                }
            }
        }
        
        // Equals button spans full width: blue background + white text
        lv_obj_t *btn_eq = lv_btn_create(lv_scr_act());
        lv_obj_set_size(btn_eq, btn_w * 4 + gap * 3, btn_h);
        lv_obj_set_pos(btn_eq, start_x, start_y + 4 * (btn_h + gap));
        lv_obj_add_event_cb(btn_eq, calc_btn_event_handler, LV_EVENT_ALL, NULL);
        lv_obj_set_style_bg_color(btn_eq, lv_color_make(0, 120, 215), 0);  // Blue background
        
        lv_obj_t *label_eq = lv_label_create(btn_eq);
        lv_label_set_text(label_eq, "=");
        lv_obj_center(label_eq);
        lv_obj_set_style_text_color(label_eq, lv_color_white(), 0);  // White text
        
        // Reset button - bottom, red background + white text
        lv_obj_t *calc_reboot_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(calc_reboot_btn, btn_w * 4 + gap * 3, btn_h);
        lv_obj_set_pos(calc_reboot_btn, start_x, start_y + 5 * (btn_h + gap));
        lv_obj_add_event_cb(calc_reboot_btn, reboot_handler, LV_EVENT_ALL, NULL);
        lv_obj_set_style_bg_color(calc_reboot_btn, lv_color_make(220, 53, 69), 0);  // Red background
        
        lv_obj_t *calc_reboot_label = lv_label_create(calc_reboot_btn);
        lv_label_set_text(calc_reboot_label, "RESET");
        lv_obj_center(calc_reboot_label);
        lv_obj_set_style_text_color(calc_reboot_label, lv_color_white(), 0);  // White text
    }
}

static void reboot_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        // Force reboot using Pico SDK watchdog
        // Enable watchdog with 1ms timeout
        watchdog_enable(1, false);
        
        // Wait for watchdog to trigger reboot (after ~1ms)
        while(1) {
            tight_loop_contents();
        }
    }
}

static void beep_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED)
    {
        gpio_xor_mask(0x2000);
    }
}

static inline void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) |
           ((uint32_t)(g) << 16) |
           (uint32_t)(b);
}

static void slider_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        lv_color_t color = lv_colorwheel_get_rgb(obj);
        put_pixel(urgb_u32(color.ch.red << 5, ((color.ch.green_h << 2) + color.ch.green_h) << 2, (color.ch.blue << 3)));
    }
}

static void clr_rgb_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED)
    {
        put_pixel(urgb_u32(0, 0, 0));
    }
}

// 防抖：记录上次按键触发时间（毫秒）
static uint32_t last_button_time_gpio14 = 0;
static uint32_t last_button_time_gpio15 = 0;
#define DEBOUNCE_DELAY_MS 50  // 防抖延迟50毫秒

void gpio_callback(uint gpio, uint32_t events)
{
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    switch (gpio)
    {
    case 15:
        // 防抖检查：只处理上升沿，且距离上次触发超过DEBOUNCE_DELAY_MS
        if ((events & GPIO_IRQ_EDGE_RISE) && 
            (current_time - last_button_time_gpio15 > DEBOUNCE_DELAY_MS))
        {
            last_button_time_gpio15 = current_time;
            lv_led_toggle(led1);
            gpio_xor_mask(1ul << 16);
        }
        break;
    case 14:
        // 防抖检查：只处理上升沿，且距离上次触发超过DEBOUNCE_DELAY_MS
        if ((events & GPIO_IRQ_EDGE_RISE) && 
            (current_time - last_button_time_gpio14 > DEBOUNCE_DELAY_MS))
        {
            last_button_time_gpio14 = current_time;
            lv_led_toggle(led2);
            gpio_xor_mask(1ul << 17);
        }
        break;
    }
}

static void hw_handler(lv_event_t *e)
{
    lv_obj_t *label;

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED)
    {
        if (img1 !=NULL) {
            lv_obj_del(img1);
            img1 = NULL;
        }
        lv_obj_clean(lv_scr_act());
        // vTaskDelay(100 / portTICK_PERIOD_MS);

        // Reset button - top left corner, red background + white text
        lv_obj_t *reboot_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(reboot_btn, 80, 35);  // Small size
        lv_obj_align(reboot_btn, LV_ALIGN_TOP_LEFT, 10, 10);  // Top left
        lv_obj_add_event_cb(reboot_btn, reboot_handler, LV_EVENT_ALL, NULL);
        lv_obj_set_style_bg_color(reboot_btn, lv_color_make(220, 53, 69), 0);  // Red background
        
        lv_obj_t *reboot_label = lv_label_create(reboot_btn);
        lv_label_set_text(reboot_label, "RESET");
        lv_obj_center(reboot_label);
        lv_obj_set_style_text_color(reboot_label, lv_color_white(), 0);  // White text

        // Buzzer
        gpio_init(13);
        gpio_set_dir(13, GPIO_OUT);

        lv_obj_t *beep_btn = lv_btn_create(lv_scr_act());
        lv_obj_add_event_cb(beep_btn, beep_handler, LV_EVENT_ALL, NULL);
        lv_obj_align(beep_btn, LV_ALIGN_TOP_MID, 0, 40);
        lv_obj_add_flag(beep_btn, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_height(beep_btn, LV_SIZE_CONTENT);

        label = lv_label_create(beep_btn);
        lv_label_set_text(label, "Beep");
        lv_obj_center(label);

        // Clear RGB color
        lv_obj_t *clr_rgb_btn = lv_btn_create(lv_scr_act());
        lv_obj_add_event_cb(clr_rgb_btn, clr_rgb_handler, LV_EVENT_ALL, NULL);
        lv_obj_align(clr_rgb_btn, LV_ALIGN_TOP_MID, 0, 80);

        label = lv_label_create(clr_rgb_btn);
        lv_label_set_text(label, "Turn off RGB");
        lv_obj_center(label);

        // RGB LED

        /*Create a slider in the center of the display*/
        lv_obj_t *lv_colorwheel = lv_colorwheel_create(lv_scr_act(), true);
        lv_obj_set_size(lv_colorwheel, 200, 200);
        lv_obj_align(lv_colorwheel, LV_ALIGN_TOP_MID, 100, 0);

        lv_obj_center(lv_colorwheel);
        lv_obj_add_event_cb(lv_colorwheel, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

        // todo get free sm
        PIO pio = pio0;
        int sm = 0;
        uint offset = pio_add_program(pio, &ws2812_program);

        ws2812_program_init(pio, sm, offset, 12, 800000, true);
        put_pixel(urgb_u32(0, 0, 0));

        gpio_set_irq_enabled_with_callback(14, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
        gpio_set_irq_enabled_with_callback(15, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
        gpio_set_irq_enabled_with_callback(22, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

        led1 = lv_led_create(lv_scr_act());
        lv_obj_align(led1, LV_ALIGN_TOP_MID, -30, 400);
        lv_led_set_color(led1, lv_palette_main(LV_PALETTE_GREEN));

        lv_led_off(led1);

        led2 = lv_led_create(lv_scr_act());
        lv_obj_align(led2, LV_ALIGN_TOP_MID, 30, 400);
        lv_led_set_color(led2, lv_palette_main(LV_PALETTE_BLUE));

        lv_led_off(led2);

        gpio_init(16);
        gpio_init(17);

        gpio_set_dir(16, GPIO_OUT);
        gpio_set_dir(17, GPIO_OUT);

        gpio_put(16, 0);
        gpio_put(17, 0);

        adc_en = 1;

        // Circular joystick outer frame
        joystick_circle = lv_obj_create(lv_scr_act());
        lv_obj_set_size(joystick_circle, 100, 100);
        lv_obj_align(joystick_circle, LV_ALIGN_TOP_MID, 0, 190);
        lv_obj_set_style_bg_color(joystick_circle, lv_color_white(), 0);  // White background
        lv_obj_set_style_border_color(joystick_circle, lv_color_black(), 0);  // Black border
        lv_obj_set_style_border_width(joystick_circle, 2, 0);
        lv_obj_set_style_radius(joystick_circle, LV_RADIUS_CIRCLE, 0);  // Circular
        lv_obj_set_style_pad_all(joystick_circle, 0, 0);  // Remove padding
        lv_obj_clear_flag(joystick_circle, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrollbar

        // Blue circular indicator ball
        joystick_ball = lv_obj_create(joystick_circle);
        lv_obj_set_size(joystick_ball, 12, 12);  // Slightly larger for visibility
        lv_obj_set_pos(joystick_ball, 44, 44);  // Center position: (100-12)/2 = 44
        lv_obj_set_style_bg_color(joystick_ball, lv_color_make(0, 0, 255), 0);  // Blue color
        lv_obj_set_style_border_width(joystick_ball, 0, 0);
        lv_obj_set_style_radius(joystick_ball, LV_RADIUS_CIRCLE, 0);  // Circular
        lv_obj_set_style_pad_all(joystick_ball, 0, 0);  // Ensure no padding on ball

        lv_obj_t *btn_label = lv_label_create(lv_scr_act());
        lv_label_set_text(btn_label, "Press Button to Toggle LED!");
        lv_obj_set_style_text_align(btn_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(btn_label, LV_ALIGN_TOP_MID, 0, 380);  // Above LED
    }
}

void lv_example_btn_1(void)
{
    lv_obj_t *label;

    // Hardware Demo button
    lv_obj_t *hw_btn = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(hw_btn, hw_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(hw_btn, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(hw_btn, lv_color_white(), 0);  // White background
    
    label = lv_label_create(hw_btn);
    lv_label_set_text(label, "Hardware Demo");
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);  // Black text
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);  // Larger font (default is 14)
    lv_obj_set_style_text_letter_space(label, 1, 0);  // Letter spacing for bolder look

    // Calculator button
    lv_obj_t *calc_btn = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(calc_btn, calculator_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(calc_btn, LV_ALIGN_TOP_MID, 0, 90);
    lv_obj_set_style_bg_color(calc_btn, lv_color_white(), 0);  // White background

    label = lv_label_create(calc_btn);
    lv_label_set_text(label, "Calculator");
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);  // Black text
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);  // Larger font (default is 14)
    lv_obj_set_style_text_letter_space(label, 1, 0);  // Letter spacing for bolder look
}

void task0(void *pvParam)
{
    // Lock mutex when initializing UI
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    lv_obj_clean(lv_scr_act());
    xSemaphoreGive(lvgl_mutex);
    
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Lock mutex when creating initial UI
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    img1 = lv_img_create(lv_scr_act());
    LV_IMG_DECLARE(sea);
    lv_img_set_src(img1, &sea);
    lv_obj_align(img1, LV_ALIGN_DEFAULT, 0, 0);
    lv_example_btn_1();
    xSemaphoreGive(lvgl_mutex);

    for (;;)
    {
        if (adc_en)
        {
            adc_init();
            // Make sure GPIO is high-impedance, no pullups etc
            adc_gpio_init(26);
            adc_gpio_init(27);

            for (;;)
            {
                char buf[50];

                // ADC read doesn't need mutex lock
                adc_select_input(0);
                uint adc_x_raw = adc_read();
                adc_select_input(1);
                uint adc_y_raw = adc_read();

                // Simplified mapping logic - map to 0-88 range
                const uint adc_max = (1 << 12) - 1;  // 4095
                const int max_pos = 88;  // 100-12=88 (outer frame 100, ball 12)
                
                // Direct linear mapping
                int ball_x = (adc_x_raw * max_pos) / adc_max;
                int ball_y = max_pos - (adc_y_raw * max_pos) / adc_max;  // Y-axis inverted

                // Lock mutex when updating LVGL objects
                xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
                lv_obj_set_pos(joystick_ball, ball_x, ball_y);
                xSemaphoreGive(lvgl_mutex);

                vTaskDelay(200 / portTICK_PERIOD_MS);
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void task1(void *pvParam)
{
    for (;;)
    {
        // Must lock mutex before/after lv_task_handler (LVGL official requirement)
        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
        lv_task_handler();
        xSemaphoreGive(lvgl_mutex);
        
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

int main()
{
    stdio_init_all();

    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    // Create LVGL mutex (must be created before task startup)
    lvgl_mutex = xSemaphoreCreateMutex();
    if (lvgl_mutex == NULL) {
        // Mutex creation failed, system cannot run
        while(1) {
            tight_loop_contents();
        }
    }

    UBaseType_t task0_CoreAffinityMask = (1 << 0);
    UBaseType_t task1_CoreAffinityMask = (1 << 1);

    TaskHandle_t task0_Handle = NULL;

    xTaskCreate(task0, "task0", 2048, NULL, 1, &task0_Handle);
    vTaskCoreAffinitySet(task0_Handle, task0_CoreAffinityMask);

    TaskHandle_t task1_Handle = NULL;
    xTaskCreate(task1, "task1", 2048, NULL, 2, &task1_Handle);
    vTaskCoreAffinitySet(task1_Handle, task1_CoreAffinityMask);

    vTaskStartScheduler();

    return 0;
}