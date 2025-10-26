# ESPManager Library

A comprehensive management library for ESP8266 to handle MQTT connections and remote commands like Over-the-Air (OTA) updates with ease.

`ESPManager` is designed to abstract away the boilerplate code required for maintaining MQTT connections, handling remote commands, and performing updates. This allows you to focus on your application's core logic.

## Features

- **Robust MQTT Handling**:
    - Automatically connects and reconnects to the MQTT broker.
    - Implements MQTT Last Will and Testament (LWT) to publish an "offline" status on ungraceful disconnects.
    - Publishes a retained "online" message upon successful connection.
- **Remote Commands**:
    - **Remote Reset**: Triggers a configurable callback to erase device configuration and then restarts the device.
    - **OTA Updates**: Triggers a firmware update from a specified update server when a JSON command is received.
- **Callback-driven**: Uses callbacks for handling incoming MQTT messages, connection events, and configuration erasure, allowing for clean integration with your main sketch.
- **Extensible**: Provides direct access to the underlying `PubSubClient` object for custom subscriptions and publications.

## Dependencies

- [PubSubClient](https://github.com/knolleary/pubsubclient) by Nick O'Leary
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) by Benoît Blanchon

---

## Installation

### 1. PlatformIO

This is the recommended environment for using `ESPManager`.

1.  Place the `ESPManager` folder inside the `lib/` directory of your PlatformIO project.
2.  Add the required dependencies to your `platformio.ini` file:

    ```ini
    lib_deps =
        knolleary/PubSubClient
        bblanchon/ArduinoJson
    ```

### 2. Arduino IDE

1.  **Install Core Libraries**:
    -   Go to `Sketch` > `Include Library` > `Manage Libraries...`.
    -   Search for and install `PubSubClient` by Nick O'Leary.
    -   Search for and install `ArduinoJson` by Benoît Blanchon.

2.  **Install `ESPManager`**:
    -   Place the `ESPManager` folder into your Arduino `libraries` folder (commonly found in `Documents/Arduino/libraries/`).
    -   Restart the Arduino IDE.

---

## How to Use

