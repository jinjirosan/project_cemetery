// Project Cemetery - rewrite 0.9.4 
#include "math.h"                           // Library fo calculations
#include <adafruit-sht31.h>                 // Library for Temperature-Humidity sensor
#include <tsl2561.h>                        // Library for Luminosity/Lux sensor
#include <SparkFunBQ27441.h>                // Library for battery - LiPo gauge
#include "HttpClient.h"                     // Library for dweet.io

// Set BATTERY_CAPACITY to attached LiPo
const unsigned int BATTERY_CAPACITY = 2500;

// transmit to dweet.io pre-reqs
HttpClient http;
http_request_t request;
http_response_t response;
http_header_t headers[] = { { NULL, NULL } };

//instanciate the Temperature-Humidity sensor
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// instanciate a TSL2561 object with I2C address 0x39 --> TSL2561_ADDR (0x39), TSL2561_ADDR_1 (0x49), TSL2561_ADDR_0 (0x29)
TSL2561 tsl = (TSL2561_ADDR);

/*
TSL2561 sensor.
Connections:
Photon                      tsl2561
D1                          <---->  SCL
D0                          <---->  SDA
Not connected (floating)    <---->  ADDR
*/

// TSL2561 light sensor related vars
uint16_t integrationTime;
double illuminance;
uint32_t illuminance_int;
bool autoGainOn;

// TSL2561 light execution control var
bool operational;

//TSL2561 light status vars
char tsl_status[21] = "na";
char autoGain_s[4] = "na";
uint8_t error_code;
uint8_t gain_setting;

//BQ27441 babysitter status vars
char lipo_status[21] = "na";

// Where to publish the data (options are: "dweet" or "particle" depending on where to send the output)
String publishMethod = "dweet";

// dweet thing name
String my_dweet_thing = "project_cemetery";

// Sleep calculation multiplier. Set higher when using a lower power solar panel.
int sleepCalculationMultiplier = 70;

// Enable debug or not
bool debugEnabled = false;

// function to send to dweet.io , need to fix in dict/array to transmit once instead of 4 seperate times
int send_to_dweet(String thing, String key, float value) {
    request.hostname = "dweet.io";
    request.port = 80;
    request.path = "/dweet/for/" + thing + "?" + key + "=" + value;
    request.body = "";
    http.post(request, response, headers);
    if (debugEnabled) {
        String msg = "Sent key " + key + " with value " + value + " to dweet for the " + thing + " and got response: " + response.status;
        Serial.println(msg);
    }
    return response.status;
}

// visual notification on Photon itself: pattern
int flash_rgb(int r=0, int g=0, int b=0, long time_delay=40) {
    RGB.control(true);
    RGB.color(0, 0, 0);
    delay(time_delay);
    RGB.color(r, g, b);
    delay(time_delay);
    RGB.color(0, 0, 0);
    delay(time_delay);
    RGB.control(false);
}

// for solar/LiPo power circuit and battery save mode
//int calculate_sleep(double soc) {
//    return (105 - soc) * sleepCalculationMultiplier;
//}

// visual notification on Photon itself: flash sequence for httpStatus
int flashLedByHttpCode(long httpStatus) {
    if (httpStatus == 200) {
        flash_rgb(0, 255);
    } else {
        flash_rgb(255, 0);
    }
}

// Soil Moisture Sensor requirements
int soilval = 0; //soilvalue for storing moisture soilvalue
int soilPin = A3;//Declare a variable for the soil moisture sensor
int soilPower = 3;//Variable for Soil moisture Power
int thresholdUp = 3400;//everything higher means the soil is soaked
int thresholdCenter = 2000;//between 2000-3400, everything is good
int thresholdDown = 400;//between 400-2000, plants get thirsty. Everything lower needs water
String DisplayWords;//declare string to use for action

// Get the soil moisture content. Turn power on/off to decrease chances of corrosion (increase lifespan).
int readSoil() {
    digitalWrite(soilPower, HIGH);//turn D3 "On"
    delay(10);//wait 10 milliseconds
    soilval = analogRead(soilPin);//Read the SIG soilvalue form sensor
    digitalWrite(soilPower, LOW);//turn D3 "Off"
    return soilval;//send current moisture soilvalue
}

