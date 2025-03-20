#include <Arduino.h>

// Define the Motor struct
struct Motor {
    int IN1;
    int IN2;
    int limitSwitchPin;
    bool isRunning;
    bool isStoppedBySwitch;
    int lastLimitSwitchState;
    unsigned long lastDebounceTime;
};

// Define motor configurations
const int NUM_MOTORS = 2;
Motor motors[NUM_MOTORS] = {
    {6, 7, 3, false, false, HIGH, 0}, // Motor "A1"
    {9, 10, 2, false, false, HIGH, 0} // Motor "A2"
};

// Debounce delay
const unsigned long debounceDelay = 100;

// Command buffer
const int COMMAND_BUFFER_SIZE = 32;
char commandBuffer[COMMAND_BUFFER_SIZE];
int commandIndex = 0;
int limitSwitchMotorIndex = -1;

void setup() {
    Serial1.begin(9600);
    Serial.begin(9600);

    // Initialize all motor pins
    for (int i = 0; i < NUM_MOTORS; i++) {
        pinMode(motors[i].IN1, OUTPUT);
        pinMode(motors[i].IN2, OUTPUT);
        pinMode(motors[i].limitSwitchPin, INPUT); 
    }
}

void loop() {
    // Read serial command
    if (Serial1.available() > 0) {
        char incomingByte = Serial1.read();
        if (incomingByte != '\n' && commandIndex < COMMAND_BUFFER_SIZE - 1) {
            commandBuffer[commandIndex++] = incomingByte;
        } else {
            commandBuffer[commandIndex] = '\0'; // Null-terminate the command
            commandIndex = 0; // Reset buffer index

            Serial.print("Received: ");
            Serial.println(commandBuffer);

            if (strncmp(commandBuffer, "TURN_MOTOR_", 11) == 0 && strstr(commandBuffer, "_ON") != nullptr) {
                char motorId[3];
                strncpy(motorId, commandBuffer + 11, 2);
                motorId[2] = '\0'; // Null-terminate the motor ID

                int motorIndex = getMotorIndexById(motorId);
                limitSwitchMotorIndex = motorIndex;
                if (motorIndex != -1) {
                    startMotor(motorIndex);
                    motors[motorIndex].isRunning = true;
                    motors[motorIndex].isStoppedBySwitch = false;
                } else {
                    Serial1.println("❌ Unknown motor ID");
                }
            } else {
                Serial1.println("Unknown command");
            }
        }
    }

    // Check limit switch for each motor
    checkLimitSwitch(limitSwitchMotorIndex);
}

void checkLimitSwitch(int motorIndex) {
  if (motorIndex < 0 || motorIndex >= NUM_MOTORS) {
      Serial.println("❌ Invalid motor index");
      return;
  }
  Motor& m = motors[motorIndex];

  int reading = digitalRead(m.limitSwitchPin);

        if (reading != m.lastLimitSwitchState) {
            m.lastDebounceTime = millis(); // Reset debounce timer
        }

        if ((millis() - m.lastDebounceTime) > debounceDelay) {
            if (reading == LOW && m.isRunning && !m.isStoppedBySwitch) {
                Serial.println("🔴 Limit switch activated! Stopping motor.");
                stopMotor(motorIndex);
                m.isRunning = false;
                m.isStoppedBySwitch = true;
            }
        }

        m.lastLimitSwitchState = reading;
}

int getMotorIndexById(const char* motorId) {
    if (strcmp(motorId, "A1") == 0) return 0;
    if (strcmp(motorId, "A2") == 0) return 1;
    return -1; 
}

void startMotor(int motorIndex) {
    if (motorIndex < 0 || motorIndex >= NUM_MOTORS) {
        Serial.println("❌ Invalid motor index");
        return;
    }

    Motor& m = motors[motorIndex];
    digitalWrite(m.IN1, HIGH);
    digitalWrite(m.IN2, LOW);

    Serial1.print("✅ Motor ");
    Serial1.print(motorIndex == 0 ? "A1" : "A2");
    Serial1.println(" Running...");
}

void stopMotor(int motorIndex) {
    if (motorIndex < 0 || motorIndex >= NUM_MOTORS) {
        Serial.println("❌ Invalid motor index");
        return;
    }

    Motor& m = motors[motorIndex];
    digitalWrite(m.IN1, LOW);
    digitalWrite(m.IN2, LOW);

    Serial1.print("🛑 Motor ");
    Serial1.print(motorIndex == 0 ? "A1" : "A2");
    Serial1.println(" Stopped.");
}