#include "packethandler.h"

#include <thread>
#include <math.h>
#include <cstdlib>
#include <stdio.h>
#include <cstring>
#include <algorithm>
#include <iostream>

uint32_t PacketHandler::iv[4];

void PacketHandler::init() {
  srand((unsigned)time(NULL));
  AES_init_ctx_iv(&aes_ctx, reinterpret_cast<uint8_t*>(aes_key), reinterpret_cast<uint8_t*>(iv));
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
      std::cout << "All fragments received for message ID: " << (int)it->first.id << std::endl;
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

            // Decrypt the message
            AES_CTR_xcrypt_buffer(&aes_ctx, reassembled, total_length);
            // Get the padding length from the last byte
            int padding_length = reassembled[total_length - 1];
            
            // Validate padding length
            if (padding_length > 0 && padding_length < 16) {
              // PKCS#7 padding: the last byte indicates the number of padding bytes
              for (uint32_t i = total_length - padding_length; i < total_length; i++) {
                if (reassembled[i] != padding_length) {
                  padding_length = -1; // Invalid padding
                  break;
                }
              }
            } else if (total_length % 16 == 0) {
              padding_length = 0; // No padding
            } else {
              padding_length = -2; // Invalid padding
            }

            // Verify padding is valid (should be between 1 and 15)
            if (padding_length < 16 && padding_length >= 0) {
              // Calculate actual message length without padding
              uint32_t actual_length = total_length - padding_length;
              
              // Call callback with unpadded data
              msg_callback(reassembled, actual_length);
            }
            else {
              // Invalid padding, handle error
              std::cerr << "Invalid padding length: " << padding_length << std::endl;
            }

            free(reassembled);
          }
          break;

        case PacketTypes::ENCRYP: {
          
          uint32_t* iv_vector = reinterpret_cast<uint32_t *>(reassembled);
          swap_endianness(iv_vector, 4);
          
          // Initialize AES context with new IV
          memcpy(this->iv, iv_vector, front->lenght);
          AES_ctx_set_iv(&aes_ctx, reinterpret_cast<uint8_t*>(iv_vector));
          
          std::cout << "Received IV" << std::endl;
          std::cout << "IV: ";
          for (uint32_t i = 0; i < 4; i++) {
            std::cout << std::hex << iv_vector[i] << " ";
          }
          std::cout << std::endl;

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
  // Generate random values for the AES IV
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

  // PKCS#7 padding:
  uint8_t padding_value = (uint8_t)paddedSizeBytes - size;
  memset(paddedData, padding_value, paddedSizeBytes);

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
  uint8_t rx_received = lora->receive(rx_buffer, sizeof(rx_buffer), 500, &error);
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

  std::cout << "Pushing packet to received queue" << std::endl;

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

  bool succes = false;

  while (!succes) {
    succes = lora->send(buffer, pkt->lenght + 9);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
 