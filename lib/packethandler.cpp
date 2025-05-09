#include "packethandler.h"

#include <thread>
#include <math.h>
#include <cstdlib>
#include <stdio.h>
#include <cstring>
#include <algorithm>

uint32_t PacketHandler::iv[4];

PacketHandler::PacketHandler() {}

void PacketHandler::init() {
  SPI *spi = NULL;

  spi_config.mode=0;
  spi_config.speed=1 * pow(10, 6); // Freq - SPI
  spi_config.delay=0;
  spi_config.bits_per_word=8;
  
  spi = new SPI("/dev/spidev0.0", &spi_config);
  spi->begin();

  uint8_t res = lora_init(&lora, spi, LORA_BASE_FREQUENCY_EU);
  AES_init_ctx_iv(&aes_ctx, reinterpret_cast<uint8_t*>(aes_key), reinterpret_cast<uint8_t*>(iv));
  if (res != LORA_OK) {
    // This should not happen
    throw "Error init";
  }
}

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

void PacketHandler::set_msg_callback(msg_data_callback_t callback) {
  this->msg_callback = callback;
}

void swap_endianness(uint32_t* data, std::size_t word_count) {
  for (std::size_t i = 0; i < word_count; ++i) {
    uint32_t v = data[i];
    data[i] = ((v & 0x000000FFu) << 24) |
              ((v & 0x0000FF00u) <<  8) |
              ((v & 0x00FF0000u) >>  8) |
              ((v & 0xFF000000u) >> 24);
  }
}

void PacketHandler::poll() {
  // Process received multi-fragment messages.
  for (auto it = received.begin(); it != received.end(); ) {
    auto& fragments = it->second;

    if (!fragments.empty() && fragments[0]->total_fragments == fragments.size()) {
      // All fragments received; process complete message.
      // Sort the message based on id
      std::sort(fragments.begin(), fragments.end(), [](const packet_t* a, const packet_t* b) {
        return a->fragment_id < b->fragment_id;
      });

      // Calculate total length for the complete message.
      uint32_t total_length = 0;
      for (const auto pkt : fragments) {
        total_length += pkt->lenght;
      }

      // Reassembly of the data
      uint32_t offset = 0;
      uint8_t* reassembled = reinterpret_cast<uint8_t*>(malloc((total_length) * sizeof(uint8_t)));
      
      auto& front = fragments.front();
      PacketTypes type = (PacketTypes)front->type;

      for (const auto pkt : fragments) {
        memcpy(reassembled + offset, pkt->data, pkt->lenght);
        offset += pkt->lenght;
        free(pkt);
      }

      switch (type) {
        case PacketTypes::MSG:
          if (msg_callback) {
            printf("Encrypted message: ");
            for (uint32_t i = 0; i < total_length; i++) {
                printf("%02X ", reassembled[i]);
            }
            printf("\n");

            AES_CTR_xcrypt_buffer(&aes_ctx, reassembled, total_length);
            
            msg_callback(reassembled, total_length);
            free(reassembled);
          }
          break;

        case PacketTypes::ENCRYP: {
          uint32_t* iv_vector = reinterpret_cast<uint32_t *>(reassembled);
          swap_endianness(iv_vector, 4);
          
          // Initialize AES context with new IV
          memcpy(this->iv, iv_vector, front->lenght);
          AES_ctx_set_iv(&aes_ctx, reinterpret_cast<uint8_t*>(iv_vector));

          free(reassembled);
          break;
        }

        case PacketTypes::ACK:
          break;

        case PacketTypes::NACK:
          break;

        default:
          break;
      }

      it = received.erase(it);
    } else {
      it++;
    }
  }
}

void PacketHandler::clean() {
  time_t now = time(NULL);
  
  for (auto it = messages.begin(); it != messages.end(); ) {
    if (it->first.expiration <= now) {
      for (auto* pkt : it->second) {
        free(pkt);
      }
      it = messages.erase(it);  // erase returns next valid iterator
    } else {
      ++it;
    }
  }
}

void PacketHandler::u32_to_bytes(uint32_t* input, uint8_t* output) {
  for (int i = 0; i < 4; i++) {
      output[i * 4 + 0] = (input[i] >> 24) & 0xFF;
      output[i * 4 + 1] = (input[i] >> 16) & 0xFF;
      output[i * 4 + 2] = (input[i] >> 8) & 0xFF;
      output[i * 4 + 3] = input[i] & 0xFF;
  }
}

