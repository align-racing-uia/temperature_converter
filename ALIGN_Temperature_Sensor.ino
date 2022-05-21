#include <CAN.h>

// Temperature Sensor for Battery Management System
    // For "Li-ion building block Li4P25RT"
    
const int sg1 = A0;
const int light = 13;
const int fan = 0;        // fan speed
const int outFan = 3;     // Pin out for fan PWM
double volt[33] = {2.44, 2.42, 2.40, 2.38, 2.35, 2.32, 2.27, 2.23, 2.17, 2.11, 2.05, 1.99, 1.86, 1.80, 1.74, 1.68,
                 1.63, 1.59, 1.55, 1.51, 1.48, 1.45, 1.43, 1.40, 1.38, 1.37, 1.35, 1.34, 1.33, 1.32, 1.31, 1.30};
double temp[33] = {-40, -35, -30, -25, -20, -15, -10, -5,  0, 5, 10, 15, 20, 25, 30, 35, 40, 45 , 50 , 55 , 60 , 65 , 70 , 75 , 80 , 85 , 90 , 95 , 100 ,105 ,110 ,115 ,120}; 

int diff[33] = {};
int index = 0;

double sensor1 = 0;
double temperature = 0;   // temperature


void setup() {
Serial.begin(9600);
Serial.println("Startup");
pinMode(light, OUTPUT);
}

void loop() {
  
sensor1 = (analogRead(sg1));


for (int j = 0; j <= 32; j++)
  { diff[j] = abs((volt[j])*1023/5 - sensor1); }

for (int i = 0; i <= 32; i++)
  {
    if (diff[i] <= diff[index])
    { index = i; }
  }
  
temperature = temp[index];

Serial.print("Temperature 1: ");
Serial.print(temperature);
Serial.print(" Voltage 1: ");
Serial.println(sensor1*5/1023);

if (temperature < 25)
  { fan = 0; }
else if (temperature >= 25 || temperature < 30)
  { fan = 255*(1/4); }
else if (temperature >= 30 || temperature < 35)
  { fan = 255*(2/4); }
else if (temperature >= 35 || temperature < 40)
  { fan = 255*(3/4); }
else
  { fan = 255; }


  
if (temperature >= 50)
  { digitalWrite(light, HIGH); }
else
  { digitalWrite(light, LOW); }

}
