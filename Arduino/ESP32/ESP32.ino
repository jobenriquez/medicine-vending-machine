#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

const char* ssid = "TestWifi";
const char* password = "Admin1234567890!!";
//const char* host = "192.168.1.7"; // Flask server IP
//const char* host = "192.168.1.23"; // Flask server IP THIS PC
const char* host = "192.168.1.10"; // Flask server IP KEV LAPTOP
//const char* host = "192.168.1.30"; // Flask server IP
const uint16_t port = 5000;
const char* notification_endpoint = "/dispense_status";

bool dispenseCompletionStatus = false;
const String dispenseSuccessMessage = "DISPENSE_SUCCESSFUL";
const String dispenseUnsuccessfulMessage = "DISPENSE_UNSUCCESSFUL";
String receivedArduinoData = "";
bool dispenseStatusReceived = false;
String dispenseStatus = "";

String dispenseRow = "";       // e.g., "A"
String dispenseColumn = "";    // e.g., "1"
String dispenseQuantity = "1"; // Default to 1 if not otherwise specified

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
              String motorPosition = arr[1];  // Expected format: "A1" or "A1_2" where "2" is the quantity
              if (motorPosition.length() > 0) {
                  // Parse motorPosition. For example, if the string is "A1" or "A1_2"
                  // Split by '_' if available.
                  int underscoreIndex = motorPosition.indexOf('_');
                  if (underscoreIndex != -1) {
                      // Format is "A1_2": first two characters are row & column, rest is quantity
                      dispenseRow = motorPosition.substring(0, 1);              // "A"
                      dispenseColumn = motorPosition.substring(1, 2);           // "1"
                      dispenseQuantity = motorPosition.substring(underscoreIndex + 1); // "2"
                      motorPosition = motorPosition.substring(0, underscoreIndex); // "A1"
                  } else {
                      // If no quantity provided, use defaults
                      dispenseRow = motorPosition.substring(0, 1);
                      dispenseColumn = motorPosition.substring(1);
                      dispenseQuantity = "1";
                  }
                  
                // Build command string that the Mega expects
                String motorCommand = "TURN_MOTOR_" + motorPosition + "_" + dispenseQuantity + "_ON";  
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

    while (Serial2.available()) {
      char inChar = (char)Serial2.read();
      receivedArduinoData += inChar;
      if (receivedArduinoData.endsWith("\n")) { 
        Serial.print("Received from Arduino: ");
        Serial.println(receivedArduinoData);
      if (receivedArduinoData.startsWith(dispenseSuccessMessage)) {
        dispenseStatus = "success";
        dispenseStatusReceived = true;
      } else if (receivedArduinoData.startsWith(dispenseUnsuccessfulMessage)) {
        dispenseStatus = "failure";
        dispenseStatusReceived = true;
      }
      receivedArduinoData = ""; 
    }
  }

    // If a dispense status is received, notify the Flask server
  if (dispenseStatusReceived) {
    Serial.print("Sending dispense status to Flask: ");
    Serial.println(dispenseStatus);
    sendDispenseStatusToFlask(dispenseStatus);
    dispenseStatusReceived = false; // Reset the flag
    dispenseStatus = "";         // Reset the status
    delay(5000); 
  }

  delay(100); // Small delay
}

void sendDispenseStatusToFlask(String status) {
  HTTPClient http;
  String serverPath = String("http://") + host + ":" + port + notification_endpoint;
  
  if (!http.begin(serverPath.c_str())) {
    Serial.println("Error: Unable to begin HTTP request");
    http.end();
    return;
  }
  
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  // Include additional information for creating a transaction record
  String httpRequestData = "status=" + status +
                           "&row=" + dispenseRow +
                           "&column=" + dispenseColumn +
                           "&quantity=" + dispenseQuantity;
  
  int httpResponseCode = http.POST(httpRequestData);
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.print("HTTP Response body: ");
    Serial.println(response);
  } else {
    Serial.print("HTTP Error Code: ");
    Serial.println(httpResponseCode);
    Serial.println("Error sending HTTP request (no detailed string available)");
  }
  http.end();
}
