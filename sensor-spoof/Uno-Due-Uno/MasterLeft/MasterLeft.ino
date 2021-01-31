// Master Code Arduino Uno Left
// Sends value to Arduino Due (the middle slave)

#include <Wire.h>
#define SLAVE_ADDR 4
// the address of slave in the middle through which data will be transferred
#define ANSWERSIZE num

int num;
void setup()
{
  Wire.begin(); // join i2c bus (address optional for master)
  Serial.begin(9600);
}

void loop()
{
  delay(100);
  //opens communication with middle slave
  Wire.beginTransmission(SLAVE_ADDR); 
  Wire.write(num);
  Wire.endTransmission();
}
