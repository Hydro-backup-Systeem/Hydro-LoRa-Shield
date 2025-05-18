#pragma once

#include "lora_sx1276.h"
#include "wiringPi.h"

#include <mutex>
#include <memory>
#include <thread>
#include <math.h>
#include <iostream>

enum class LoRaMode {
  Receive,
  Transmit
};

extern volatile LoRaMode mode;

class LoRa {
  public:
    LoRa();

  public:
    void init();
    bool set_long_range();
    bool set_short_range();

  public:
    bool send(const uint8_t* data, uint16_t len);
    uint8_t receive(uint8_t* buffer, uint16_t maxLen, uint16_t timeoutMs, uint8_t* errorOut);

  public:
    LoRaMode current_mode() { return mode; }

  private:
    void set_receive_mode();
    void set_transmit_mode();

  private:
    friend void SX1276_IRQ_Handle_Task();

  private:
    lora_sx1276 lora;

    spi_config_t spi_config;
    std::shared_ptr<SPI> spi;

  private:
    std::mutex mutex;

};
