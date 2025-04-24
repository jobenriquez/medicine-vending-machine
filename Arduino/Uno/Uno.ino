#include <AddicoreRFID.h>
#include <SPI.h>

#define lockPin 5      
#define MAX_LEN 16     

AddicoreRFID myRFID; 

const int chipSelectPin = 10;
const int NRSTPD = 5;

// Allowed RFID serial numbers
byte RFIDCARD[4]     = {3, 200, 25, 226}; 
byte RFIDKEYCHAIN[4] = {98, 122, 51, 2};

void setup() {
  Serial.begin(9600);      
  SPI.begin();            

  pinMode(chipSelectPin, OUTPUT);             
  digitalWrite(chipSelectPin, LOW);         
  pinMode(NRSTPD, OUTPUT);                
  digitalWrite(NRSTPD, HIGH);

  myRFID.AddicoreRFID_Init(); 
  pinMode(lockPin, OUTPUT);   
}

void loop() {
  uint8_t str[MAX_LEN];  
  uint8_t status;

  status = myRFID.AddicoreRFID_Request(PICC_REQIDL, str);
  if (status != MI_OK) return; 

  status = myRFID.AddicoreRFID_Anticoll(str);
  if (status == MI_OK) {
    // Check if the UID matches any allowed card
    bool isCard = true;
    bool isKeychain = true;

    for (int i = 0; i < 4; i++) {
      if (str[i] != RFIDCARD[i]) isCard = false;
      if (str[i] != RFIDKEYCHAIN[i]) isKeychain = false;
    }

    if (isCard || isKeychain) {
      Serial.println("Access granted!");
      digitalWrite(lockPin, HIGH);  
      delay(5000);                  
      digitalWrite(lockPin, LOW);  
    } else {
      Serial.println("Access denied.");
    }
  }

  myRFID.AddicoreRFID_Halt(); 
}
