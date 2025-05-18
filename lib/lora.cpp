#include "lora.h"
#include "packethandler.h"

volatile LoRaMode mode;

extern LoRa lora;
extern PacketHandler packetHandler;

void SX1276_IRQ_Handle_Task() {
  if (mode == LoRaMode::Transmit) {
    std::cout << "Transmit done" << std::endl;
    lora.set_receive_mode();
  }

  if (mode == LoRaMode::Receive) {
    std::cout << "Receive" << std::endl;
    packetHandler.receive();
  }
};

LoRa::LoRa() { 
  mode = LoRaMode::Receive;
};

void LoRa::init() {
  // wiringPi init + ISR
  if (wiringPiSetup() == -1) {
    std::cerr << "wiringPi setup failed.\n";
    return;
  }

  pinMode(7, INPUT);
  if (wiringPiISR(7, INT_EDGE_RISING, &SX1276_IRQ_Handle_Task) < 0) {
    std::cerr << "Failed to setup ISR for GPIO7.\n";
    return;
  }

  // Initialize LoRa
  spi_config.mode = 0;
  spi_config.speed = 1 * pow(10, 6); // Freq - SPI
  spi_config.delay = 0;
  spi_config.bits_per_word = 8;

  spi = std::make_shared<SPI>("/dev/spidev0.0", &spi_config);
  spi->begin();

  volatile uint8_t res = lora_init(&lora, (SPI*)spi.get(), LORA_BASE_FREQUENCY_EU);

  set_receive_mode();
}

bool LoRa::set_long_range() {
  std::lock_guard<std::mutex> guard(mutex);

  lora_mode_sleep(&lora);

  lora_set_spreading_factor(&lora, 8);
  lora_set_signal_bandwidth(&lora, LORA_BANDWIDTH_62_5_KHZ);
  lora_set_coding_rate(&lora, LORA_CODING_RATE_4_8);
  lora_set_tx_power(&lora, 20);
  lora_set_preamble_length(&lora, 10);
  lora_set_crc(&lora, 1);

  set_receive_mode();

  return true;
}

bool LoRa::set_short_range() {
  std::lock_guard<std::mutex> guard(mutex);

  lora_mode_sleep(&lora);

  lora_set_spreading_factor(&lora, 7);
  lora_set_signal_bandwidth(&lora, LORA_BANDWIDTH_125_KHZ);
  lora_set_coding_rate(&lora, LORA_CODING_RATE_4_5);
  lora_set_tx_power(&lora, 10);
  lora_set_preamble_length(&lora, 5);
  lora_set_crc(&lora, 0);

  set_receive_mode();

  return true;
}

bool LoRa::send(const uint8_t* data, uint16_t len) {
  std::lock_guard<std::mutex> guard(mutex);

  uint8_t status = lora_send_packet(&lora, (uint8_t*)data, len);
  bool succes = status == LORA_OK;

  if (succes) {
    set_transmit_mode();
  } else {
    set_receive_mode();
  }

  return succes;
}

uint8_t LoRa::receive(uint8_t* buffer, uint16_t maxLen, uint16_t timeoutMs, uint8_t* errorOut) {
  std::lock_guard<std::mutex> guard(mutex);

  return lora_receive_packet_blocking(&lora, buffer, maxLen, timeoutMs, errorOut);
}

void LoRa::set_receive_mode() {
  lora_mode_receive_continuous(&lora);
  lora_enable_interrupt_rx_done(&lora);
  mode = LoRaMode::Receive;
}

void LoRa::set_transmit_mode() {
  lora_enable_interrupt_tx_done(&lora);
  mode = LoRaMode::Transmit;
}
