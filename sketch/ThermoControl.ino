// Include the libraries we need
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2

#define HEATER_RELAY_OUT 3
#define HEATER_ON LOW
#define HEATER_OFF HIGH
#define HEATER_UNKNOWN -1

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;

int highTemperature = 30;
int lowTemperature = 25;
int eepromAddr = 0;
int currentHeaterState = HEATER_UNKNOWN;
String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete
float currentTemperature = 0;

/*
 * Setup function. Here we do the basics
 */
void setup(void)
{
  pinMode(HEATER_RELAY_OUT, OUTPUT);
  // start serial port
  Serial.begin(9600);
  Serial.println("Dallas Temperature IC Control Library Demo");

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

  // Method 1:
  // Search for devices on the bus and assign based on an index. Ideally,
  // you would do this to initially discover addresses on the bus and then 
  // use those addresses and manually assign them (see above) once you know 
  // the devices on your bus (and assuming they don't change).
  if (!sensors.getAddress(insideThermometer, 0)) 
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
  // assigns the first address found to insideThermometer
  //if (!oneWire.search(insideThermometer)) Serial.println("Unable to find address for insideThermometer");

  // show the addresses we found on the bus
  Serial.print("Device 0 Address: ");
  printAddress(insideThermometer);
  Serial.println();

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(insideThermometer, 12);
 
  Serial.print("Device 0 Resolution: ");
  Serial.print(sensors.getResolution(insideThermometer), DEC); 
  Serial.println();


   //saveTemperature(30,40);

   loadTemperature();

   Serial.print("Loaded: ");
   Serial.print(lowTemperature, DEC);
   Serial.print(" ");
   Serial.print(highTemperature, DEC);
   Serial.println();

   updateState(HEATER_OFF);


   inputString.reserve(200);
}

/*
 * Main function. It will request the tempC from the sensors and display on Serial.
 */
void loop(void)
{ 
  // call sensors.requestTemperatures() to issue a global temperature 
  // request to all devices on the bus
  sensors.requestTemperatures(); // Send the command to get temperatures
  
  // It responds almost immediately. Let's print out the data
  float tempC = sensors.getTempC(insideThermometer);
  Serial.print("Temp C: ");
  Serial.print(tempC);
  Serial.println();

  currentTemperature = tempC;
  control(tempC);
  
  delay(1000);
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


void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
   
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;

      handleString(inputString);      
      inputString.remove(0);
    }
  }
}

void loadTemperature()
{
  if (EEPROM.length()>=sizeof(int)*2)
  {
    EEPROM.get(eepromAddr, lowTemperature);
    EEPROM.get(eepromAddr+sizeof(int), highTemperature);
  }
}

void saveTemperature(int low, int high)
{
  lowTemperature = low;
  highTemperature = high;
  
  EEPROM.put(eepromAddr, low);
  EEPROM.put(eepromAddr+sizeof(int), high);
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
     digitalWrite(HEATER_RELAY_OUT, newState);
     currentHeaterState = newState;
  }
}

void handleString(String str)
{
  str.trim();
  
  Serial.print("COMMAND: ");
  Serial.print(str);
  Serial.println();
  
  
  if (str.startsWith("SETLOW ")){
      String tempStr = str.substring(7);
      int temp = tempStr.toInt();
      saveTemperature(temp,highTemperature);
      Serial.print("Saved low: ");
      Serial.print(tempStr);
      Serial.println();
  } else
   if (str.startsWith("SETHIGH ")){
      String tempStr = str.substring(7);
      int temp = tempStr.toInt();
      saveTemperature(lowTemperature,temp);
      Serial.print("Saved low: ");
      Serial.print(tempStr);
      Serial.println();
   }    else
   if (str.startsWith("STATUS")){
      Serial.print("LOW: ");
      Serial.print(lowTemperature);
      Serial.print(" HIGH: ");
      Serial.print(highTemperature);
      Serial.print(" CURRENT: ");
      Serial.print(currentTemperature);
      Serial.print(" HEATER: ");
      Serial.print(currentHeaterState);
      Serial.println();
  } else{
    Serial.print("Unknown command: ");
    Serial.print(str);
    Serial.println();
  }
}