void BatteryStatus()
{
// Read battery stats from the BQ27441-G1A
unsigned int lp_soc = lipo.soc();  // Read state-of-charge (%)
unsigned int lp_volts = lipo.voltage(); // Read battery voltage (mV)
int lp_current = lipo.current(AVG); // Read average current (mA)
unsigned int lp_fullCapacity = lipo.capacity(FULL); // Read full capacity (mAh)
unsigned int lp_capacity = lipo.capacity(REMAIN); // Read remaining capacity (mAh)
int lp_power = lipo.power(); // Read average power draw (mW)
int lp_health = lipo.soh(); // Read state-of-health (%)

Particle.publish("lipo: state-of-charge", String(lp_soc) + " %");
Particle.publish("lipo: capacity remain", String(lp_capacity) + " mAh");
Particle.publish("lipo: avg current", String(lp_current) + " mA");
Particle.publish("lipo: state-of-health", String(lp_health) + " %");
}

void setup()
{
// initialize the BQ27441-G1A and confirm that it's connected and communicating
  if (!lipo.begin()) // begin() will return true if communication is successful
  {
    strcpy(lipo_status,"Error: Unable to communicate with BQ27441");
    Particle.publish("Battery babysitter", lipo_status);
  }
  else {
    strcpy(lipo_status,"Connected to BQ27441!");
    Particle.publish("Battery babysitter", lipo_status);
  }

// set lipo.setCapacity(BATTERY_CAPACITY) based on variable
lipo.setCapacity(BATTERY_CAPACITY);
//Particle.publish("Battery size", BATTERY_CAPACITY);

// Initialization for
  error_code = 0;
  operational = false;
  autoGainOn = false;

  // variables on the cloud
  Particle.variable("lux_status", tsl_status);
  Particle.variable("lipo_status", lipo_status);
  //Particle.variable("integ_time", integrationTime);
  //Particle.variable("gain", gain_setting);
  //Particle.variable("auto_gain",autoGain_s);
  //Particle.variable("illuminance", illuminance);
  Particle.variable("int_ill", illuminance_int);

  //function on the cloud: change sensor exposure settings (mqx 4)
  Particle.function("setExposure", setExposure);

  //connecting to light sensor device
  if (tsl.begin()) {
    strcpy(tsl_status,"tsl2561 found");
    Particle.publish("Temperature-Humidity sensor", tsl_status);
  }
  else {
    strcpy(tsl_status,"tsl 2561 not found ");
    Particle.publish("Temperature-Humidity sensor", tsl_status);
  }

  // setting the sensor: gain x1 and 101ms integration time
  if(!tsl.setTiming(false,1,integrationTime)) {
    error_code = tsl.getError();
    strcpy(tsl_status,"setTimingError");
    return;
  }

  if (!tsl.setPowerUp()) {
    error_code = tsl.getError();
    strcpy(tsl_status,"PowerUPError");
    return;
  }

  // device initialized
  operational = true;
  strcpy(tsl_status,"initOK");
  Particle.publish("Temperature-Humidity sensor", tsl_status);

// Initialization for temperature/humidity sensor
  Serial.begin(115200);
  Serial.println("SHT31 test");
  if (! sht31.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
  }

  // Soil Moisture Sensor setup
  pinMode(soilPower, OUTPUT);//Set D3 as an OUTPUT
  digitalWrite(soilPower, LOW);//Set to LOW so no power is flowing through the sensor
}

