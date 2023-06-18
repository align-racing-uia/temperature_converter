/*
 * The temperature converter emulates an Orion BMS thermistor expansion module.
 * It measures the voltage across temperature sensors in the cell modules, calculates the temperatures
 * and reports these temperatures via CANBUS as if it were a thermistor expansion module.
 * 
 * The voltages measured are filtered through a software lowpass filter.
 * The temperatures are calcutlated with a regression model based on the
 * temperature/voltage chart provided by the manufacturer of the cell modules.
 * 
 * Author: Stian Mykland (stian.mykland@alignracing.no)
 * 
 * Additional documentation:
 * Thermistor expansion module CAN communication: https://www.orionbms.com/downloads/misc/thermistor_module_canbus.pdf
 * Temp sensors: https://drive.google.com/file/d/1PSEUp2PVK2kEyxq5ntKf7EPpJIx6y6IH/view?usp=sharing
 */

#include <SPI.h>
#include <mcp2515.h>

// --- Constants ---
// configurable constants; change as necessary
const byte segNum = 7;                  // physical segment number (1-indexed)
const byte sensors[] = {0,1,2,4,5,6,7}; // temperature sensors that are reported to the BMS (0-indexed). Use this to selectively ignore sensors
const float ADC_ref = 3.30;             // board voltage
const float alpha = 0.98;               // filter weight
const int measurementInterval = 40;     // microseconds

// fixed constants; don't change
const byte boardNum = segNum - 1;       // logical segment number (0-indexed)
const byte nSensors = sizeof(sensors);  // number of sensors
const int minVoltage = 1;               // minimum plausible voltage from temp sensors
const int maxVoltage = 2.5;             // maximun plausible voltage from temp sensors
const int messageInterval = 100;        // milliseconds
const int ADC_res = 12;                 // ADC bits
const uint32_t CAN_ID1 = 0x1839F380 + boardNum | CAN_EFF_FLAG;
const uint32_t CAN_ID2 = 0x700 + segNum;

// defining pin names
// analog inputs
#define T0p PC0
#define T0n PC1
#define T1p PC2
#define T1n PC3
#define T2p PA0
#define T2n PA1
#define T3p PB0
#define T3n PB1
#define T4p PA2
#define T4n PA3
#define T5p PC4
#define T5n PC5
#define T6p PA6
#define T6n PC7
#define T7p PA4
#define T7n PA5
// SPI pins
#define clk PB3
#define miso PB4
#define mosi PB5
#define cs PB6

//2D array of analog input pins in pseudodifferential pairs
const int inputPin[8][2] = {
  {T0p, T0n},
  {T1p, T1n},
  {T2p, T2n},
  {T3p, T3n},
  {T4p, T4n},
  {T5p, T5n},
  {T6p, T6n},
  {T7p, T7n}
 };

// declaring variables
float voltage[8][2];
unsigned long lastMeasurement = -1;
unsigned long lastMessage = 0;

// creating CAN controller object
MCP2515 can(cs);

//CAN frame struct for BMS
struct can_frame msg1;
//CAN frame struct for manual monitoring
struct can_frame msg2;

// function for converting voltage to temperature
float getTemp(float v){
  return(-753.269 * pow(v, 5) + 7175.619 * pow(v, 4) - 27248.194 * pow(v, 3) + 51567.592 * pow(v, 2) - 48733.028 * v + 18489.36);
}

//function that returns the cell number that a sensor corresponds to
byte cellID(int index){
  int cells[8] = {1, 8, 9, 3, 11, 4, 12};
  return(12 * boardNum + cells[index]);
}

