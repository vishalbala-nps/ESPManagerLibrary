#include "ESPManager.h"

ESPManager* ESPManager::_instance = nullptr;

ESPManager::ESPManager(WiFiClient& wifiClient)
    : _wifiClient(wifiClient), _mqttClient(wifiClient) {
    _instance = this;
}

void ESPManager::begin(const char* deviceId, const char* appVersion, const char* mqttServer, int mqttPort, const char* mqttUser, const char* mqttPassword, const char* updateServer) {
    _deviceId = deviceId;
    _appVersion = appVersion;
    _mqttServer = mqttServer;
    _mqttPort = mqttPort;
    _mqttUser = mqttUser;
    _mqttPassword = mqttPassword;
    _updateServer = updateServer;

    _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);
    _mqttClient.setCallback(mqttCallback);
}

void ESPManager::setMessageRecieveCallback(MQTTMessageCallback callback) {
    _messageCallback = callback;
}

void ESPManager::onConnect(MQTTConnectCallback callback) {
    _connectCallback = callback;
}

void ESPManager::onErase(EraseConfigCallback callback) {
    _eraseCallback = callback;
}

void ESPManager::loop() {
    if (!_mqttClient.connected()) {
        long now = millis();
        if (now - _lastReconnectAttempt > 5000) {
            _lastReconnectAttempt = now;
            reconnect();
        }
    } else {
        _mqttClient.loop();
    }
}

PubSubClient& ESPManager::getClient() {
    return _mqttClient;
}

void ESPManager::reconnect() {
    Serial.println("*em:Attempting MQTT connection...");
    String lwtTopic = "device/status/" + _deviceId;
    String lwtPayload = "{\"deviceId\":\"" + _deviceId + "\",\"status\":\"offline\",\"version\":\"" + _appVersion + "\"}";
    
    if (_mqttClient.connect(("ESPClient-" + _deviceId).c_str(), _mqttUser.c_str(), _mqttPassword.c_str(), lwtTopic.c_str(), 0, true, lwtPayload.c_str())) {
        Serial.println("*em:MQTT connected");
        
        String statusTopic = "device/status/" + _deviceId;
        String onlinePayload = "{\"deviceId\":\"" + _deviceId + "\",\"status\":\"online\",\"version\":\"" + _appVersion + "\"}";
        _mqttClient.publish(statusTopic.c_str(), onlinePayload.c_str(), true);
    }
}
