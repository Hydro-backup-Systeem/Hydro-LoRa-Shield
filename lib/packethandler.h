#pragma once 

#include <stdint.h>
#include <vector>
#include <unordered_map>

#include "aes.hpp"
#include "packet.h"
#include "lora.h"

#include <ctime>

#define LEASE_TIME_MS              2500

#define SUPER_SECRET_SANTA_AES_KEY_DO_NOT_SHARE { 0xC939CC13, 0x397C1D37, 0xDE6AE0E1, 0xCB7C423C }

struct message_entry_t {
  uint8_t   id;
  time_t  expiration;

  bool operator==(const message_entry_t &other) const {
    return id == other.id;
  }
};

// Specialization of std::hash for message_entry_t
namespace std {
  template<>
  struct hash<message_entry_t> {
    std::size_t operator()(const message_entry_t& k) const {
      return std::hash<uint8_t>{}(k.id) ^ (std::hash<uint32_t>{}(k.expiration) << 1);
    }
  };
}

typedef void(*msg_data_callback_t)(uint8_t* data, size_t len);

class PacketHandler {
  public:
    PacketHandler(LoRa* lora) : lora(lora) {};

  public:
    void set_msg_callback(msg_data_callback_t callback);
  
    void init();
  
    // Regular processing methods.
    void poll();
    void clean();
    void receive();
    void send(uint8_t* data, uint32_t size, PacketTypes type);
  
  private:
    // Low-level send operation for one packet.
    void send_pkt(packet_t* pkt);
    // Compute the packet checksum and return it.
    uint8_t compute_checksum(packet_t* pkt, uint8_t len);
    // Validate a packet's checksum using compute_checksum().
    bool validate_checksum(packet_t* pkt, uint16_t chksum);
    // Perform an encryption handshake with the given message ID.
    void encryption_handshake(uint8_t msg_id);
    // Helper function to convert uint32_t array to bytes
    void u32_to_bytes(uint32_t* input, uint8_t* output);
  
  private:
    // Initial AES vector for encryption (aligned as required).
    static uint32_t iv[4];
    uint32_t aes_key[4] = SUPER_SECRET_SANTA_AES_KEY_DO_NOT_SHARE;
  
    // LoRa chip instance.
    LoRa* lora;

    AES_ctx aes_ctx;
  
    msg_data_callback_t msg_callback = nullptr;
  
    // Outgoing and incoming message buffers.
    std::unordered_map<message_entry_t, std::vector<packet_t*>> messages;
    std::unordered_map<message_entry_t, std::vector<packet_t*>> received;
  };