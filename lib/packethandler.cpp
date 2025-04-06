#include "packethandler.h"

#include <string.h>
#include <stdio.h>

#include <chrono>
#include <math.h>
#include <ctime>
#include <thread>
#include <algorithm>

PacketHandler::PacketHandler() {
  SPI *spi = NULL;

  spi_config.mode=0;
  spi_config.speed=1 * pow(10, 6); // Freq - SPI
  spi_config.delay=0;
  spi_config.bits_per_word=8;
  
  spi = new SPI("/dev/spidev0.0", &spi_config);
  spi->begin();

  uint8_t res = lora_init(&lora, spi, LORA_BASE_FREQUENCY_EU);
  if (res != LORA_OK) {
    // This should not happen
    throw "Error init";
  }
}

void PacketHandler::clean() {
  time_t now = time(NULL);

  // Remove expired messages if no response is received
  std::erase_if(messages, [now](const auto& pair) {
    return pair.first.expiration <= now;
  });
};

void PacketHandler::receive_mode() {
  lora_mode_receive_continuous(&lora);
  lora_enable_interrupt_rx_done(&lora);
}

void PacketHandler::set_long_range() {
  lora_mode_sleep(&lora);

  lora_set_spreading_factor(&lora, 8);
  lora_set_signal_bandwidth(&lora, LORA_BANDWIDTH_62_5_KHZ);
  lora_set_coding_rate(&lora, LORA_CODING_RATE_4_8);
  lora_set_tx_power(&lora, 20);
  lora_set_preamble_length(&lora, 10);
  lora_set_crc(&lora, 1);

  lora_mode_standby(&lora);
}

void PacketHandler::set_short_range() {
  lora_mode_sleep(&lora);

  lora_set_spreading_factor(&lora, 7);
  lora_set_signal_bandwidth(&lora, LORA_BANDWIDTH_125_KHZ);
  lora_set_coding_rate(&lora, LORA_CODING_RATE_4_5);
  lora_set_tx_power(&lora, 10);
  lora_set_preamble_length(&lora, 5);
  lora_set_crc(&lora, 0);

  lora_mode_standby(&lora);
}

void PacketHandler::send(uint8_t* data, uint32_t size, PacketTypes type) {
  const uint8_t MAX_SIZE = PACKET_MAX_SIZE;

  uint8_t id = rand();
  message_entry_t entry {
    id,
    (time(NULL) * 1000) + 10000 // Convert to milliseconds + 10000 // Make message stay in memory for max 10 seconds
  };

  uint32_t index     = 0;
  uint8_t  frag_idx  = 0;
  uint32_t remaining = size;

  uint8_t total_fragments = size / PACKET_MAX_SIZE;
  if (size % MAX_SIZE) {
    total_fragments++;
  }

  while (remaining > 0) {
    uint32_t packet_len = (remaining > MAX_SIZE) ? MAX_SIZE : remaining;

    packet_t* pkt = (packet_t*) malloc(sizeof(packet_t));

    pkt->type            = (uint8_t) type;
    pkt->message_id      = id;
    pkt->fragment_id     = frag_idx;
    pkt->total_fragments = total_fragments;
    pkt->lenght          = packet_len;
    memcpy(pkt->data, &data[index], packet_len);

    pkt->checksum = compute_checksum(pkt, packet_len);

    messages[entry].push_back(pkt);

    frag_idx++;

    index     += packet_len;
    remaining -= packet_len;
  }

  auto& packets = messages[entry];

  for (auto pk : packets) {
    send_pkt(pk);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
  }
}

void PacketHandler::receive(){
  uint8_t error = 0;
  uint8_t rx_buffer[PACKET_MAX_SIZE + 9];

  lora_mode_receive_continuous(&lora);

  // Blocking call: wait for an incoming packet.
  uint8_t rx_received = lora_receive_packet_blocking(&lora, rx_buffer, sizeof(rx_buffer), 1000, &error);

  if (rx_received == 0 || error != LORA_OK) {
    return;
  }
  
  // Validate the fixed header bytes.
  if (rx_buffer[0] != 0xA5 || rx_buffer[1] != 0x5A) {
    printf("Invalid header received.\n");
    return;
  }
  
  // Allocate memory for the packet structure.
  packet_t* pkt = (packet_t*) malloc(sizeof(packet_t));
  if (!pkt) {
    printf("Memory allocation failure for packet.\n");
    return;
  }
  
  // Parse the packet header.
  pkt->type            = rx_buffer[2];
  pkt->message_id      = rx_buffer[3];
  pkt->fragment_id     = rx_buffer[4];
  pkt->total_fragments = rx_buffer[5];
  pkt->lenght          = rx_buffer[6];

  // Copy the payload data.
  memcpy(pkt->data, &rx_buffer[7], pkt->lenght);
  
  // Retrieve the received checksum.
  uint8_t received_checksum = rx_buffer[pkt->lenght + 8];
  
  // Now, pkt->checksum holds the computed checksum.
  if (validate_checksum(pkt, received_checksum)) {
    printf("Checksum mismatch: computed 0x%X, received 0x%X\n", pkt->checksum, received_checksum);
    free(pkt);
    return;
  }
  
  // At this point, the packet is valid.
  printf("Received packet: message_id=%d, fragment %d/%d, length=%d\n",
         pkt->message_id, pkt->fragment_id + 1, pkt->total_fragments, pkt->lenght);
  
  printf("%s\n\r", pkt->data);

  // TODO: For multi-fragment messages, store the packet for later reassembly.
  // Example:
  //   message_entry_t entry = { pkt->message_id, /* expiration */ some_future_time };
  //   received_messages[entry].push_back(pkt);
  //
  // If this is a complete message or single fragment, process it immediately.

  // In this example, we simply free the packet after processing.
  free(pkt);

  lora_mode_standby(&lora);
}

void PacketHandler::send_pkt(packet_t* pkt) {
  uint8_t buffer[pkt->lenght + 9];

  buffer[0] = 0xA5;
  buffer[1] = 0x5A;
  buffer[2] = pkt->type;
  buffer[3] = pkt->message_id;
  buffer[4] = pkt->fragment_id;
  buffer[5] = pkt->total_fragments;
  buffer[6] = pkt->lenght;

  memcpy(&buffer[7], pkt->data, pkt->lenght);

  buffer[pkt->lenght + 8] = pkt->checksum;

  lora_enable_interrupt_tx_done(&lora);

  uint8_t res = LORA_BUSY;

  while(res == LORA_BUSY) {
    res = lora_send_packet(&lora, buffer, pkt->lenght + 9);
  }

  if (res != LORA_OK) {
    printf("Oh no pizza boi failed!\n");
  }
}

uint8_t PacketHandler::compute_checksum(packet_t* pkt, uint8_t len) {
  uint16_t sum = 0;
  uint8_t* bytes = pkt->data;

  for (size_t i = 0; i < len; ++i) {
    sum += bytes[i];
  }

  return static_cast<uint8_t>(~sum);
}

bool PacketHandler::validate_checksum(packet_t* pkt, uint16_t chksum) {
  // Recompute checksum and compare.
  uint8_t computed = compute_checksum(pkt, pkt->lenght);
  return computed == chksum;
}