// Orion temperature canbus: https://www.orionbms.com/downloads/misc/thermistor_module_canbus.pdf
// Orion temperature third-party: https://www.orionbms.com/manuals/pdf/third_party_thermistors.pdf

#include <SPI.h>
#include <lib/mcp2515.h>
struct can_frame tempCanMessage; // To send for one peace of data. Can be duplicated for other ID's and data
MCP2515 can0(10);                // Chip select
// Temperature Sensor for Battery Management System
// For "Li-ion building block Li4P25RT"

#define VOLTAGE_OFFSET -0.10
#define LIGHT_PIN 13
int fan = 0; // fan speed
float volt[33] = {2.44, 2.42, 2.40, 2.38, 2.35, 2.32, 2.27, 2.23, 2.17, 2.11, 2.05, 1.99, 1.86, 1.80, 1.74, 1.68, 1.63, 1.59, 1.55, 1.51, 1.48, 1.45, 1.43, 1.40, 1.38, 1.37, 1.35, 1.34, 1.33, 1.32, 1.31, 1.30};
float temp[33] = {-40, -35, -30, -25, -20, -15, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120};
float diff[33] = {};
int sensorPins[5] = {A0, A1, A2, A3, A4};
float sensor1 = 0;
unsigned long registerTime = 0;
float fabs(float value)
{
  return value < 0 ? -value : value;
}
float getSensorVoltage(int sensor)
{
  return ((analogRead(sensor) * 5.0 / 1023.0) + VOLTAGE_OFFSET);
}
float convertToTemperature(float voltage)
{
  int currentIndex = 0;
  for (int j = 0; j <= 31; j++)
  {
    float delta = (volt[j] - voltage);
    diff[j] = fabs(delta);
  }

  for (int i = 0; i <= 31; i++)
  {
    if (diff[i] <= diff[currentIndex])
    {
      currentIndex = i;
    }
  }
  return temp[currentIndex];
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
  can0.setNormalMode();

  Serial.begin(9600);
  pinMode(LIGHT_PIN, OUTPUT);
  Serial.println((String) "#14,Temperature, Voltage");
  Serial.println((String) "#15,Temperature, Voltage");
  Serial.println((String) "#16,Temperature, Voltage");
  Serial.println((String) "#17,Temperature, Voltage");
  Serial.println((String) "#18,Temperature, Voltage");
  Serial.println((String) "Summary,Max,Max Index,Min,Min Index,Avg");
}
unsigned long tempSendTime, serialTime;

void loop()
{
  // --- Canbus receive ---
  struct can_frame frame;

  // if (can0.readMessage(&frame) == MCP2515::ERROR_OK)
  // {
  //   // frame contains received message
  // }
  float voltages[5] = {};
  float temperatures[5] = {};
  int i = 0;
  int tempAvg = 0;
  int maxIndex = 0;
  int minIndex = 0;
  for (int sensor : sensorPins)
  {
    voltages[i] = getSensorVoltage(sensor);
    temperatures[i] = convertToTemperature(voltages[i]);
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
    Serial.println((String) "Summary," + temperatures[maxIndex] + "," + maxIndex + "," + temperatures[minIndex] + "," + minIndex + "," + avgTemp);
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