void loop()
{
BatteryStatus();
// Battery statistics to Photon variables
  //Particle.variable("SOC", String(soc));
  //Particle.publish("charge state", String(soc));

// Loop for Lux sensor
  uint16_t broadband, ir;

  // update exposure settings display vars
  if (tsl._gain)
    gain_setting = 16;
  else
    gain_setting = 1;

  if (autoGainOn)
    strcpy(autoGain_s,"yes");
  else
    strcpy(autoGain_s,"no");

  if (operational)
  {
    // device operational, update status vars
    strcpy(tsl_status,"OK");

    // get raw data from sensor
    if(!tsl.getData(broadband,ir,autoGainOn))
    {
      error_code = tsl.getError();
      strcpy(tsl_status,"saturated?");
      operational = false;
    }

    // compute illuminance value in lux
    if(!tsl.getLux(integrationTime,broadband,ir,illuminance))
    {
      error_code = tsl.getError();
      strcpy(tsl_status,"getLuxError");
      operational = false;
    }

    // try the integer based calculation
    if(!tsl.getLuxInt(broadband,ir,illuminance_int))
    {
      error_code = tsl.getError();
      strcpy(tsl_status,"getLuxIntError");
      operational = false;
    }

  }
  else
  // device not set correctly
  {
      strcpy(tsl_status,"OperationError");
      illuminance = -1.0;
      // trying a fix
      // power down the sensor
      tsl.setPowerDown();
      delay(100);
      // re-init the sensor
      if (tsl.begin())
      {
        // power up
        tsl.setPowerUp();
        // re-configure
        tsl.setTiming(tsl._gain,1,integrationTime);
        // try to go back normal again
        operational = true;
      }
  }

  delay(2000);

// ROUND loop for temperature-humidity
  double t = round(sht31.readTemperature()*10)/10.0;
  double hum = round(sht31.readHumidity()*10)/10.0;
  double tF = round(((t* 9) /5 + 32)*10)/10.0;
  double s = readSoil();

  if (! isnan(t)) {  // check if 'is not a number'
     //Temperature in C
    Serial.print("Temp *C = "); Serial.println(t);
    Particle.variable("TempC", t);
    Particle.publish("TempC", String(t));
    //Temperature in F
    Serial.print("Temp *F = "); Serial.println(tF);
    Particle.variable("TempF", tF);
  } else {
    Serial.println("Failed to read temperature");
    Particle.publish("Failed to read temperature");
  }

  if (! isnan(hum)) {  // check if 'is not a number'
    Serial.print("Hum. % = "); Serial.println(hum);
    Particle.variable("Humidity", hum);
    Particle.publish("Humidity", String(hum));
  } else {
    Serial.println("Failed to read humidity");
    Particle.publish("Failed to read humidity");
  }

  if (! isnan(s)) {  // check if 'is not a number'
    Serial.print("moisture = "); Serial.println(s);
    Particle.variable("moisture", s);
    Particle.publish("Soil moisture", String(s));
  } else {
    Serial.println("Failed to read Soil moisture level");
    Particle.publish("Failed to read Soil moisture level");
  }
  if (s <= thresholdDown){
    DisplayWords = "Dry, needs water!!";
    Particle.publish("Plant life", String(DisplayWords));
  } else if ((thresholdDown < s) && (s <= thresholdCenter)){
    DisplayWords = "Getting thirsty";
    Particle.publish("Plant life", String(DisplayWords));
  } else if ((thresholdCenter < s) && (s <= thresholdUp)){
    DisplayWords = "Normal, all good!";
    Particle.publish("Plant life", String(DisplayWords));
  } else if (s >= thresholdUp){
    DisplayWords = "Soil soaked";
    Particle.publish("Plant life", String(DisplayWords));
  }
  Serial.println();
  delay(2000);

  if (publishMethod == "particle") {
    Particle.publish("temperature", String(t));
    delay(2000);
    Particle.publish("humidity", String(hum));
    delay(2000);
    Particle.publish("lightLevel", String(illuminance));
    delay(2000);
 }

 if (publishMethod == "dweet") {
    flashLedByHttpCode(send_to_dweet(my_dweet_thing, "Temperature", round(t*10)/10.0));
    delay(1000);
    flashLedByHttpCode(send_to_dweet(my_dweet_thing, "Humidity", round(hum*10)/10.0));
    delay(1000);
    flashLedByHttpCode(send_to_dweet(my_dweet_thing, "LightLevel", round(illuminance)));
    delay(1000);
    flashLedByHttpCode(send_to_dweet(my_dweet_thing, "SoilMoistureLevel", s));
    delay(1000);
 }
}

// cloud function to change exposure settings (gain and integration time)
int setExposure(String command)
//command is expected to be [gain={0,1,2},integrationTimeSwitch={0,1,2}]
// gain = 0:x1, 1: x16, 2: auto
// integrationTimeSwitch: 0: 14ms, 1: 101ms, 2:402ms

{
    // private vars
    char gainInput;
    uint8_t itSwitchInput;
    boolean _setTimingReturn = false;

    // extract gain as char and integrationTime swithc as byte
    gainInput = command.charAt(0);//we expect 0, 1 or 2
    itSwitchInput = command.charAt(2) - '0';//we expect 0,1 or 2

    if (itSwitchInput >= 0 && itSwitchInput < 3){
      // acceptable integration time value, now check gain value
      if (gainInput=='0'){
        _setTimingReturn = tsl.setTiming(false,itSwitchInput,integrationTime);
        autoGainOn = false;
      }
      else if (gainInput=='1') {
        _setTimingReturn = tsl.setTiming(true,itSwitchInput,integrationTime);
        autoGainOn = false;
      }
      else if (gainInput=='2') {
        autoGainOn = true;
        // when auto gain is enabled, set starting gain to x16
        _setTimingReturn = tsl.setTiming(true,itSwitchInput,integrationTime);
      }
      else{
        // no valid settings, raise error flag
        _setTimingReturn = false;
      }
    }
    else{
      _setTimingReturn = false;
    }

    // setTiming has an error
    if(!_setTimingReturn){
        // set appropriate status variables
        error_code = tsl.getError();
        strcpy(tsl_status,"CloudSettingsError");
        //disable getting illuminance value
        operational = false;
        return -1;
    }
    else {
      // all is good
      operational = true;
      return 0;
    }
}
