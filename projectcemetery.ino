// Project Cemetery
#include "math.h"                           // Library fo calculations
#include <adafruit-sht31.h>                 // Library for Temperature-Humidity sensor
#include <tsl2561.h>                        // Library for Luminosity/Lux sensor
#include "HttpClient.h"                     // Library for dweet.io
HttpClient http;
http_request_t request;
http_response_t response;
http_header_t headers[] = { { NULL, NULL } };

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

// TSL2561 sensor related vars
uint16_t integrationTime;
double illuminance;
uint32_t illuminance_int;
bool autoGainOn;

// TSL2561 execution control var
bool operational;

//TSL2561 status vars
char tsl_status[21] = "na";
char autoGain_s[4] = "na";
uint8_t error_code;
uint8_t gain_setting;

// Where to publish the data (options are: dweet, particle)
String publishMethod = "dweet";

// dweet thing name
String my_dweet_thing = "project_cemetery";

// Sleep calculation multiplier. Set higher when using a lower power solar panel.
int sleepCalculationMultiplier = 70;

// Enable debug or not
bool debugEnabled = false;

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

int calculate_sleep(double soc) {
    return (105 - soc) * sleepCalculationMultiplier;
}

int flashLedByHttpCode(long httpStatus) {
    if (httpStatus == 200) {
        flash_rgb(0, 255);
    } else {
        flash_rgb(255, 0);
    }
}

void setup()
{
// Initialization for 
  error_code = 0;
  operational = false;
  autoGainOn = false;

  // variables on the cloud
  Particle.variable("status", tsl_status);
  Particle.variable("integ_time", integrationTime);
  Particle.variable("gain", gain_setting);
  Particle.variable("auto_gain",autoGain_s);
  Particle.variable("illuminance", illuminance);
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
  if(!tsl.setTiming(false,1,integrationTime))
  {
    error_code = tsl.getError();
    strcpy(tsl_status,"setTimingError");
    return;
  }


  if (!tsl.setPowerUp())
  {
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
}


void loop() 
{
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
  double tF = (t* 9) /5 + 32;
  
  if (! isnan(t)) {  // check if 'is not a number'
     //Temperature in C
    Serial.print("Temp *C = "); Serial.println(t);
    Particle.variable("TempC", t);
    Particle.publish("TempC", t);
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
    Particle.publish("Humidity", hum);
  } else { 
    Serial.println("Failed to read humidity");
    Particle.publish("Failed to read humidity");
  }
  Serial.println();
  delay(1000);
  
 if (publishMethod == "dweet") {
    flashLedByHttpCode(send_to_dweet(my_dweet_thing, "temperature", round(t)));
    flashLedByHttpCode(send_to_dweet(my_dweet_thing, "humidity", round(hum)));
    flashLedByHttpCode(send_to_dweet(my_dweet_thing, "lightLevel", round(illuminance)));
 }
    Particle.publish("TempC", t);
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