Here is a step-by-step guide to integrating `ESPManager` into your project. This example uses [WifiHandlerESP](https://github.com/vishalbala-nps/WifiHandlerESP) for WiFi management, but you can use any library you prefer.

### 1. Include Headers

```cpp
#include <Arduino.h>
#include <WifiHandlerESP.h> // Or your preferred WiFi library
#include <ESPManager.h>
#include <WiFiClient.h>
```

### 2. Instantiate Objects

Create global instances of `WiFiClient` and `ESPManager`.

```cpp
WiFiClient espClient;
WifiHandlerESP wifiHandler; // From your WiFi library
ESPManager espManager(espClient);
```

### 3. Define Callbacks

Create callback functions to handle MQTT events and the erase command.

```cpp
// Called when a message is received on a subscribed topic
void messageReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// Called once the MQTT client successfully connects
void onMqttConnect() {
  Serial.println("MQTT Connected. Subscribing to custom topics...");
  espManager.getClient().subscribe("my-custom-topic");
}

// Called when a remote erase command is received
void eraseDeviceConfig() {
  Serial.println("Erasing WiFi configuration...");
  wifiHandler.erase(); // Call the erase function from your WiFi library
}
```

### 4. Configure `setup()`

In your `setup()` function, initialize your WiFi connection and then initialize `ESPManager`.

```cpp
void setup() {
  Serial.begin(115200);

  // ... (Your WiFi connection logic) ...
  // For example, using WifiHandlerESP:
  wifiHandler.begin();

  // Set ESPManager callbacks
  espManager.setMessageRecieveCallback(messageReceived);
  espManager.onConnect(onMqttConnect);
  espManager.onErase(eraseDeviceConfig);

  // If WiFi connects, retrieve config and start ESPManager
  if (WiFi.status() == WL_CONNECTED) {
    // ... (Retrieve your config values) ...
    String deviceId = "my-device";
    String mqttServer = "192.168.1.100";
    String updateServer = "192.168.1.100:8080";

    if (deviceId.length() > 0 && mqttServer.length() > 0) {
      espManager.begin(
        deviceId.c_str(),
        "1.0.0", // Your app version
        mqttServer.c_str(),
        1883,    // MQTT Port
        "user",  // MQTT User
        "pass",  // MQTT Password
        updateServer.c_str()
      );
    }
  }
}
```

### 5. Run in `loop()`

Finally, call your WiFi library's handler and `espManager.loop()` in your main `loop()`.

```cpp
void loop() {
  // wifiHandler.process(); // Or your WiFi library's loop function

  if (WiFi.status() == WL_CONNECTED) {
    espManager.loop();
  }
}
```

---

## API Reference

### Public Methods

`ESPManager(WiFiClient& wifiClient)`
The constructor requires a `WiFiClient` instance.

`void begin(const char* deviceId, const char* appVersion, const char* mqttServer, int mqttPort, const char* mqttUser, const char* mqttPassword, const char* updateServer)`
Initializes the manager with all necessary configuration.

`void loop()`
Keeps the MQTT client connected and processes incoming messages. Must be called repeatedly in your main loop.

`PubSubClient& getClient()`
Returns a reference to the internal `PubSubClient` object, allowing you to call methods like `publish()` and `subscribe()` directly.

`void setMessageRecieveCallback(MQTTMessageCallback callback)`
Registers a callback function to handle messages from topics that are not internally managed.

`void onConnect(MQTTConnectCallback callback)`
Registers a callback function that is executed every time the client successfully connects or reconnects to the MQTT broker. Ideal for subscribing to topics.

`void onErase(EraseConfigCallback callback)`
Registers a callback function that is executed when a remote reset command is received. This is where you should call the configuration erase function of your WiFi management library.

`void onUpdateBegin(UpdateBeginCallback callback)`
Registers a callback function that is executed just before the firmware update process begins. This is useful for saving state or disabling peripherals.

---

## Built-in Remote Commands

`ESPManager` automatically subscribes to `device/status/<deviceId>` and listens for the following commands:

### Device Reset

-   **Topic**: `device/status/<deviceId>`
-   **Payload**: A JSON message with an `action` of `delete`.
    ```json
    {"action": "delete"}
    ```
-   **Action**: The device will publish a blank retained message to its status topic, gracefully disconnect from the MQTT broker, execute the registered `onErase` callback, and then restart.

### OTA Firmware Update

-   **Topic**: `device/status/<deviceId>`
-   **Payload**: A JSON message with an `action` and `version`.
    ```json
    {"action": "update", "version": "1.0.1"}
    ```
-   **Action**: The device will construct a URL and attempt to download the new firmware binary.
-   **URL Format**: `http://<updateServer>/api/updates/<version>/download`

---

## Automatic Status Messages

To provide real-time device state, `ESPManager` automatically publishes status messages to the `device/status/<deviceId>` topic. These messages are sent with the `retain` flag set to `true`, ensuring that the last known status is always available to MQTT clients.

### Updating Status

-   **Published**: Just before the firmware update process begins.
-   **Payload**: A JSON message indicating the device is updating.
    ```json
    {"deviceId": "<deviceId>", "status": "updating", "version": "<appVersion>"}
    ```

### Online Status

-   **Published**: On successful connection to the MQTT broker.
-   **Payload**: A JSON message indicating the device is online.
    ```json
    {"deviceId": "<deviceId>", "status": "online", "version": "<appVersion>"}
    ```

### Offline Status (Last Will and Testament)

-   **Published**: Automatically by the MQTT broker if the device disconnects ungracefully (e.g., power loss).
-   **Payload**: A JSON message indicating the device is offline. This is set as the device's Last Will and Testament (LWT).
    ```json
    {"deviceId": "<deviceId>", "status": "offline", "version": "<appVersion>"}
    ```
