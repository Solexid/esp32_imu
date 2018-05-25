#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "st7735.h"
#include "app_wifi.h"
#include "driver/gpio.h"

#include "imu_task.h"

#define BUTTON_1        35
#define BUTTON_2        34
#define BUTTON_3        39

#define BUTTON_PIN_SEL    ((1ULL << BUTTON_1) | (1ULL << BUTTON_2) | (1ULL << BUTTON_3))

static const char* TAG = "lcd driver";
static xQueueHandle gpio_evt_queue = NULL;

static void
IRAM_ATTR gpio_isr_handler(void* arg)
{
  uint32_t gpio_num = (uint32_t) arg;

  xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}


static void
print_page1(void)
{
  tcpip_adapter_ip_info_t   info;
  bool                      is_configured;
  char                      ssid[32];
  char                      buffer[32];

  app_wifi_get_config(&info,  ssid, &is_configured);

  st7735_drawstring(0, 0, "System Ready         ", ST7735_WHITE);

  sprintf(buffer, "SSID:%-16s", ssid);
  st7735_drawstring(0, 1, buffer, ST7735_WHITE);

  if(is_configured)
  {
    st7735_drawstring(0, 2, "IP Addreess Info     ", ST7735_WHITE);

    sprintf(buffer, "IP  :%-16s", ip4addr_ntoa(&info.ip));
    st7735_drawstring(0, 3, buffer, ST7735_WHITE);

    sprintf(buffer, "Mask:%-16s", ip4addr_ntoa(&info.netmask));
    st7735_drawstring(0, 4, buffer, ST7735_WHITE);

    sprintf(buffer, "GW  :%-16s", ip4addr_ntoa(&info.gw));
    st7735_drawstring(0, 5, buffer, ST7735_WHITE);
  }
  else
  {
    st7735_drawstring(0, 2, "Obtaining IP Address ", ST7735_WHITE);
    st7735_drawstring(0, 3, "                     ", ST7735_WHITE);
    st7735_drawstring(0, 4, "                     ", ST7735_WHITE);
    st7735_drawstring(0, 5, "                     ", ST7735_WHITE);
  }
}

static void
print_page2(void)
{
  imu_sensor_data_t raw;
  imu_data_t        data;
  char                      buffer[32];
  imu_mode_t        mode;

  imu_task_get_raw_and_data(&mode, &raw, &data);

  if(mode == imu_mode_normal)
  {
    st7735_drawstring(0, 0, "Page 2               ", ST7735_WHITE);

    sprintf(buffer, "Roll : %-5.2f", data.orientation[0]);
    st7735_drawstring(0, 1, buffer, ST7735_WHITE);

    sprintf(buffer, "Pitch: %-5.2f", data.orientation[1]);
    st7735_drawstring(0, 2, buffer, ST7735_WHITE);

    sprintf(buffer, "Yaw  : %-5.2f", data.orientation[2]);
    st7735_drawstring(0, 3, buffer, ST7735_WHITE);
  }
  else
  {
    st7735_drawstring(0, 0, "Page 2               ", ST7735_WHITE);
    switch(mode)
    {
    case imu_mode_gyro_calibrating:
      st7735_drawstring(0, 1, "Calibrating Gyro     ", ST7735_WHITE);
      break;

    case imu_mode_accel_calibrating:
      st7735_drawstring(0, 1, "Calibrating Accelero ", ST7735_WHITE);
      break;

    case imu_mode_mag_calibrating:
      st7735_drawstring(0, 1, "Calibrating Magneto  ", ST7735_WHITE);
      break;

    default:
      st7735_drawstring(0, 1, "BUG................  ", ST7735_WHITE);
      break;
    }
    st7735_drawstring(0, 2, "                     ", ST7735_WHITE);
    st7735_drawstring(0, 3, "                     ", ST7735_WHITE);
  }
}

static void
print_page3(void)
{
  st7735_drawstring(0, 0, "Page 3               ", ST7735_WHITE);
}

static  void
lcd_driver_task(void *pvParameter)
{
  uint32_t pin_num,
           page_num = 1;

  st7735_initr(INITR_144GREENTAB);
  st7735_fillscreen(ST7735_BLACK);

  st7735_setrotation(0) ;

  st7735_drawstring(0, 0, "Booting up...        ", ST7735_WHITE);

  while(1)
  {
    switch(page_num)
    {
    case 1:
      print_page1();
      break;

    case 2:
      print_page2();
      break;

    case 3:
      print_page3();
      break;
    }

    if(xQueueReceive(gpio_evt_queue, &pin_num, 500 / portTICK_PERIOD_MS))
    {
      switch(pin_num)
      {
      case BUTTON_1:
        page_num = 1;
        break;

      case BUTTON_2:
        page_num = 2;
        break;

      case BUTTON_3:
        // page_num = 3;
        imu_task_do_mag_calibration();
        break;
      }
      st7735_fillscreen(ST7735_BLACK);
    }

  }
}

void
lcd_driver_init(void)
{
  gpio_config_t io_conf;

  io_conf.intr_type     = GPIO_PIN_INTR_NEGEDGE;
  io_conf.pin_bit_mask  = BUTTON_PIN_SEL;
  io_conf.mode          = GPIO_MODE_INPUT;
  io_conf.pull_down_en  = 0;
  io_conf.pull_up_en    = 0;
  gpio_config(&io_conf);

  gpio_install_isr_service(0);

  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

  gpio_isr_handler_add(BUTTON_1, gpio_isr_handler, (void*)BUTTON_1);
  gpio_isr_handler_add(BUTTON_2, gpio_isr_handler, (void*)BUTTON_2);
  gpio_isr_handler_add(BUTTON_3, gpio_isr_handler, (void*)BUTTON_3);

  ESP_LOGI(TAG, "starting lcd driver");

  xTaskCreate(lcd_driver_task, "lcd_driver_task", 4096, NULL, 5, NULL);
}
