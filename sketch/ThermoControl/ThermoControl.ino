#include <ModbusRtu.h>
#include <Timer.h>

// for DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
// for permanent parameters storage
#include <EEPROM.h>

// specify pin with temperature sensor bus
#define TEMP_SENSOR_BUS_PIN 2
// specify pin with heater relay control
#define HEATER_RELAY_OUT_PIN 3
// heater status constants
#define HEATER_ON LOW
#define HEATER_OFF HIGH
#define HEATER_UNKNOWN -1

#define HEATER_CONTROL_INTERVAL 1000

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(TEMP_SENSOR_BUS_PIN);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress waterThermometer;

// timer for control operation and temperature update
Timer t;

// Modbus slave object
Modbus slave(5, 0, 10);

int highTemperature = 30;
int lowTemperature = 25;
int eepromAddr = 0;
int currentHeaterState = HEATER_UNKNOWN;
String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete
float currentTemperature = 0;

uint16_t modbusRegisters[16] = {
  0, 0, 0, 0, 0, 0, 28182, 8, 0, 0, 0, 0, 0, 0, 1, -1 };


/*
 * Setup function. Here we do the basics
 */
void setup(void)
{
  pinMode(HEATER_RELAY_OUT_PIN, OUTPUT);
  // start serial port
  Serial.begin(9600);
  slave.begin(9600,SERIAL_8E1);

  // locate devices on the bus
  Serial.print("Locating devices...");
  sensors.begin();
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // report parasite power requirements
  Serial.print("Parasite power is: ");
  if (sensors.isParasitePowerMode()) Serial.println("ON");
    else Serial.println("OFF");

  if (!sensors.getAddress(waterThermometer, 0))
      Serial.println("Unable to find address for Device 0");

  // method 2: search()
  // search() looks for the next device. Returns 1 if a new address has been
  // returned. A zero might mean that the bus is shorted, there are no devices,
  // or you have already retrieved all of them. It might be a good idea to
  // check the CRC to make sure you didn't get garbage. The order is
  // deterministic. You will always get the same devices in the same order
  //
  // Must be called before search()
  //oneWire.reset_search();
  // assigns the first address found to waterThermometer
  //if (!oneWire.search(waterThermometer)) Serial.println("Unable to find address for waterThermometer");

  // show the addresses we found on the bus
  Serial.print("Device 0 Address: ");
  printAddress(waterThermometer);
  Serial.println();

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(waterThermometer, 12);

  Serial.print("Device 0 Resolution: ");
  Serial.print(sensors.getResolution(waterThermometer), DEC);
  Serial.println();

   loadTemperature();

   Serial.print("Loaded: ");
   Serial.print(lowTemperature, DEC);
   Serial.print(" ");
   Serial.print(highTemperature, DEC);
   Serial.println();

   updateState(HEATER_OFF);

   t.every(2000, &updateTemp);
}

/*
 * Main function. It will request the tempC from the sensors and display on Serial.
 */
void loop(void)
{
  slave.poll( modbusRegisters, 16 );
  modbusCheckUpdates();
  t.update();
}

void updateTemp()
{
  sensors.requestTemperatures(); // Send the command to get temperatures
  
  float tempC = sensors.getTempC(waterThermometer);
  //Serial.print("Temp C: ");
  //Serial.print(tempC);
  //Serial.println();

  currentTemperature = tempC;
  modbusSetCurrentTemperature();
  control(tempC);
  
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
      Serial.print(deviceAddress[i], HEX);
  }
}

void loadTemperature()
{
  if (EEPROM.length()>=sizeof(int)*2)
  {
    EEPROM.get(eepromAddr, lowTemperature);
    EEPROM.get(eepromAddr+sizeof(int), highTemperature);

    modbusSetLowHigh();
  }
}

void saveTemperature(int low, int high)
{
  lowTemperature = low;
  highTemperature = high;

  EEPROM.put(eepromAddr, low);
  EEPROM.put(eepromAddr+sizeof(int), high);

  modbusSetLowHigh();
}


void control(float currentTemperature)
{
  float high = (float)highTemperature;
  float low = (float)lowTemperature;

  if (currentTemperature > highTemperature)
  {
    updateState(HEATER_OFF);
  } else{
    if (currentTemperature < lowTemperature)
    {
      updateState(HEATER_ON);
    }
  }
}


void updateState(int newState)
{
  if (currentHeaterState != newState)
  {
     digitalWrite(HEATER_RELAY_OUT_PIN, newState);
     currentHeaterState = newState;
     modbusSetHeaterState();
  }
}

void modbusSetLowHigh()
{
  modbusRegisters[3] = lowTemperature;
  modbusRegisters[4] = highTemperature;
}

void modbusSetHeaterState()
{
  modbusRegisters[5] = 1-currentHeaterState;
}

void modbusSetCurrentTemperature()
{
  uint16_t intTempC = (uint16_t)currentTemperature;
  uint16_t decTempC = (uint16_t)((currentTemperature-intTempC)*100);
  modbusRegisters[1] = intTempC;
  modbusRegisters[2] = decTempC;  
}

void modbusCheckUpdates()
{
  uint16_t newLow = modbusRegisters[3];
  uint16_t newHigh = modbusRegisters[4];

  if (lowTemperature!=newLow || highTemperature!=newHigh)
  {
    saveTemperature(newLow, newHigh);  
  }
}

