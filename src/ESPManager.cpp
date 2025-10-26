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

void ESPManager::onUpdateBegin(UpdateBeginCallback callback) {
    _updateBeginCallback = callback;
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
        _mqttClient.subscribe(statusTopic.c_str());

        if (_connectCallback) {
            _connectCallback();
        }
    }
}

void ESPManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance == nullptr) {
        return;
    }

    String s_topic(topic);
    payload[length] = '\0';
    String s_payload((char*)payload);

    String statusTopic = "device/status/" + _instance->_deviceId;
    if (s_topic == statusTopic) {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, s_payload);

        if (error) {
            // Not a JSON payload, could be the blank message we published.
            // Ignore it.
            return;
        }

        const char* action = doc["action"];

        if (action == nullptr) {
            return;
        }

        if (strcmp(action, "update") == 0) {
            const char* version = doc["version"];
            if (version) {
                if (_instance->_updateBeginCallback) {
                    _instance->_updateBeginCallback();
                }
                Serial.println("*em:Got update command");
                String updatePayload = "{\"deviceId\":\"" + _instance->_deviceId + "\",\"status\":\"updating\",\"version\":\"" + _instance->_appVersion + "\"}";
                _instance->_mqttClient.publish(statusTopic.c_str(), updatePayload.c_str(), true);
                String url = "http://" + _instance->_updateServer + "/api/updates/" + version + "/download";
                Serial.println("*em:Update URL: " + url);
                ESPhttpUpdate.onProgress([](int cur, int total) {
                    Serial.printf("*em:UPDATE Progress: %d%% (%d/%d)\n", (cur * 100) / total, cur, total);
                });
                delay(2000); // Give some time for the Serial prints to complete
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
        } else if (strcmp(action, "delete") == 0) {
            Serial.println("*em:Got delete command");
            _instance->_mqttClient.publish(statusTopic.c_str(), "", true);
            delay(2000);
            _instance->_mqttClient.disconnect();
            Serial.println("*em:Disconnected from MQTT broker");
            if (_instance->_eraseCallback) {
                _instance->_eraseCallback();
            }
            ESP.restart();
        }
    } else {
        if (_instance->_messageCallback) {
            _instance->_messageCallback(topic, payload, length);
        }
    }
}
