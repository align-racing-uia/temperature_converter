// Orion temperature canbus: https://www.orionbms.com/downloads/misc/thermistor_module_canbus.pdf
// Orion temperature third-party: https://www.orionbms.com/manuals/pdf/third_party_thermistors.pdf

#include <SPI.h>
#include <lib/mcp2515.h>
struct can_frame tempCanMessage; // To send for one peace of data. Can be duplicated for other ID's and data
MCP2515 can0(10);                // Chip select
// Temperature Sensor for Battery Management System
// For "Li-ion building block Li4P25RT"

#define VOLTAGE_OFFSET -0.10
#define SIGNAL_PIN_1 A0
#define LIGHT_PIN 13
int fan = 0;          // fan speed
#define FAN_PIN_OUT 3 // Pin out for fan PWM
float volt[33] = {2.44, 2.42, 2.40, 2.38, 2.35, 2.32, 2.27, 2.23, 2.17, 2.11, 2.05, 1.99, 1.86, 1.80, 1.74, 1.68, 1.63, 1.59, 1.55, 1.51, 1.48, 1.45, 1.43, 1.40, 1.38, 1.37, 1.35, 1.34, 1.33, 1.32, 1.31, 1.30};
float temp[33] = {-40, -35, -30, -25, -20, -15, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120};

float diff[33] = {};
int index = 0;

float sensor1 = 0;
float temperature = 0; // temperature
void setup()
{

  tempCanMessage.can_id = 0x1839F380; // CANBUS ID
  tempCanMessage.can_dlc = 8;         // Length of the message
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
  Serial.println("Startup");
  pinMode(LIGHT_PIN, OUTPUT);
}
unsigned long tempSendTime;


void loop()
{
  // --- Canbus receive ---
  struct can_frame frame;

  if (can0.readMessage(&frame) == MCP2515::ERROR_OK)
  {
    // frame contains received message
    Serial.println(frame.can_id);
  }

  sensor1 = (analogRead(SIGNAL_PIN_1));
  float voltage = ((sensor1 * 5.0 / 1023.0) + VOLTAGE_OFFSET);
  for (int j = 0; j <= 32; j++)
  {
    diff[j] = abs(((volt[j]) - voltage));
  }

  for (int i = 0; i <= 32; i++)
  {
    if (diff[i] <= diff[index])
    {
      index = i;
    }
  }

  temperature = temp[index];

  Serial.print("Temperature 1: ");
  Serial.print(temperature);
  Serial.print(" Voltage 1: ");
  Serial.println(voltage);
  Serial.println(index);
  // --- Fan controller ---
  int tmp = constrain(temperature, 25, 40);
  fan = map(tmp, 25, 40, 0, 255);

  if (temperature >= 50)
  {
    digitalWrite(LIGHT_PIN, HIGH);
  }
  else
  {
    digitalWrite(LIGHT_PIN, LOW);
  }
  delay(1000);
  // --- CAN-BUS ---
  if (millis() - tempSendTime > 100)
    can0.sendMessage(&tempCanMessage);
}
