#include <Wire.h>

#define PRINT_INFO                    0
#define COUNT_TEMP_BIAS               1
#define DEBUG                         0
#define CALIBRATE                     0


// register addresses FXAS21002C_H_
#define FXAS21002C_H_OUT_X_MSB        0x01    
#define FXAS21002C_H_CTRL_REG0        0x0D  
#define FXAS21002C_H_TEMP             0x12
#define FXAS21002C_H_CTRL_REG1        0x13
#define FXAS21002C_H_INT_SRC_FLAG     0x0B

// gyro parameters
#define ODR_VALUE                     12.5
#define ODR_NUM                       0b110 
#define FSR_VALUE                     250
#define FSR_NUM                       0b11
#define SENSITIVITY                   7.8125e-3
#define TEMP_BIAS_COE_X               0.02
#define TEMP_BIAS_COE_Y               0.02
#define TEMP_BIAS_COE_Z               0.01
#define TEMP_SENS_COE_X               0.0008
#define TEMP_SENS_COE_Y               0.0008
#define TEMP_SENS_COE_Z               0.0001

//gy
long intt;
int i = 0;
double testtime = 20000;
boolean testing = 0;

int8_t tempData;
int8_t tempData0;
float gyroX, gyroY, gyroZ;
float gBiasX, gBiasY, gBiasZ;
uint8_t address = 0x20;

// test var
double xLog[50], yLog[50], zLog[50];
double xLast, yLast, zLast;
double xBiasCum, yBiasCum, zBiasCum;

void setup() {
  xBiasCum = 0;
  yBiasCum = 0;
  zBiasCum = 0;
  Serial.begin(115200);
  Wire.begin();

  setConfigures();
#if PRINT_INFO
  Serial.println("Gyroscope FXAS2100c");
  Serial.print("Output data rate: ");
  Serial.print(ODR_VALUE);
  Serial.println(" Hz");
  Serial.print("Full scale range: ");
  Serial.print(FSR_VALUE);
  Serial.println(" Degree/s");
#endif
  active();
  delay(500);
  readTempData(&tempData);
  tempData0 = tempData;
  ready();
#if PRINT_INFO
  Serial.print("Temperature: ");
  Serial.print(tempData);
  Serial.println(" Degree");
#endif

#if CALIBRATE
  calibrate(&gBiasX, &gBiasY, &gBiasZ);
  #if PRINT_INFO
  Serial.print("Calibration finished");
  #endif
#endif

  active();

}

void loop() {
  if (i == floor(testtime*ODR_VALUE)){
    testing = 0;
  }
  
  if (!(testing)){
    int j = 1;
    while(j){
      if(Serial.available())  {
        String value = Serial.readStringUntil('\n');
        Serial.println(value);
        if(value=="start") {
            i = 0;
            testtime = testtime;
            j = 0;
            testing = 1;
            delay(2000);
          }
        }
      }
  }
  intt = micros();

  i=i+1;

  // Query the sensor
  readGyroData(&gyroX, &gyroY, &gyroZ, &tempData);

  #if DEBUG
    if (i <= 50){
      xLog[i%50-1] = gyroX;
      yLog[i%50-1] = gyroY;
      zLog[i%50-1] = gyroZ;
      if (i=50){
        for(int j =0; j<50; j++){
        xBiasCum += xLog[j];
        yBiasCum += yLog[j];
        zBiasCum += zLog[j];
        }
        xBiasCum /= 50;
        yBiasCum /= 50;
        zBiasCum /= 50;
      } 
    } else {
      xLast = xLog[i%50-1];
      xLog[i%50-1] = gyroX;
      yLast = yLog[i%50-1];   
      yLog[i%50-1] = gyroY;
      zLast = zLog[i%50-1];
      zLog[i%50-1] = gyroZ;
  
      xBiasCum = (xBiasCum*50+gyroX-xLast)/50;
      yBiasCum = (yBiasCum*50+gyroY-yLast)/50;
      zBiasCum = (zBiasCum*50+gyroZ-zLast)/50;
    }
  #endif
  
  Serial.print(" ");
  Serial.print(gyroX,7);
  Serial.print(" ");
  Serial.print(gyroY,7);
  Serial.print(" ");
  Serial.println(gyroZ,7);

  #if DEBUG
    Serial.print(" ");
    Serial.print(xBiasCum,7);
    Serial.print(" ");
    Serial.print(yBiasCum,7);
    Serial.print(" ");
    Serial.println(zBiasCum,7);
  #endif

  #if PRINT_INFO
  Serial.print(" ");
  Serial.println(tempData);
  #endif
  while(micros()-intt<1.0/ODR_VALUE*1e6){
  }
}

// Reads register 'reg' and return it as a byte.
void readReg(uint8_t reg, uint8_t *value)
{
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(address, (uint8_t)1);
  *value = Wire.read();
}

// Reads 'counts' number of values from registration
// 'reg' and stores the values in 'dest[]'.
void readRegs(uint8_t reg, uint8_t count, uint8_t dest[])
{
  uint8_t i = 0;

  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(address, count);

  while (Wire.available()) {
    dest[i++] = Wire.read();
  }
}

