#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <math.h>

#include "./src/InterfaceConnection.h"
#include "./src/LoraHandling.h"

#include "./lib/lora_sx1276.h"

spi_config_t spi_config;
uint8_t buff[256];

int main() {
    SPI *spi = NULL;

    spi_config.mode=0;
    spi_config.speed=5 * pow(10, 6);
    spi_config.delay=0;
    spi_config.bits_per_word=8;

    spi = new SPI("/dev/spidev0.0", &spi_config);

    spi->begin();

    lora_sx1276 lora;
    uint8_t res = lora_init(&lora, spi, LORA_BASE_FREQUENCY_EU);

    if (res != LORA_OK) {
        printf("AAAAAAAAAAHHHHHHHHHH\n\r");
        return -1;
    }

    res = lora_version(&lora);

    printf("Lora version: %d\n\r", res);

    // Put LoRa modem into continuous receive mode
    lora_mode_receive_continuous(&lora);
    
    while (true) {
        // Receive buffer
        uint8_t buffer[32];
        // Wait for packet up to 10sec
        uint8_t res;
        uint8_t len = lora_receive_packet_blocking(&lora, buffer, sizeof(buffer), 10000, &res);
        if (res != LORA_OK) {
            // Receive failed
            printf("Oh no! failure\n\r");
        }

        buffer[len] = 0;  // null terminate string to print it

        printf("'%s'\n", buffer);
    }

    // -- Socket handling 
    // InterfaceConnection server(8080);

    // if (!server.createSocket()) return 1;

    // server.createConnection();
    // server.clientHandling();

    return 0;
}