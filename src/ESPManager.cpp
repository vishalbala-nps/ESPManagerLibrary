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

void ESPManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance == nullptr) {
        return;
    }

    String s_topic(topic);
    payload[length] = '\0';
    String s_payload((char*)payload);

    String commandTopic = "device/command/" + _instance->_deviceId;
    if (s_topic == commandTopic) {
        StaticJsonDocument<200> doc;
        deserializeJson(doc, s_payload);
        const char* command = doc["command"];
        if (strcmp(command, "update") == 0) {
            const char* url = doc["url"];
            if (url) {
                Serial.println("*em:Got update command");
                t_httpUpdate_return ret = ESPhttpUpdate.update(_instance->_wifiClient, url);
                switch (ret) {
                    case HTTP_UPDATE_FAILED:
                        Serial.printf("*em:HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                        break;
                    case HTTP_UPDATE_NO_UPDATES:
                        Serial.println("*em:HTTP_UPDATE_NO_UPDATES");
                        break;
                    case HTTP_UPDATE_OK:
                        Serial.println("*em:HTTP_UPDATE_OK");
                        break;
                }
            }
        } else if (strcmp(command, "erase") == 0) {
            if (_instance->_eraseCallback) {
                _instance->_eraseCallback();
            }
        }
    } else {
        if (_instance->_messageCallback) {
            _instance->_messageCallback(topic, payload, length);
        }
    }
}
