#ifndef ESP_HAL_H
#define ESP_HAL_H

// include RadioLib
#include <RadioLib.h>
#include <driver/spi_master.h>

// modify from
// I'm using ESP-IDF API here
// https://github.com/jgromes/RadioLib/blob/aca1d78a9742e17947eaf5cddb63a10f10e3fafe/examples/NonArduino/ESP-IDF/main/EspHal.h
#if CONFIG_IDF_TARGET_ESP32C3 == 0
#error Target is not ESP32C3!
#endif

// include all the dependencies
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp32/rom/gpio.h"
#include "soc/rtc.h"
#include "soc/spi_reg.h"
#include "soc/spi_struct.h"
#include "driver/gpio.h"
#include "hal/gpio_hal.h"
#include "esp_timer.h"
#include "esp_log.h"

// define Arduino-style macros
#define LOW                      (0x0)
#define HIGH                     (0x1)
#define INPUT                    (0x01)
#define OUTPUT                   (0x03)
#define RISING                   (0x01)
#define FALLING                  (0x02)
#define NOP()                    asm volatile("nop")

#define MATRIX_DETACH_OUT_SIG    (0x100)
#define MATRIX_DETACH_IN_LOW_PIN (0x30)

// all the following is needed to calculate SPI clock divider
#define ClkRegToFreq(reg) (apb_freq / (((reg)->clkdiv_pre + 1) * ((reg)->clkcnt_n + 1)))

typedef union {
  uint32_t value;
  struct {
    uint32_t clkcnt_l : 6;
    uint32_t clkcnt_h : 6;
    uint32_t clkcnt_n : 6;
    uint32_t clkdiv_pre : 13;
    uint32_t clk_equ_sysclk : 1;
  };
} spiClk_t;

uint32_t getApbFrequency();

uint32_t spiFrequencyToClockDiv(uint32_t freq);