void setup() {
  // enabling peripheral clock of ports A and B
  RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;
  
  // enabling alternate functiton on pins A8, B3, B4, B5
  GPIOA->MODER |= GPIO_MODER_MODER8_1;
  GPIOB->MODER |= GPIO_MODER_MODER3_1 | GPIO_MODER_MODER4_1 | GPIO_MODER_MODER5_1;

  // setting alternate function 0 on pin A8 (MCO)
  GPIOA->AFR[1] |= 0x00 << GPIO_AFRH_AFSEL8_Pos;
  //setting alternate function 0 on pins B3, B4 and B5 (SPI1_SCK, SPI1_MISO, SPI1_MOSI)
  GPIOB->AFR[0] |= (0x00 << GPIO_AFRL_AFSEL3_Pos) | (0x00 << GPIO_AFRL_AFSEL4_Pos) | (0x00 << GPIO_AFRL_AFSEL5_Pos);
  
  // setting MCO pin to output HSI clock
  RCC->CFGR |= RCC_CFGR_MCO_HSI;

  // configuring SPI pins
  SPI.setMOSI(mosi);
  SPI.setMISO(miso);
  SPI.setSCLK(clk);

  //initiating communication with CAN controller
  can.reset();
  can.setBitrate(CAN_500KBPS, MCP_8MHZ);
  can.setNormalMode();

  // CAN data that won't change over time
  msg1.can_id = CAN_ID1;
  msg1.can_dlc = 8;
  msg2.can_id = CAN_ID2;
  msg2.can_dlc = 8;

  // sets resolution of ADC
  analogReadResolution(ADC_res);

  for (int i=0; i<7; i++){
    pinMode(inputPin[i][0], INPUT);
    pinMode(inputPin[i][1], INPUT);
  }
}

void loop() {
  // measure and filter voltages every [measurementInterval] µs
  while((micros() - lastMeasurement) < measurementInterval){} 
  lastMeasurement = micros();
  for (int i=0; i<8; i++){
    for (int j=0; j<2; j++){
      float currentVoltage = analogRead(inputPin[i][j]) * ADC_ref / (pow(2, ADC_res) - 1);
      voltage[i][j] = alpha * voltage[i][j] + (1 - alpha) * currentVoltage;
    }
  }

  // send CANBUS message if there's been more than [messageInterval] ms since last message was sent
  if ((millis() - lastMessage) >= messageInterval){
    lastMessage = millis();
    float temp[8];
    int maxIndex = 0;
    int minIndex = 0;
    float avgTemp = 0;
    for (int i=0; i<nSensors; i++){
      int n = sensors[i];
      float diffVoltage = voltage[n][0] - voltage[n][1];
      if ((diffVoltage < maxVoltage) && (diffVoltage > minVoltage)){
        temp[n] = getTemp(diffVoltage);
      } else {
        temp[n] = 0;
      }
      avgTemp += temp[n];
      if (temp[n] >= temp[maxIndex]){
        maxIndex = n;
      }
      if (temp[n] <= temp[minIndex]){
        minIndex = n;
      }
    }
    avgTemp /= nSensors;

    // content of CANBUS message for BMS
    msg1.data[0] = boardNum;              // Thermistor Module Number
    msg1.data[1] = byte(temp[minIndex]);  // Lowest thermistor value  (ºC)
    msg1.data[2] = byte(temp[maxIndex]);  // Highest thermistor value (ºC)
    msg1.data[3] = byte(avgTemp);         // Average thermistor value (ºC)
    msg1.data[4] = nSensors;              // Number of thermistors enabled
    msg1.data[5] = cellID(maxIndex);      // Highest thermistor ID on the module
    msg1.data[6] = cellID(minIndex);      // Lowest thermistor ID on the module
    msg1.data[7] = 0x41;                  // ask Orion
    for (int i=0; i<7; i++){
      msg1.data[7] += msg1.data[i];
    }
    can.sendMessage(&msg1);

    // content of a separate, more user friendly CANBUS message. Not functionally necessary, but simplifies debugging
    for (int i=0; i<8; i++){
      msg2.data[i] = byte(temp[i]);
//      msg2.data[i] = byte((voltage[i][0] - voltage[i][1]) * 10);
    }
    can.sendMessage(&msg2);
  }
}
