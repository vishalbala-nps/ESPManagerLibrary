#include "ESPManager.h"

ESPManager* ESPManager::_instance = nullptr;

ESPManager::ESPManager(WiFiClient& wifiClient)
    : _wifiClient(wifiClient), _mqttClient(wifiClient) {
    _instance = this;
}

void ESPManager::begin(const char* deviceId, const char* appVersion, const char* mqttServer, int mqttPort, const char* mqttUser, const char* mqttPassword, const char* updateServer, const char* statusTopic, const char* commandTopic, const char* infoTopic) {
    _deviceId = deviceId;
    _appVersion = appVersion;
    _mqttServer = mqttServer;
    _mqttPort = mqttPort;
    _mqttUser = mqttUser;
    _mqttPassword = mqttPassword;
    _updateServer = updateServer;
    _statusTopic = statusTopic;
    _commandTopic = commandTopic;
    _infoTopic = infoTopic;

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

void ESPManager::onUpdateProgress(UpdateProgressCallback callback) {
    _updateProgressCallback = callback;
}

void ESPManager::onUpdateComplete(UpdateCompleteCallback callback) {
    _updateCompleteCallback = callback;
}

void ESPManager::onUpdateFailed(UpdateFailedCallback callback) {
    _updateFailedCallback = callback;
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
    String lwtPayload = "{\"deviceId\":\"" + _deviceId + "\",\"status\":\"offline\",\"version\":\"" + _appVersion + "\"}";
    String fullStatusTopic = _statusTopic + "/" + _deviceId;
    String fullCommandTopic = _commandTopic + "/" + _deviceId;
    
    if (_mqttClient.connect(("ESPClient-" + _deviceId).c_str(), _mqttUser.c_str(), _mqttPassword.c_str(), fullStatusTopic.c_str(), 0, true, lwtPayload.c_str())) {
        Serial.println("*em:MQTT connected");
        
        String onlinePayload = "{\"deviceId\":\"" + _deviceId + "\",\"status\":\"online\",\"version\":\"" + _appVersion + "\"}";
        _mqttClient.publish(fullStatusTopic.c_str(), onlinePayload.c_str(), true);
        _mqttClient.subscribe(fullCommandTopic.c_str());

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

    String fullCommandTopic = _instance->_commandTopic + "/" + _instance->_deviceId;
    String fullStatusTopic = _instance->_statusTopic + "/" + _instance->_deviceId;
    String fullInfoTopic = _instance->_infoTopic + "/" + _instance->_deviceId;

    if (s_topic == fullCommandTopic) {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, s_payload);

        if (error) {
            return; // Not a valid JSON command, ignore.
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
                _instance->_mqttClient.publish(fullStatusTopic.c_str(), updatePayload.c_str(), true);
                
                _instance->_mqttClient.disconnect();
                Serial.println("*em:Disconnected from MQTT broker for update");
                delay(2000);

                String url = "http://" + _instance->_updateServer + "/api/updates/" + version + "/download";
                Serial.println("*em:Update URL: " + url);
                ESPhttpUpdate.onProgress([](int cur, int total) {
                    Serial.printf("*em:UPDATE Progress: %d%% (%d/%d)\n", (cur * 100) / total, cur, total);
                    if (_instance->_updateProgressCallback) {
                        _instance->_updateProgressCallback(cur, total);
                    }
                });
                
                t_httpUpdate_return ret = ESPhttpUpdate.update(_instance->_wifiClient, url);
                switch (ret) {
                    case HTTP_UPDATE_FAILED:
                        Serial.printf("*em:HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                        if (_instance->_updateFailedCallback) {
                            _instance->_updateFailedCallback(ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                        }
                        break;
                    case HTTP_UPDATE_NO_UPDATES:
                        Serial.println("*em:HTTP_UPDATE_NO_UPDATES");
                        if (_instance->_updateFailedCallback) {
                            _instance->_updateFailedCallback(-1, "No updates available");
                        }
                        break;
                    case HTTP_UPDATE_OK:
                        Serial.println("*em:HTTP_UPDATE_OK");
                        if (_instance->_updateCompleteCallback) {
                            _instance->_updateCompleteCallback();
                        }
                        break;
                }
            }
        } else if (strcmp(action, "delete") == 0) {
            Serial.println("*em:Got delete command");
            _instance->_mqttClient.publish(fullStatusTopic.c_str(), "", true);
            delay(2000);
            _instance->_mqttClient.disconnect();
            Serial.println("*em:Disconnected from MQTT broker");
            if (_instance->_eraseCallback) {
                _instance->_eraseCallback();
            }
            ESP.restart();
        } else if (strcmp(action, "info") == 0) {
            Serial.println("*em:Got info command");
            StaticJsonDocument<512> doc;
            doc["deviceId"] = _instance->_deviceId;
            doc["macAddress"] = WiFi.macAddress();
            doc["status"] = "online";
            doc["firmwareVersion"] = _instance->_appVersion;
            doc["ipAddress"] = WiFi.localIP().toString();
            doc["uptime"] = millis();
            doc["wifiSSID"] = WiFi.SSID();
            doc["wifiStrength"] = WiFi.RSSI();
            doc["freeHeap"] = ESP.getFreeHeap();

            String infoPayload;
            serializeJson(doc, infoPayload);
            
            _instance->_mqttClient.publish(fullInfoTopic.c_str(), infoPayload.c_str(), false);
        }
    } else {
        if (_instance->_messageCallback) {
            _instance->_messageCallback(topic, payload, length);
        }
    }
}