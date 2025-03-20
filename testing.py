import time
from raspi_lora import LoRa, ModemConfig

def on_tx_done(lora):
    print("Message sent successfully!")

# Initialize LoRa
lora = LoRa(
    verbose=True,
    do_calibration=True,
    modem_config=ModemConfig.Bw125Cr45Sf128,  # Standard LoRa settings
    frequency=868,  # Adjust for your region
    tx_power=14,    # Transmission power
)

lora.on_tx_done = on_tx_done

try:
    while True:
        message = "Hello, LoRa!"
        print(f"Sending: {message}")
        lora.send(bytes(message, "utf-8"))
        time.sleep(5)
except KeyboardInterrupt:
    print("\nExiting...")
    lora.close()