// writes registration 'reg' with value 'value'
void writeReg(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

// writes a signle field which has last digit at bit 'bit' 
// wiht length 'numBit' in registration 'reg' with value 'value'.
// e.g. in CTRL_REG1, ODR setting is at bits 4:2, so parameter
// 'bit' would be 2, 'numBit' would be 3.
void writeField(uint8_t reg, int bit, int numBit, uint8_t value)
{
  uint8_t c;
  readReg(reg, &c);
  for (int i = 0; i < numBit; i++) {
    c &= ~(1 << (bit + i));
  }
  writeReg(reg, c | (value<<bit));
}

// Reads the temperature data
void readTempData(int8_t *tempData)
{
  uint8_t rawTempData;
  readReg(FXAS21002C_H_TEMP, &rawTempData);
  *tempData = (int8_t) rawTempData;
}

// Puts the FXAS21002C into standby mode.
void standby()
{
  writeField(FXAS21002C_H_CTRL_REG1, 0, 2, 0b00);
}

// Sets the FXAS21000 to active mode.
void ready()
{
  writeField(FXAS21002C_H_CTRL_REG1, 0, 2, 0b01);
}

// Puts the FXAS21002C into active mode.
void active()
{
  writeField(FXAS21002C_H_CTRL_REG1, 0, 2, 0b10);
}

// sets up basic configures, full-scale rage, 
// cut-off frequency and output data rate.
void setConfigures()
{
  ready();
  writeField(FXAS21002C_H_CTRL_REG0, 0, 2, FSR_NUM);
  writeField(FXAS21002C_H_CTRL_REG1, 2, 3, ODR_NUM);
}

// Reads the gyroscope data
void readGyroData(float *gyroX, float *gyroY, float *gyroZ, int8_t *tempData)
{
  readTempData(tempData);
  int8_t tempDelta = *tempData - tempData0;
  uint8_t rawData[6];  // x/y/z gyro register data stored here
  readRegs(FXAS21002C_H_OUT_X_MSB, 6, &rawData[0]);  // Read the six raw data registers into data array
  *gyroX = ((int16_t)(((int16_t)rawData[0]) << 8 | ((int16_t) rawData[1])));
  *gyroY = ((int16_t)(((int16_t)rawData[2]) << 8 | ((int16_t) rawData[3])));
  *gyroZ = ((int16_t)(((int16_t)rawData[4]) << 8 | ((int16_t) rawData[5])));
  #if COUNT_TEMP_BIAS
  *gyroX = (*gyroX)*SENSITIVITY*(1 + TEMP_SENS_COE_X*(int16_t) tempDelta) - gBiasX - TEMP_BIAS_COE_X*(int16_t) tempDelta;
  *gyroY = (*gyroY)*SENSITIVITY*(1 + TEMP_SENS_COE_Y*(int16_t) tempDelta) - gBiasY - TEMP_BIAS_COE_Y*(int16_t) tempDelta;
  *gyroZ = (*gyroZ)*SENSITIVITY*(1 + TEMP_SENS_COE_X*(int16_t) tempDelta) - gBiasZ - TEMP_BIAS_COE_Z*(int16_t) tempDelta;
  #else
  *gyroX = (*gyroX)*SENSITIVITY - gBiasX;
  *gyroY = (*gyroY)*SENSITIVITY - gBiasY;
  *gyroZ = (*gyroZ)*SENSITIVITY - gBiasZ;  
  #endif
}

void calibrate(float *gBiasX, float *gBiasY, float *gBiasZ)
{
  uint16_t ii, fcount = 5*ODR_VALUE;
  uint8_t rawData[6];
  int16_t temp[3];
  float gyro_bias[3] = {0, 0, 0};

  active();  // Set to active to start collecting data
  
  for(ii = 0; ii < fcount+60; ii++)
  {
    double intt = micros();
    readRegs(FXAS21002C_H_OUT_X_MSB, 6, &rawData[0]);
    temp[0] = ((int16_t)( ((int16_t) rawData[0]) << 8 | ((int16_t) rawData[1])));
    temp[1] = ((int16_t)( ((int16_t) rawData[2]) << 8 | ((int16_t) rawData[3])));
    temp[2] = ((int16_t)( ((int16_t) rawData[4]) << 8 | ((int16_t) rawData[5])));
  
    if (ii>60) {
      gyro_bias[0] += (int32_t) temp[0];
      gyro_bias[1] += (int32_t) temp[1];
      gyro_bias[2] += (int32_t) temp[2];
    }
    while(micros()-intt < 1.0/(ODR_VALUE)*1e6){
    }
  }

  gyro_bias[0] /= (int32_t) fcount; // get average values
  gyro_bias[1] /= (int32_t) fcount;
  gyro_bias[2] /= (int32_t) fcount;

  *gBiasX = gyro_bias[0]* SENSITIVITY;
  *gBiasY = gyro_bias[1]* SENSITIVITY;
  *gBiasZ = gyro_bias[2]* SENSITIVITY;

  ready();
}

void reset(){
  writeReg(FXAS21002C_H_CTRL_REG1, 0b1000000); // set reset bit to 1 to assert software reset to zero at end of boot process
  delay(100);

  uint8_t flag;
  readReg(FXAS21002C_H_INT_SRC_FLAG, &flag);
  while(!(flag & 0x08))  { // wait for boot end flag to be set
      readReg(FXAS21002C_H_INT_SRC_FLAG, &flag);
  }
}
