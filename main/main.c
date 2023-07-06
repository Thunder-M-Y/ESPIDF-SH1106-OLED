#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "sdkconfig.h"

// OLED I2C地址
#define OLED_I2C_ADDR 0x3C

// SDA引脚
#define I2C_SDA_PIN 18

// SCL引脚
#define I2C_SCL_PIN 17

// OLED屏幕宽度和高度
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// I2C通信相关定义
#define I2C_MASTER_NUM 0
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_MASTER_TIMEOUT_MS 1000

// OLED命令定义
#define OLED_COMMAND_MODE 0x00
#define OLED_DATA_MODE 0x40

// OLED缓冲区
uint8_t oled_buffer[128 * 64 / 8];

// I2C初始化
static void i2c_master_init()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA_PIN;
    conf.scl_io_num = I2C_SCL_PIN;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = 0; // 或者根据硬件支持进行设置
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

// OLED写命令
static void oled_write_command(uint8_t command)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, OLED_I2C_ADDR << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, OLED_COMMAND_MODE, true);
    i2c_master_write_byte(cmd, command, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, I2C_MASTER_TIMEOUT_MS / portMAX_DELAY);
    i2c_cmd_link_delete(cmd);
}

// OLED写数据
static void oled_write_data(uint8_t *data, size_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, OLED_I2C_ADDR << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, OLED_DATA_MODE, true);
    i2c_master_write(cmd, data, size, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, I2C_MASTER_TIMEOUT_MS / portMAX_DELAY);
    i2c_cmd_link_delete(cmd);
}

// OLED初始化
static void oled_init()
{
    oled_write_command(0xAE); // 关闭显示
    oled_write_command(0xD5); // 设置时钟分频因子
    oled_write_command(0x80); // 默认值
    oled_write_command(0xA8); // 设置驱动引脚和屏幕高度
    oled_write_command(0x3F); // 默认值
    oled_write_command(0xD3); // 设置显示偏移
    oled_write_command(0x00); // 默认值
    oled_write_command(0x40); // 设置显示起始行
    oled_write_command(0x8D); // 设置电荷泵
    oled_write_command(0x14); // 启用电荷泵
    oled_write_command(0x20); // 设置内存地址模式
    oled_write_command(0x00); // 水平模式
    oled_write_command(0xA1); // 设置段重定向
    oled_write_command(0xC8); // 设置COM扫描方向
    oled_write_command(0xDA); // 设置COM引脚硬件配置
    oled_write_command(0x12); // 默认值
    oled_write_command(0x81); // 设置对比度控制
    oled_write_command(0xCF); // 默认值
    oled_write_command(0xD9); // 设置预充电周期
    oled_write_command(0xF1); // 默认值
    oled_write_command(0xDB); // 设置VCOMH Deselect Level
    oled_write_command(0x40); // 默认值
    oled_write_command(0xA4); // 全局显示开启
    oled_write_command(0xA6); // 设置显示方式
    oled_write_command(0xAF); // 开启显示
}

// 清空OLED缓冲区
static void oled_clear_buffer()
{
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

// 在OLED缓冲区中设置像素点
static void oled_set_pixel(int x, int y, uint8_t value)
{
    if (x >= 0 && x < OLED_WIDTH && y >= 0 && y < OLED_HEIGHT)
    {
        if (value)
            oled_buffer[x + (y / 8) * OLED_WIDTH] |= (1 << (y % 8));
        else
            oled_buffer[x + (y / 8) * OLED_WIDTH] &= ~(1 << (y % 8));
    }
}

// 在OLED缓冲区中绘制水平线
static void oled_draw_hline(int x, int y, int width, uint8_t value)
{
    for (int i = x; i < x + width; i++)
    {
        oled_set_pixel(i, y, value);
    }
}

// 在OLED缓冲区中绘制垂直线
static void oled_draw_vline(int x, int y, int height, uint8_t value)
{
    for (int i = y; i < y + height; i++)
    {
        oled_set_pixel(x, i, value);
    }
}

// 更新OLED显示
static void oled_update_display()
{
    for (int page = 0; page < OLED_HEIGHT / 8; page++)
    {
        oled_write_command(0xB0 + page); // 设置页地址
        oled_write_command(0x01);        // 设置低列地址  ,0x00时屏幕右侧有一条竖线乱码
        oled_write_command(0x10);        // 设置高列地址

        uint8_t *page_data = &oled_buffer[page * OLED_WIDTH / 8];

        oled_write_data(page_data, OLED_WIDTH);
    }
}

void app_main(void)
{
    i2c_master_init(); // 初始化I2C总线
    oled_init();       // 初始化OLED屏幕

    oled_clear_buffer();                                // 清空OLED缓冲区
    oled_draw_hline(0, 0, OLED_WIDTH, 1);               // 绘制顶部水平线
    oled_draw_hline(0, OLED_HEIGHT - 1, OLED_WIDTH, 1); // 绘制底部水平线
    oled_draw_vline(0, 0, OLED_HEIGHT, 1);              // 绘制左侧垂直线
    oled_draw_vline(OLED_WIDTH - 1, 0, OLED_HEIGHT, 1); // 绘制右侧垂直线
    oled_update_display();                              // 更新OLED显示

    while (1)
    {
        vTaskDelay(1000 / portMAX_DELAY);
    }
}
