// Orion temperature canbus: https://www.orionbms.com/downloads/misc/thermistor_module_canbus.pdf
// Orion temperature third-party: https://www.orionbms.com/manuals/pdf/third_party_thermistors.pdf

#include <SPI.h>
#include <lib/mcp2515.h>
struct can_frame tempCanMessage; // To send for one peace of data. Can be duplicated for other ID's and data
struct can_frame APPSCanMessage; // To send for one peace of data. Can be duplicated for other ID's and data
MCP2515 can0(10);                // Chip select
// Temperature Sensor for Battery Management System
// For "Li-ion building block Li4P25RT"

#define VOLTAGE_OFFSET 0.10
#define LIGHT_PIN 13
#define APPS_CAN_ID 0x879

int fan = 0; // fan speed
int sensorPins[5] = {A0, A1, A2, A3, A4};
float sensor1 = 0;
unsigned long registerTime = 0;
float fabs(float value)
{
  return value < 0 ? -value : value;
}

// https://elvistkf.wordpress.com/2016/04/19/arduino-implementation-of-filters/
// Low pass filtering function
const float alpha = 0.0003;
float data_filtered[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

float lowPassFilter(int index, float data)
{
  // Low Pass Filter
  int n = index * 2 - 1;
  data_filtered[n] = alpha * data + (1 - alpha) * data_filtered[n - 1];
  // Store the last filtered data in data_filtered[n-1]
  data_filtered[n - 1] = data_filtered[n];
  // Print Data
  return data_filtered[n];
}

float getSensorVoltage(int sensor)
{
  return ((analogRead(sensor) * 5.0 / 1023.0) + VOLTAGE_OFFSET);
}
float convertToTemperature(int index, float voltage)
{
  int currentIndex = 0;
  float y = -86.956 * voltage + 188.695;
  float temp = lowPassFilter(index, y);
  if (temp >= 40)
    temp = temp - 20;
  else if (temp >= 30)
    temp = temp - 10;
  return temp;
}
void setup()
{

  tempCanMessage.can_id = 0x1839F380 | CAN_EFF_FLAG; // CANBUS ID
  tempCanMessage.can_dlc = 8;                        // Length of the message
  //---CAN DATA START --- //
  tempCanMessage.data[0] = 0x01; // Thermistor Module Number
  tempCanMessage.data[1] = 0x00; // Lowest thermistor value (8 bit signed, 1*C increments)
  tempCanMessage.data[2] = 0x00; // Highest thermistor value (8 bit signed, 1*C increments)
  tempCanMessage.data[3] = 0x00; // Average thermistor value (8 bit signed, 1*C increments)
  tempCanMessage.data[4] = 0x00; // Number of thermistors enabled
  tempCanMessage.data[5] = 0x00; // Highest thermistor ID on the module
  tempCanMessage.data[6] = 0x00; // Lowest thermistor ID on the module
  tempCanMessage.data[7] = 0x00; // Checksum 8-bytes sum of all bytes + ID + length
  //---CAN DATA END --- //

  while (!Serial)
  {
    Serial.begin(9600);
  }
  SPI.begin();

  can0.reset();
  can0.setBitrate(CAN_500KBPS); // Rate of CANBUS 500kbps for normal usage
  // Configure CAN-bus masks
  can0.setConfigMode();
  can0.setFilterMask(MCP2515::MASK0, false, 0xFFF);
  can0.setFilterMask(MCP2515::MASK1, false, 0xFFF);
  can0.setFilter(MCP2515::RXF0, false, APPS_CAN_ID);
  can0.setFilter(MCP2515::RXF1, false, APPS_CAN_ID);
  can0.setFilter(MCP2515::RXF2, false, APPS_CAN_ID);
  can0.setFilter(MCP2515::RXF3, false, APPS_CAN_ID);
  can0.setFilter(MCP2515::RXF4, false, APPS_CAN_ID);
  can0.setFilter(MCP2515::RXF5, false, APPS_CAN_ID);

  can0.setNormalMode();

  Serial.begin(9600);
  pinMode(LIGHT_PIN, OUTPUT);
  Serial.println((String) "#14,Temperature, Voltage");
  Serial.println((String) "#15,Temperature, Voltage");
  Serial.println((String) "#16,Temperature, Voltage");
  Serial.println((String) "#17,Temperature, Voltage");
  Serial.println((String) "#18,Temperature, Voltage");
  Serial.println((String) "Summary,Max,Max Index,Min,Min Index,Avg, R2D");
}
unsigned long tempSendTime, serialTime;
int R2D = 0;
void loop()
{
  // --- Canbus receive ---
  struct can_frame frame;

  // if (can0.readMessage(&frame) == MCP2515::ERROR_OK)
  // {
  //   // frame contains received message
  // }
  if (can0.readMessage(&APPSCanMessage) == MCP2515::ERROR_OK)
  {
    // frame contains received message
    if (APPSCanMessage.can_id == APPS_CAN_ID)
    {
      R2D = APPSCanMessage.data[0];
    }
  }
  float voltages[5] = {};
  float temperatures[5] = {};
  int i = 0;
  int tempAvg = 0;
  int maxIndex = 0;
  int minIndex = 0;
  for (int sensor : sensorPins)
  {
    voltages[i] = getSensorVoltage(sensor);
    temperatures[i] = convertToTemperature(i + 1, voltages[i]);
    tempAvg += temperatures[i];
    maxIndex = (temperatures[i] > temperatures[maxIndex] ? i : maxIndex);
    minIndex = (temperatures[i] < temperatures[minIndex] ? i : minIndex);
    if (millis() - serialTime >= 500)
      Serial.println((String) "#" + sensor + "," + temperatures[i] + "," + voltages[i]);

    // Serial.println((String) "Sensor: " + sensor + " Temp: " + temperatures[i] + " Volt: " + voltages[i]);

    i++;
  }
  int avgTemp = tempAvg / 5;
  if (millis() - serialTime >= 500)

  {
    Serial.println((String) "Summary," + temperatures[maxIndex] + "," + maxIndex + "," + temperatures[minIndex] + "," + minIndex + "," + avgTemp + "," + R2D);
    serialTime = millis();
  } // Serial.println((String) "Max: " + temperatures[maxIndex] + " Max Index: " + maxIndex + " Min: " + temperatures[minIndex] + " Min Index: " + minIndex + " Avg: " + avgTemp);
  // Serial.println("");

  tempCanMessage.data[0] = 0x00;                   // Thermistor Module Number
  tempCanMessage.data[1] = temperatures[minIndex]; // Lowest thermistor value (8 bit signed, 1*C increments)
  tempCanMessage.data[2] = temperatures[maxIndex]; // Highest thermistor value (8 bit signed, 1*C increments)
  tempCanMessage.data[3] = avgTemp;                // Average thermistor value (8 bit signed, 1*C increments)
  tempCanMessage.data[4] = 0x01;                   // Number of thermistors enabled
  tempCanMessage.data[5] = 0x01;                   // Highest thermistor ID on the module
  tempCanMessage.data[6] = 0x00;                   // Lowest thermistor ID on the module
  tempCanMessage.data[7] = ((tempCanMessage.data[0] +
                             tempCanMessage.data[1] +
                             tempCanMessage.data[2] +
                             tempCanMessage.data[3] +
                             tempCanMessage.data[4] +
                             tempCanMessage.data[5] +
                             tempCanMessage.data[6] +
                             0x41) & // 0x41 is a constant given from the length of bytes and 0x39 given in contact with Orion
                            0xFF);   // Checksum 8-bytes sum of all bytes + ID + length https://www.orionbms.com/manuals/utility_jr/param_cell_broadcast_message.html

  if (avgTemp >= 50)
  {
    digitalWrite(LIGHT_PIN, HIGH);
  }
  else
  {
    digitalWrite(LIGHT_PIN, LOW);
  }
  // --- CAN-BUS ---
  if (millis() - tempSendTime > 100)
  {
    can0.sendMessage(&tempCanMessage);
    tempSendTime = millis();
  }
}
