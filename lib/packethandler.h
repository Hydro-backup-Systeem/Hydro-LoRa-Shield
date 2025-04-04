#pragma once 

#include <ctime>
#include <vector>
#include <unordered_map>

#include <stdint.h>

#include "packet.h"
#include "lora_sx1276.h"

#define LEASE_TIME_MS              2500

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

class PacketHandler {

  public:
    PacketHandler();

  public:
    void clean();
    void receive_mode();
    void send(uint8_t* data, uint32_t size);
    void receive();

  private:
    void send_pkt(packet_t* pkt);
    void checksum(packet_t* pkt, uint8_t len);
    bool validate_checksum(packet_t* pkt, uint16_t chksum);

  private:
    spi_config_t spi_config;
    lora_sx1276 lora;
    std::unordered_map<message_entry_t, std::vector<packet_t*>> messages;
};