#ifndef ESPManager_h
#define ESPManager_h

#include <Arduino.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>

class ESPManager {
public:
    typedef void (*MQTTMessageCallback)(char* topic, byte* payload, unsigned int length);
    typedef void (*MQTTConnectCallback)();
    typedef void (*EraseConfigCallback)();
    typedef void (*UpdateBeginCallback)();

    ESPManager(WiFiClient& wifiClient);

    void begin(const char* deviceId, const char* appVersion, const char* mqttServer, int mqttPort, const char* mqttUser, const char* mqttPassword, const char* updateServer);
    void loop();
    PubSubClient& getClient();
    void setMessageRecieveCallback(MQTTMessageCallback callback);
    void onConnect(MQTTConnectCallback callback);
    void onErase(EraseConfigCallback callback);
    void onUpdateBegin(UpdateBeginCallback callback);

private:
    void reconnect();
    static void mqttCallback(char* topic, byte* payload, unsigned int length);

    WiFiClient& _wifiClient;
    PubSubClient _mqttClient;

    String _deviceId;
    String _appVersion;
    String _mqttServer;
    int _mqttPort;
    String _mqttUser;
    String _mqttPassword;
    String _updateServer;

    unsigned long _lastReconnectAttempt = 0;
    MQTTMessageCallback _messageCallback = nullptr;
    MQTTConnectCallback _connectCallback = nullptr;
    EraseConfigCallback _eraseCallback = nullptr;
    UpdateBeginCallback _updateBeginCallback = nullptr;

    static ESPManager* _instance;
};

#endif
