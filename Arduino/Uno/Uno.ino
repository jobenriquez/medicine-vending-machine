#include <SoftwareSerial.h>

SoftwareSerial espSerial(2, 3); // RX on pin 2, TX on pin 3

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

// Vending machine motor configuration
const int NUM_MOTORS = 2;
Motor motors[NUM_MOTORS] = {
    {9, 8, 6, false, false, HIGH, 0}, // Motor "A1"
    {5, 4, 7, false, false, HIGH, 0} // Motor "A2" WORKING
};

// Debounce delay
const unsigned long debounceDelay = 100;

// Command buffer
const int COMMAND_BUFFER_SIZE = 32;
char commandBuffer[COMMAND_BUFFER_SIZE];
int commandIndex = 0;
int limitSwitchMotorIndex = -1;

// TB6600 configuration
const int dirPin = 9;  
const int stepPin = 8; 
const int initialPosition = 0;

void setup() {
    espSerial.begin(9600);
    Serial.begin(9600);

    // Initialize all vending machine motors
    for (int i = 0; i < NUM_MOTORS; i++) {
        pinMode(motors[i].IN1, OUTPUT);
        pinMode(motors[i].IN2, OUTPUT);
        pinMode(motors[i].limitSwitchPin, INPUT); 
    }

    // Initialize TB6600 motors
    pinMode(dirPin, OUTPUT);
    pinMode(stepPin, OUTPUT);
    moveToPosition(initialPosition); 
}

void loop() {
    // Read serial command
    if (espSerial.available() > 0) {
        char incomingByte = espSerial.read();
        if (incomingByte != '\n' && commandIndex < COMMAND_BUFFER_SIZE - 1) {
            commandBuffer[commandIndex++] = incomingByte;
        } else {
            commandBuffer[commandIndex] = '\0';
            commandIndex = 0;

            Serial.print("Received: ");
            Serial.println(commandBuffer);

            if (strncmp(commandBuffer, "TURN_MOTOR_", 11) == 0 && strstr(commandBuffer, "_ON") != nullptr) {
                char motorId[3];
                strncpy(motorId, commandBuffer + 11, 2);
                motorId[2] = '\0';

                // Extract rack ID (first character of motorId)
                char rackId = motorId[0];
                
                // Move stepper to position
                //moveToPosition(getPosition(rackId));
                //delay(2000);

                // Start DC motor
                int motorIndex = getMotorIndexById(motorId);
                if (motorIndex != -1) {
                    startMotor(motorIndex);
                    motors[motorIndex].isRunning = true;
                    motors[motorIndex].isStoppedBySwitch = false;
                    delay(500);
                } else {
                    espSerial.println("❌ Unknown motor ID");
                }

                //delay(5000);
                
                // Return stepper to initial position
                //moveToPosition(-getPosition(rackId));
            } else {
                espSerial.println("Unknown command");
            }
        }
    }

    // Check limit switches
    for (int i = 0; i < NUM_MOTORS; i++) {
        checkLimitSwitch(i);
    }
}
// ----- Vending machine motor functinos -----
void checkLimitSwitch(int motorIndex) {
  if (motorIndex < 0 || motorIndex >= NUM_MOTORS) {
      Serial.println("❌ Invalid motor index");
      return;
  }
  Motor& m = motors[motorIndex];

  int reading = digitalRead(m.limitSwitchPin);
      // 🛑 Debug: Print the raw switch state
        Serial.print("Motor ");
        Serial.print(motorIndex);
        Serial.print(" | Limit Switch State: ");
        Serial.println(reading);

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
    if (strcmp(motorId, "B1") == 0) return 1;
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

    espSerial.print("✅ Motor ");
    espSerial.print(motorIndex == 0 ? "A1" : "B1");
    espSerial.println(" Running...");
}

void stopMotor(int motorIndex) {
    if (motorIndex < 0 || motorIndex >= NUM_MOTORS) {
        Serial.println("❌ Invalid motor index");
        return;
    }

    Motor& m = motors[motorIndex];
    digitalWrite(m.IN1, LOW);
    digitalWrite(m.IN2, LOW);

    espSerial.print("🛑 Motor ");
    espSerial.print(motorIndex == 0 ? "A1" : "B1");
    espSerial.println(" Stopped.");
}

// ----- TB6600 functinos -----
void moveToPosition(int steps) {
  bool moveForward = steps > 0;
  steps = abs(steps); 
  
  // Set direction
  digitalWrite(dirPin, moveForward ? HIGH : LOW); 
  
  for (int i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(500); 
    digitalWrite(stepPin, LOW);
    delayMicroseconds(500); 
  }
}

int getPosition(char rackId) {
  switch (rackId) {  
    case 'A': return 1000;
    case 'B': return 2000;
    case 'C': return 3000;
    case 'D': return 4000;
    default: return 0;  
  }
}

