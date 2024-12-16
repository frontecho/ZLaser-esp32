#include <Arduino.h>
#include <Ticker.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"

#include "SPIRenderer.h"
#include "network.h"
#include "ilda.h"

#define PIN_NUM_MISO -1
#define PIN_NUM_MOSI 25
#define PIN_NUM_CLK 26
#define PIN_NUM_CS 27
#define PIN_NUM_LDAC 33
#define PIN_NUM_LASER_R 13
#define PIN_NUM_LASER_G 16
#define PIN_NUM_LASER_B 17

// Handle for a device on a SPI bus
typedef struct spi_device_t *spi_device_handle_t;
class SPIRenderer
{
private:
  TaskHandle_t spi_task_handle;
  spi_device_handle_t spi;
  volatile int draw_position;
  volatile int frame_position;

public:
  SPIRenderer();
  void IRAM_ATTR draw();
  void start();
};

Ticker drawer;
SPIRenderer *renderer;
TaskHandle_t fileBufferHandle;

void setupRenderer()
{
  renderer = new SPIRenderer();
  renderer->start();
}

void draw_task()
{
  renderer->draw();
}

SPIRenderer::SPIRenderer()
{
  frame_position = 0;
  draw_position = 0;
}

void SPIRenderer::start()
{

  pinMode(PIN_NUM_LASER_R, OUTPUT);
  pinMode(PIN_NUM_LASER_G, OUTPUT);
  pinMode(PIN_NUM_LASER_B, OUTPUT);
  pinMode(PIN_NUM_LDAC, OUTPUT);

  // setup SPI output
  esp_err_t ret;
  spi_bus_config_t buscfg = {
      .mosi_io_num = PIN_NUM_MOSI,
      .miso_io_num = PIN_NUM_MISO,
      .sclk_io_num = PIN_NUM_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 0};
  spi_device_interface_config_t devcfg = {
      .command_bits = 0,
      .address_bits = 0,
      .dummy_bits = 0,
      .mode = 0,
      .clock_speed_hz = 40000000,
      .spics_io_num = PIN_NUM_CS, // CS pin
      .flags = SPI_DEVICE_NO_DUMMY,
      .queue_size = 2,
  };
  // Initialize the SPI bus
  ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
  printf("Ret code is %d\n", ret);
  ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
  printf("Ret code is %d\n", ret);

  xTaskCreatePinnedToCore(
    fileBufferLoop, "fileBufferHandle", 5000 // Stack size
    ,
    NULL, 3 // Priority
    ,
    &fileBufferHandle, 0
  );
}

void IRAM_ATTR SPIRenderer::draw()
{
  // Clear the interrupt
  // do we still have things to draw?
  // Serial.println(ilda->frames[frame_position].number_records);
  if (draw_position < ilda->frames[frame_position].number_records)
  {
    const ILDA_Record_t &instruction = ilda->frames[frame_position].records[draw_position];
    int y = 2048 + (instruction.x * 1024) / 32768;
    int x = 2048 + (instruction.y * 1024) / 32768;
    // Serial.print(instruction.x);
    // Serial.print(" ");
    // Serial.println(instruction.y);
    // set the laser state

    // channel A
    spi_transaction_t t1 = {};
    t1.length = 16;
    t1.flags = SPI_TRANS_USE_TXDATA;
    t1.tx_data[0] = 0b11010000 | ((x >> 8) & 0xF);
    t1.tx_data[1] = x & 255;
    spi_device_polling_transmit(spi, &t1);
    // channel B
    spi_transaction_t t2 = {};
    t2.length = 16;
    t2.flags = SPI_TRANS_USE_TXDATA;
    t2.tx_data[0] = 0b01010000 | ((y >> 8) & 0xF);
    t2.tx_data[1] = y & 255;
    spi_device_polling_transmit(spi, &t2);

    // 设置激光颜色
    if ((instruction.status_code & 0b01000000) == 0)
    {
      if (instruction.color <= 9)
      { // RED
        digitalWrite(PIN_NUM_LASER_R, LOW);
      }
      else if (instruction.color <= 18)
      { // YELLOW
        digitalWrite(PIN_NUM_LASER_R, LOW);
        digitalWrite(PIN_NUM_LASER_G, LOW);
      }
      else if (instruction.color <= 27)
      { // GREEN
        digitalWrite(PIN_NUM_LASER_G, LOW);
      }
      else if (instruction.color <= 36)
      { // CYAN
        digitalWrite(PIN_NUM_LASER_G, LOW);
        digitalWrite(PIN_NUM_LASER_B, LOW);
      }
      else if (instruction.color <= 45)
      { // BLUE
        digitalWrite(PIN_NUM_LASER_B, LOW);
      }
      else if (instruction.color <= 54)
      { // Magenta
        digitalWrite(PIN_NUM_LASER_B, LOW);
        digitalWrite(PIN_NUM_LASER_R, LOW);
      }
      else if (instruction.color <= 63)
      { // WHITE
        digitalWrite(PIN_NUM_LASER_B, LOW);
        digitalWrite(PIN_NUM_LASER_R, LOW);
        digitalWrite(PIN_NUM_LASER_G, LOW);
      }
    }
    else
    { // 不亮的Point
      digitalWrite(PIN_NUM_LASER_R, HIGH);
      digitalWrite(PIN_NUM_LASER_G, HIGH);
      digitalWrite(PIN_NUM_LASER_B, HIGH);
    }

    // DAC Load
    digitalWrite(PIN_NUM_LDAC, LOW);
    digitalWrite(PIN_NUM_LDAC, HIGH);

    draw_position++;
  }
  else
  {
    ilda->frames[frame_position].isBuffered = false;
    draw_position = 0;
    frame_position++;
    if (frame_position >= bufferFrames)
    {
      frame_position = 0;
    }
    if (!isStreaming)
    {
      xTaskNotifyGive(fileBufferHandle);
    }
  }
}
