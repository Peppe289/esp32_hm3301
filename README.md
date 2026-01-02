# ESP32 HM3301 & GPS

#### Module:

- GPS: [Grove GPS Module](https://www.seeedstudio.com/Grove-GPS-Module.html)
  - UART: TX GPIO21; RX GPIO 20.
- HM3301: [Grove Laser PM2.5 Sensor HM3301](https://wiki.seeedstudio.com/Grove-Laser_PM2.5_Sensor-HM3301/)
  - I2C: SDA GPIO6; SCL GPIO7.

#### GPS Functionality:
To convert RAW GPS data into a readable format, use the `igrr/libnmea` component.

# Work Stuff 

- Synchronization type from: [Synchronization Techniques in Real-Time Operating Systems Implementation and Evaluation on Arduino with FreeRTOS](https://www.researchgate.net/publication/379063740_Synchronization_Techniques_in_Real-Time_Operating_Systems_Implementation_and_Evaluation_on_Arduino_with_FreeRTOS)
- Why C? from: [Performance Evaluation of C/C++, MicroPython, Rust and TinyGo Programming Languages on ESP32 Microcontroller](https://www.mdpi.com/2036322)
- Why LittleFS? from docs: [file system considerations](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/file-system-considerations.html). (SPIFFS is (g)old; I don't need FatFS; I(the docs) like LittleFS).

# Logbook:

This repository is part of my **internship** project where I am working with an **ESP32** and various sensors like the **GPS** and **PM2.5 sensor (HM3301)**. The purpose of this project is to implement a real-time system for data collection and processing from multiple devices. Below are some key learnings and challenges I have encountered so far:

- **GPS**: Using pthread (POSIX) introduces more overhead than directly reading from UART until all data is received. Previously, I used a pthread with a shared variable protected by a mutex to continuously read the latest data from UART. However, I found that this introduced unnecessary overhead compared to simply reading directly from the UART until all the data is received, as the thread and mutex handling added complexity and performance costs.
- **HM3301**: It's necessary to enable `CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2` in sdkconfig. If this option is disabled, the system will use the older drivers, which is not optimal.
- **Wi-Fi Configuration via Bluetooth**: I implemented a feature where, when the device is not connected to any network via Wi-Fi, it automatically opens a Bluetooth connection (GATT BLE) for Wi-Fi configuration. The user can send Wi-Fi authentication data over Bluetooth, after which the Bluetooth connection is closed, and the system attempts to connect to the Wi-Fi network. If the connection fails or is lost, the system shuts down the Wi-Fi and reopens the Bluetooth for a new configuration attempt. This ensures that the device can be easily reconfigured without needing physical access or resetting it.
- **Custom Partition for Larger Code**: To fit the larger codebase, including both Bluetooth and Wi-Fi drivers, I had to create a custom partition. Without this partition, the default partition layout was insufficient, and I wasn't able to fit both the Bluetooth and Wi-Fi drivers into the same firmware for flashing on the **ESP32-C3**. This solution allowed me to properly include all the necessary drivers and functionalities in the firmware. [Custom Partition here](partitions.csv).
- **MQTT TLS**: As mentioned in the commit `40cb8e53eca16bbc95c8f2b4904da04efa7f6c74` server verification is skipped by default. In my case, this is standard.

# MQTT Broker

Enable the broker on a remote server by following the documentation: [man 7 mosquitto-tls](https://mosquitto.org/man/mosquitto-tls-7.html)

Generate a certificate authority certificate and key:

```sh
$ openssl req -new -x509 -days <duration> -extensions v3_ca -keyout ca.key -out ca.crt
```
Generate a server key (with encryption):

```sh
$ openssl genrsa -aes256 -out server.key 2048
```

**Note**:
The server private key is generated encrypted with a PEM passphrase.
For this reason, when Mosquitto is started, it will prompt for the passphrase.

When running Mosquitto as a system service (for example via systemctl), the startup will fail, because the passphrase cannot be entered interactively.
This behavior is intentional in this setup.

Generate a certificate signing request to send to the CA:

```sh
$ openssl req -out server.csr -key server.key -new
```

Send the CSR to the CA, or sign it with your CA key:

```sh
$ openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days <duration>
```

**Note**:
When `openssl` prompts with `Enter PEM pass phrase:`, enter a PEM passphrase.
If everything is done correctly, you should end up with the following files: 
- `server.crt`
- `server.csr`
- `server.key`
- `ca.crt`
- `ca.key`
- `ca.srl`

These files are required to enable TLS encryption on the broker side.
Client certificates are not required in this configuration.


## Create a password for authentication:

```sh
$ mosquitto_passwd -H sha512 -c password.txt <username>
```

## Configure Mosquitto

Edit the configuration files as described in [man 5 mosquitto-conf](https://mosquitto.org/man/mosquitto-conf-5.html).

A basic example `mosquitto.conf`:

```
listener 8883
allow_anonymous false
password_file password.txt
cafile ca.crt
keyfile server.key
certfile server.crt
require_certificate false
```

The following configuration enables a TLS-encrypted MQTT listener.
A non-TLS listener (plain MQTT) is intentionally not configured.

## Broker ready and client authentication

At this point, the MQTT broker is fully configured and ready to accept client connections. On the client side, **certificate validation can be disabled** if required, while **TLS encryption remains enabled**. Clients can now authenticate **using username and password credentials** defined in the `password.txt` file.

On the client side, certificate validation can be disabled if required (for example in testing environments), while **TLS encryption remains enabled**.
Clients authenticate using username and password.

## MQTT Auth

Create `private.txt` file:

```CMake
set(ENV{MQTT_BROKER}   "address")
set(ENV{MQTT_PORT}     "port")
set(ENV{MQTT_USERNAME} "username")
set(ENV{MQTT_PASSWORD} "passwd")
```

<!-- ### Notes

build command (build with Make): `idf.py -G "Unix Makefiles" build` -->