#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

//const char* ssid = "PLDTHOMEFIBRbdb28";
//const char* password = "PLDTWIFI6utmu";
const char* ssid = "TestWifi";
const char* password = "Admin1234567890!!";
//const char* host = "192.168.1.7"; // Flask server IP
const char* host = "192.168.1.23"; // Flask server IP
const uint16_t port = 5000;

WebSocketsClient webSocket;

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
    switch(type) {
        case WStype_CONNECTED:
            Serial.println("✅ Connected to WebSocket!");
            webSocket.sendTXT("40"); // Join default namespace
            break;  
        case WStype_TEXT: {
            String payloadStr = (char*)payload;
            Serial.printf("Received: %s\n", payloadStr);

            // Handle ping/pong
            if (payloadStr == "2") {
                webSocket.sendTXT("3");
                Serial.println("🏓 Sent pong");
                return;
            }

            // Handle namespace confirmation
            if (payloadStr == "40") {
                Serial.println("🔌 Joined default namespace");
                return;
            }

            // Handle event "42["toggle_motor","ON"]"
            if (payloadStr.startsWith("42[\"toggle_motor\",")) {
                String jsonStr = payloadStr.substring(2); // Remove "42"
                StaticJsonDocument<256> doc;
                DeserializationError error = deserializeJson(doc, jsonStr);

                if (error) {
                    Serial.print("deserializeJson() failed: ");
                    Serial.println(error.c_str());
                    return;
                }

                JsonArray arr = doc.as<JsonArray>();
                String motorPosition = arr[1];  // Extract row and column

                if (motorPosition.length() > 0) {
                    String motorCommand = "TURN_MOTOR_" + motorPosition + "_ON";  
                    Serial2.println(motorCommand);  // Send to Mega
                    Serial.println("🚀 Forwarded command to Mega: " + motorCommand);
                }
            }
            break;
        }
        case WStype_DISCONNECTED:
            Serial.println("❌ Disconnected!");
            break;
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200); // Debugging
    Serial2.begin(9600, SERIAL_8N1, 18, 19); // UART2 on GPIO 18 (RX2), 19 (TX2)

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("✅ Connected to WiFi");

    webSocket.begin(host, port, "/socket.io/?EIO=4&transport=websocket");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
}

void loop() {
    webSocket.loop();

     if (Serial2.available()) {
        String message = Serial2.readString();  // Read the incoming message
        Serial.println("Received from Arduino: " + message);

        // Check if the message is "Process_Complete"
        if (message == "Process_Complete") {
            // Send the completion message to Flask server via WebSocket
            String completionMessage = "Process completed successfully!";
            webSocket.sendTXT(completionMessage);
            Serial.println("✅ Process complete, message sent to Flask app.");
        }
    }
}