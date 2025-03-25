// #include <pigpio.h>
// #include <iostream>

// int main() {
//     // Initialize pigpio
//     if (gpioInitialise() < 0) {
//         std::cerr << "pigpio initialization failed!" << std::endl;
//         return -1;
//     }

//     // Set GPIO 17 as output
//     gpioSetMode(17, PI_OUTPUT);

//     // Blink the LED
//     for (int i = 0; i < 10; ++i) {
//         gpioWrite(17, 1); // Set GPIO 17 high (turn on LED)
//         gpioDelay(1000000); // Delay for 1 second
//         gpioWrite(17, 0); // Set GPIO 17 low (turn off LED)
//         gpioDelay(1000000); // Delay for 1 second
//     }

//     // Terminate pigpio
//     gpioTerminate();

//     return 0;
// }
#include <wiringPi.h>
#include <stdio.h>

#define LED_PIN 0  // GPIO17, WiringPi pin 0

int main(void) {
    // Initialize WiringPi library
    if (wiringPiSetup() == -1) {
        printf("WiringPi setup failed!\n");
        return 1;
    }

    // Set the LED pin as an output pin
    pinMode(LED_PIN, OUTPUT);

    while (1) {
        // Turn the LED on
        digitalWrite(LED_PIN, HIGH);
        delay(1000);  // Wait for 1 second

        // Turn the LED off
        digitalWrite(LED_PIN, LOW);
        delay(1000);  // Wait for 1 second
    }

    return 0;
}