#include <Arduino.h>
#include <AddicoreRFID.h>
#include <SPI.h>

// Define the Motor struct  
struct Motor {
    int IN1;
    int limitSwitchPin;
    bool isRunning;
    bool isStoppedBySwitch;
};

// Vending machine motor configuration
const int NUM_MOTORS = 16;
Motor motors[NUM_MOTORS] = {
    {23, 31, false, false}, // A1
    {24, 32, false, false}, // A2
    {25, 33, false, false}, // A3
    {4, 34, false, false}, // A4
    {5, 35, false, false}, // B1
    {6, 36, false, false}, // B2
    {7, 37, false, false}, // B3
    {8, 38, false, false}, // B4
    {9, 39, false, false}, // C1
    {10, 40, false, false}, // C2
    {11, 41, false, false}, // C3
    {12, 42, false, false}, // C4
    {20, 43, false, false}, // D1
    {14, 44, false, false}, // D2
    {15, 45, false, false}, // D3
    {16, 46, false, false} // D4
};

// LED Lights
#define redLight 27
#define greenLight 28

// Ultrasonic sensor
#define trigPin 30
#define echoPin 29

// Command buffer
const int COMMAND_BUFFER_SIZE = 32;
char commandBuffer[COMMAND_BUFFER_SIZE];
int commandIndex = 0;
int limitSwitchMotorIndex = -1;

// TB6600 configuration
const int dirPin = 50;  
const int stepPin = 51; 
const int initialPosition = 0;
const int emergencyStopPin = 53;

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
        pinMode(motors[i].limitSwitchPin, INPUT_PULLUP); 
    }

    // Initialize TB6600 motors
    pinMode(dirPin, OUTPUT);
    pinMode(stepPin, OUTPUT);
    pinMode(emergencyStopPin, INPUT_PULLUP);

    // Initialize ultrasonic sensor
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);

    // Initialize LED lights
    pinMode(redLight, OUTPUT);
    pinMode(greenLight, OUTPUT);

    // Turn on green light by default
    digitalWrite(greenLight, HIGH);
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

                digitalWrite(greenLight, LOW);
                digitalWrite(redLight, HIGH);

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
      delay(1000);
      if (checkIfDispensed()) {
        Serial1.println("DISPENSE_SUCCESSFUL");
      } else {
        Serial1.println("DISPENSE_UNSUCCESSFUL");
      }

      digitalWrite(redLight, LOW);
      digitalWrite(greenLight, HIGH);
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
    if (strlen(motorId) != 2) return -1;

    char row = motorId[0];
    char col = motorId[1];

    if (row >= 'A' && row <= 'D' && col >= '1' && col <= '4') {
      return (row - 'A') * 4 + (col - '1');
    }

    return -1;
}

void startMotor(int motorIndex) {
    if (motorIndex < 0 || motorIndex >= NUM_MOTORS) {
        Serial.println("❌ Invalid motor index");
        return;
    }

    Motor& m = motors[motorIndex];
    digitalWrite(m.IN1, HIGH);

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

    Serial1.print("🛑 Motor ");
    Serial1.print(motorIndex == 0 ? "A1" : "B1");
    Serial1.println(" Stopped.");
}

// ----- TB6600 functions -----
// Move motor for a specific time (milliseconds)
void moveForTime(long timeInMilliseconds) {
  bool moveForward = timeInMilliseconds > 0;
  unsigned long duration = abs(timeInMilliseconds); 
  
  digitalWrite(dirPin, moveForward ? LOW : HIGH);
  //digitalWrite(dirPin, HIGH);
  
  unsigned long startTime = millis();
  while (millis() - startTime < duration) {
    if (digitalRead(emergencyStopPin) == HIGH) {
      Serial.println("Limit switch activated! Stopping motor.");
      break;
    }
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(20);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(20);
  }
}

long getPosition(char rackId) {
  switch (rackId) {  
    case 'A': return 21000;  
    case 'B': return 15000;
    case 'C': return 8500;
    case 'D': return 2000;
    default: return 0;  
  }
}

// ----- Ultrasonic Sensor functions -----
bool checkIfDispensed() {
  long duration;
  int distance;
  bool itemDetected = false;
   // Trigger pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Measure echo time
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  Serial.println(distance);

  if (distance <= 43) {
    Serial.println("Item detected!");
    itemDetected = true;
    delay(2000); 
  }

  return itemDetected;
  //return true;

}