// create a new ESP-IDF hardware abstraction layer
// the HAL must inherit from the base RadioLibHal class
// and implement all of its virtual methods
// this is pretty much just copied from Arduino ESP32 core
class ESPHal : public RadioLibHal {
private:
  // the HAL can contain any additional private members
  static constexpr decltype(SPI2_HOST) SPI_HOST = SPI2_HOST;
  int8_t spiSCK;
  int8_t spiMISO;
  int8_t spiMOSI;
  spi_device_handle_t spi;

public:
  // default constructor - initializes the base HAL and any needed private members
  ESPHal(int8_t sck, int8_t miso, int8_t mosi)
      : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING),
        spiSCK(sck), spiMISO(miso), spiMOSI(mosi) {
  }

  void init() override {
    // we only need to init the SPI here
    spiBegin();
  }

  void term() override {
    // we only need to stop the SPI here
    spiEnd();
  }

  // GPIO-related methods (pinMode, digitalWrite etc.) should check
  // RADIOLIB_NC as an alias for non-connected pins
  void pinMode(uint32_t pin, uint32_t mode) override {
    if (pin == RADIOLIB_NC) {
      return;
    }

    gpio_hal_context_t gpiohal;
    gpiohal.dev = GPIO_LL_GET_HW(GPIO_PORT_0);

    gpio_config_t conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode         = (gpio_mode_t)mode,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = (gpio_int_type_t)gpiohal.dev->pin[pin].int_type,
    };
    gpio_config(&conf);
  }

  void digitalWrite(uint32_t pin, uint32_t value) override {
    if (pin == RADIOLIB_NC) {
      return;
    }

    gpio_set_level((gpio_num_t)pin, value);
  }

  uint32_t digitalRead(uint32_t pin) override {
    if (pin == RADIOLIB_NC) {
      return (0);
    }

    return (gpio_get_level((gpio_num_t)pin));
  }

  void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override {
    if (interruptNum == RADIOLIB_NC) {
      return;
    }

    gpio_install_isr_service((int)ESP_INTR_FLAG_IRAM);
    gpio_set_intr_type((gpio_num_t)interruptNum, (gpio_int_type_t)(mode & 0x7));

    // this uses function typecasting, which is not defined when the functions have different signatures
    // untested and might not work
    gpio_isr_handler_add((gpio_num_t)interruptNum, (void (*)(void *))interruptCb, NULL);
  }

  void detachInterrupt(uint32_t interruptNum) override {
    if (interruptNum == RADIOLIB_NC) {
      return;
    }

    gpio_isr_handler_remove((gpio_num_t)interruptNum);
    gpio_wakeup_disable((gpio_num_t)interruptNum);
    gpio_set_intr_type((gpio_num_t)interruptNum, GPIO_INTR_DISABLE);
  }

  void delay(unsigned long ms) override {
    vTaskDelay(ms / portTICK_PERIOD_MS);
  }

  void delayMicroseconds(unsigned long us) override {
    uint64_t m = (uint64_t)esp_timer_get_time();
    if (us) {
      uint64_t e = (m + us);
      if (m > e) { // overflow
        while ((uint64_t)esp_timer_get_time() > e) {
          NOP();
        }
      }
      while ((uint64_t)esp_timer_get_time() < e) {
        NOP();
      }
    }
  }

  unsigned long millis() override {
    return ((unsigned long)(esp_timer_get_time() / 1000ULL));
  }

  unsigned long micros() override {
    return ((unsigned long)(esp_timer_get_time()));
  }

  long pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) override {
    if (pin == RADIOLIB_NC) {
      return (0);
    }

    this->pinMode(pin, INPUT);
    uint32_t start   = this->micros();
    uint32_t curtick = this->micros();

    while (this->digitalRead(pin) == state) {
      if ((this->micros() - curtick) > timeout) {
        return (0);
      }
    }

    return (this->micros() - start);
  }

  void spiBegin() override {
    constexpr auto TAG = "ESPHal::spiBegin";
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = this->spiMOSI,
        .miso_io_num     = this->spiMISO,
        .sclk_io_num     = this->spiSCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 32,
    };

    // it might be initialized already
    auto ret = spi_bus_initialize(SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK){
      ESP_LOGW("spi_bus_initialize", "%s (%d)", esp_err_to_name(ret), ret);
    }

    spi_device_interface_config_t dev_cfg = {
        .command_bits   = 0,
        .address_bits   = 0,
        .mode           = 0,
        .clock_speed_hz = 2000000,
        .spics_io_num   = -1, // trigger CS manually
        .queue_size = 7
    };
    ret = spi_bus_add_device(SPI_HOST, &dev_cfg, &spi);
    ESP_ERROR_CHECK(ret);
  }

  void spiBeginTransaction() override {
    // not needed - in ESP32 Arduino core, this function
    // repeats clock div, mode and bit-order configuration
  }

  uint8_t spiTransferByte(uint8_t b) {
    spi_transaction_t trans = {
        .cmd      = 0,
        .addr     = 0,
        .length   = 8,
        .rxlength = 8,
    };
    trans.tx_data[0] = b;
    // acquire finite time not supported now
    spi_device_acquire_bus(spi, portMAX_DELAY);
    auto ret = spi_device_polling_transmit(spi, &trans);
    ESP_ERROR_CHECK(ret);
    spi_device_release_bus(spi);
    return trans.rx_data[0];
  }

  void spiTransfer(uint8_t *out, size_t len, uint8_t *in) override {
    spi_transaction_t trans = {
        .cmd    = 0,
        .addr   = 0,
        .length = len * 8,
        // 0 defaults this to the value of `length`
        .rxlength  = 0,
        .tx_buffer = out,
        .rx_buffer = in,
    };
    spi_device_acquire_bus(spi, portMAX_DELAY);
    auto ret = spi_device_polling_transmit(spi, &trans);
    ESP_ERROR_CHECK(ret);
    spi_device_release_bus(spi);
  }

  void spiEndTransaction() override {
    // nothing needs to be done here
  }

  void spiEnd() override {
    spi_bus_remove_device(spi);
    spi_bus_free(SPI_HOST);
  }

  void yield() override {
    taskYIELD();
  }
};

#endif