void PacketHandler::encryption_handshake(uint8_t msg_id) {
  // printf("Sending IV....\n\r");
  // Generate random values for the AES IV.
  srand((unsigned)time(NULL));
  for (int i = 0; i < 16; ++i) {
    ((uint8_t*)iv)[i] = rand() & 0xFF;
  }
  
  printf("My IV-key: 0x%04x, 0x%04x, 0x%04x, 0x%04x\n\r", iv[0], iv[1], iv[2], iv[3]);

  // Convert key and IV to byte arrays for AES
  uint8_t key_bytes[16];
  uint8_t iv_bytes[16];
  
  u32_to_bytes(aes_key, key_bytes);
  u32_to_bytes(iv, iv_bytes);
  
  // Initialize AES context with new IV
  AES_init_ctx_iv(&aes_ctx, key_bytes, iv_bytes);

  // Prepare handshake packet.
  packet_t handshakePkt;
  handshakePkt.type = static_cast<uint8_t>(PacketTypes::ENCRYP);
  handshakePkt.message_id = msg_id;
  handshakePkt.fragment_id = 0;
  handshakePkt.total_fragments = 1;
  handshakePkt.lenght = 16;  // Send full 16 bytes IV

  memcpy(handshakePkt.data, iv_bytes, handshakePkt.lenght);
  handshakePkt.checksum = compute_checksum(&handshakePkt, handshakePkt.lenght);

  // printf("Sending iv key...\n\r");
  send_pkt(&handshakePkt);
}

void PacketHandler::send(uint8_t* data, uint32_t size, PacketTypes type) {
  uint8_t id = rand();
  const uint8_t MAX_SIZE = PACKET_MAX_SIZE;

  message_entry_t entry {
    id,
    time(NULL) + 10000 // Expiration time: 10 seconds.
  };

  // Encryption handshake.
  encryption_handshake(entry.id);

  // Ensure data is padded to a multiple of 16 bytes.
  uint32_t paddedSizeBytes = ((size + 15) / 16) * 16;
  uint8_t* paddedData = new uint8_t[paddedSizeBytes]();
  memcpy(paddedData, data, size);

  if (!paddedData) {
    printf("oh no!\n");
    delete[] paddedData; 
    return;
  }

  AES_CTR_xcrypt_buffer(&aes_ctx, paddedData, paddedSizeBytes);

  uint32_t remaining     = paddedSizeBytes;
  uint32_t offset        = 0;
  uint32_t rawFragments  = (paddedSizeBytes + MAX_SIZE - 1) / MAX_SIZE;
  uint8_t totalFragments = static_cast<uint8_t>(rawFragments);
  uint8_t fragIdx        = 0;

  while (remaining) {
    uint32_t packetLen = (remaining > MAX_SIZE) ? MAX_SIZE : remaining;
    packet_t* pkt = new packet_t;
    if (!pkt) break;

    pkt->type = static_cast<uint8_t>(type);
    pkt->message_id = id;
    pkt->fragment_id = fragIdx;
    pkt->total_fragments = totalFragments;
    pkt->lenght = packetLen;

    memcpy( pkt->data, 
            paddedData + offset, 
            packetLen);

    pkt->checksum = compute_checksum(pkt, packetLen);

    messages[entry].push_back(pkt);

    // Advance
    fragIdx++;
    offset += packetLen;
    remaining -= packetLen;
  }

  delete[] paddedData;  // Changed from free() to delete[]
  // Transmit each fragment.
  for (auto pkt : messages[entry]) {
    send_pkt(pkt);
  }
}

void PacketHandler::receive() {
  uint8_t error = 0;
  uint8_t rx_buffer[PACKET_MAX_SIZE + 9];

  // Blocking call with a 500ms timeout.
  uint8_t rx_received = lora_receive_packet_blocking(&lora, rx_buffer, sizeof(rx_buffer), 500, &error);

  if (rx_received == 0 || error != LORA_OK) {
    return;
  }

  // Validate header bytes.
  if (rx_buffer[0] != 0xA5 || rx_buffer[1] != 0x5A) {
    return;
  }

  packet_t* pkt = reinterpret_cast<packet_t*>(malloc(sizeof(packet_t)));
  if (!pkt) {
    return;
  }

  pkt->type = rx_buffer[2];
  pkt->message_id = rx_buffer[3];
  pkt->fragment_id = rx_buffer[4];
  pkt->total_fragments = rx_buffer[5];
  pkt->lenght = rx_buffer[6];
  memcpy(pkt->data, &rx_buffer[7], pkt->lenght);

  uint8_t received_checksum = rx_buffer[pkt->lenght + 8];
  if (!validate_checksum(pkt, received_checksum)) {
    free(pkt);
    return;
  }

  message_entry_t entry {
    pkt->message_id,
    0xFFFFFFFF // All fragments must arrive within 1 second.
  };

  received[entry].push_back(pkt);
}

void PacketHandler::send_pkt(packet_t* pkt) {
  // printf("Sending packet type %d\n\r", pkt->type);

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

  uint8_t res = LORA_BUSY;
  while (res == LORA_BUSY) {
    res = lora_send_packet(&lora, buffer, pkt->lenght + 9);
  }

  if (res != LORA_OK) {
    // TODO
    printf("Oh no pizza didn't send\n\r");
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
 