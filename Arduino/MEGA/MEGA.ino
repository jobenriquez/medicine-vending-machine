#include <Arduino.h>

// Define the Motor struct  
struct Motor {
    int IN1;
    int IN2;
    int limitSwitchPin;
    bool isRunning;
    bool isStoppedBySwitch;
};

// Vending machine motor configuration
const int NUM_MOTORS = 1;
Motor motors[NUM_MOTORS] = {
    {7, 6, 5, false, false}, // Motor "A1"
};


// Command buffer
const int COMMAND_BUFFER_SIZE = 32;
char commandBuffer[COMMAND_BUFFER_SIZE];
int commandIndex = 0;
int limitSwitchMotorIndex = -1;

// TB6600 configuration
const int dirPin = 13;  
const int stepPin = 12; 
const int initialPosition = 0;
const int stepsPerSecond = 100;

bool motorCompleted = false;
char lastRackId = '\0'; 
int quantity = 0;
int quantityDispensed = 0;

void setup() {
    Serial1.begin(9600);
    Serial.begin(9600);

    // Initialize all vending machine motors
    for (int i = 0; i < NUM_MOTORS; i++) {
        pinMode(motors[i].IN1, OUTPUT);
        pinMode(motors[i].IN2, OUTPUT);
        pinMode(motors[i].limitSwitchPin, INPUT_PULLUP); 
    }

    // Initialize TB6600 motors
    pinMode(dirPin, OUTPUT);
    pinMode(stepPin, OUTPUT);
}

void loop() {
    // Read serial command
    if (Serial1.available() > 0) {
        char incomingByte = Serial1.read();
        if (incomingByte != '\n' && commandIndex < COMMAND_BUFFER_SIZE - 1) {
            commandBuffer[commandIndex++] = incomingByte;
        } else {
            commandBuffer[commandIndex] = '\0';
            commandIndex = 0;

            Serial.print("Received: ");
            Serial.println(commandBuffer);

            if (strncmp(commandBuffer, "TURN_MOTOR_", 11) == 0 && strstr(commandBuffer, "_ON") != nullptr) {
                quantityDispensed = 0;
                char motorId[3];
                strncpy(motorId, commandBuffer + 11, 2);
                motorId[2] = '\0';
                // Extract rack ID (first character of motorId)
                lastRackId = motorId[0];

                quantity = commandBuffer[14] - '0';
                Serial.println(quantity);

                // Move stepper to position
                moveForTime(getPosition(lastRackId));

                // Start DC motor
                int motorIndex = getMotorIndexById(motorId);
                if (motorIndex != -1) {
                    startMotor(motorIndex);
                    motors[motorIndex].isRunning = true;
                    motors[motorIndex].isStoppedBySwitch = false;
                    motorCompleted = false; 
                    delay(1000);
                } else {
                    Serial1.println("❌ Unknown motor ID");
                }
            } else {
                Serial1.println("Unknown command");
            }

            
        }
    }
    
    // Check limit switches
    for (int i = 0; i < NUM_MOTORS; i++) {
        checkLimitSwitch(i);
    }

    if (motorCompleted && quantityDispensed == quantity) {
      delay(2000);
      moveForTime(-getPosition(lastRackId));
      motorCompleted = false;
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

  if (reading == HIGH && m.isRunning && !m.isStoppedBySwitch) {
    Serial.println("🔴 Limit switch activated! Stopping motor.");
    stopMotor(motorIndex);
    m.isRunning = false;
    m.isStoppedBySwitch = true;
    motorCompleted = true;
    quantityDispensed++;

    if (quantityDispensed != quantity) {
        startMotor(motorIndex);
        motors[motorIndex].isRunning = true;
        motors[motorIndex].isStoppedBySwitch = false;
        motorCompleted = false; 
        delay(1000);
    }
  }
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

    Serial1.print("✅ Motor ");
    Serial1.print(motorIndex == 0 ? "A1" : "B1");
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
    Serial1.print(motorIndex == 0 ? "A1" : "B1");
    Serial1.println(" Stopped.");
}

// ----- TB6600 functions -----
// Move motor for a specific time (milliseconds)
void moveForTime(long timeInMilliseconds) {
  bool moveForward = timeInMilliseconds > 0;
  unsigned long duration = abs(timeInMilliseconds);  // Get absolute duration
  
  digitalWrite(dirPin, moveForward ? HIGH : LOW);
  //digitalWrite(dirPin, LOW);
  
  unsigned long startTime = millis();
  while (millis() - startTime < duration) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(25);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(25);
  }
}

int getPosition(char rackId) {
  switch (rackId) {  
    case 'A': return 10490;  
    case 'B': return 200;
    case 'C': return 300;
    case 'D': return 400;
    default: return 0;  
  }
}
