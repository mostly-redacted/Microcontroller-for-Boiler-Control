// Import required libraries

#include <WiFi.h>               //Wifi
#include <ESPAsyncWebServer.h>  //Asynchronous Web Server
//DS18B20 libraries
#include <OneWire.h>
#include <DallasTemperature.h>

#include <LiquidCrystal_I2C.h>  //I2C LCD
#include <max6675.h>            //Added MAX 6675 Amplifier to read the K-Type thermocouple
// Libraries for SD card
#include <FS.h>
#include <SD.h>
#include <SPI.h>
// Libraries to get time from NTP Server
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

/************************Instantiate DS18B20 Sensors********************************/
// Data wire is connected to GPIO 15
#define ONE_WIRE_BUS 15

// Setup a oneWire Instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

/************************************LCD**********************************************/
// Declare lcd at address 0x27 with 16 columns and 2 rows
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address 0x27 (from DIYables LCD), 16 column and 2 rows

// Include the sensor pin for the G 1/2 flow sensor on pin 27
#define flowSensor 27

/***************************K-Type Thermocouple**************************/
//Define the pins to interface with the MAX6675 Thermocouple Amplifier
int thermoDO = 19;  //GPIO pin 19 to SO = green
int thermoCS = 23;  //GPIP pin 23 to CS = blueyjj
int thermoCLK = 5;  //GPIO pin 5 SCK = yellow

//Create Object called MAX6675 called "thermocouple1" on the pins we defined
// This is the MAX6675 amplifier used to make the K-Type thermocouple work
MAX6675 thermocouple1(thermoCLK, thermoCS, thermoDO);

//Address of Head sensor
uint8_t sensor1[8] = { 0x28, 0xB4, 0x52, 0x44, 0x9E, 0x23, 0x0B, 0x55 };
//Addresss of Boiler Sensor
uint8_t sensor2[8] = { 0x28, 0xD8, 0x47, 0x65, 0xC0, 0x23, 0x8, 0xD7 };
// Address of Water sensor
uint8_t sensor3[8] = { 0x28, 0x79, 0xFF, 0x26, 0x9E, 0x23, 0xB, 0xC8 };

// String Variables to store temperature values
String temperatureBoiler = "";
String temperatureHead = "";
String temperatureWater = "";
String temperatureRCWater = "";
String flowWater = "";

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 200;

// Wifi network credentials
const char *ssid = "SixCatHouse";
const char *password = "35Socksee";

// Define VSPI Pins as default used by K-type thermoresistor
//#define VSPI_MISO 19
//#define VSPI_MOSI 23
//#define VSPI_SCK 15
//#define VSPI_CS 5

// Define HSPI Pins as default used by SD card reader
//#define HSPI_MISO 12
//#define HSPI_MOSI 13
//#define HSPI_SCK 14
#define HSPI_CS 26  // Different pin, default is 15

// Instantiate pointers to SPI objects
// Have to begin them in setup
SPIClass vspi(VSPI);
SPIClass hspi(HSPI);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

// Save reading number on RTC memory
RTC_DATA_ATTR int readingID = 0;

// Logging on SD Card
String dataMessage;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

/************************* Flow Meter **********************************/
//Flow Meter variables in Milli Seconds
long currentMillis = 0;          // Variable Stores current time
long previousMillis = 0;         // Variable stores previous time
int interval = 1000;             // Interval for measuring flow rate (1 second)
boolean ledState = LOW;          // Variable to store LED state
float calibrationFactor = 4.5;   // Calibration factor for the flow sensor
volatile byte pulseCount;        // Variable to store pulse count (volitile as it is modified in interrupt)
byte pulse1Sec = 0;              // Pulse count in 1 second
float flowRate;                  // Flow rate variable
unsigned int flowMilliLitres;    // Flow in millilitres
unsigned long totalMilliLitres;  // Total flow in millilitres

/*******************************************************FUNCTION  To read the Temperature***************************************************/

/***Reads Condensor Head Temp in Sensor 1 Function returns Temperature in Farhenheit**************************/
String readTemperatureHead() {
  // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  sensors.requestTemperatures();
  float tempHead = sensors.getTempF(sensor1);

  // If the sensor reading is invalid return "--"
  if (int(tempHead) == -196) {
    Serial.println("Failed to read from Head sensor");
    return "--";
  } else {
    Serial.print("Head Temperature: ");
    Serial.println(tempHead);
  }
  return String(tempHead);
}

/***Reads Boiler Temp in Sensor 2 Function returns Temperature in Farhenheit******/
String readTemperatureBoiler() {
  // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  sensors.requestTemperatures();
  float tempBoiler = sensors.getTempF(sensor2);

  // If the sensor reading is invalid return "--"  if(int(tempBoiler) == -196){
  if (int(tempBoiler) == -196) {
    Serial.println("Failed to read from Boiler sensor");
    return "--";
  } else {
    Serial.print("Boiler Temperature: ");
    Serial.println(tempBoiler);
  }
  return String(tempBoiler);
}

/***Reads Water Temp in Sensor 3 Function returns Temperature in Farhenheit******/
String readTemperatureWater() {
  // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  sensors.requestTemperatures();
  float tempWater = sensors.getTempF(sensor3);

  // If the sensor reading is invalid return "--"
  if (int(tempWater) == -196) {
    Serial.println("Failed to read from Water sensor");
    return "--";
  } else {
    Serial.print("Water Temperature: ");
    Serial.println(tempWater);
  }
  return String(tempWater);
}

/***********************Reads the RC water temp from K-Type Thermocouple*************************/
String readTemperatureRCWater() {
  float tempRCWater = thermocouple1.readFahrenheit();  //Reads the Fahrenheit of the thermocouple

  // If the sensor reading is invalid return "--"
  if (int(tempRCWater) == -196) {
    Serial.println("Failed to read from RCWater sensor");
    return "--";
  } else {
    Serial.print("RC Water Temperature: ");
    Serial.println(tempRCWater);
  }
  return String(tempRCWater);
}

/************************** Pulse Counter for Flow Meter**************************/

void IRAM_ATTR pulseCounter()  // Increment pulse count (interrupt service routine)
{
  pulseCount++;
}

/*************************************Building the web page************************************************************/

//The HTML and the CSS needed to build the page are saved on the index_html Variable
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
.container {
	font-family: Arial;
    display: inline-block;
    margin: 0px auto;
    text-align: center;
    }
.units { font-size: 3rem; }
.labels{
    font-size: 3rem;
    vertical-align:middle;
    padding-bottom: 400px;
  	text-align: center;
  	color: black;
}
.temps{
    font-size: 7rem;
    vertical-align:middle;
    padding-bottom: 0px;
  	text-align: center;
  	color: black;
}

.boiler {
  position: absolute;
  top: 515px;
  left: 480px;
}

.head {
  position: absolute;
  top: 80px;
  left: 815px;
}

.water {
  position: absolute;
  top: 380px;
  left: 1470px;
}

.rc-water {
  position: absolute;
  top: 80px;
  left: 100px;
}

.flow {
  position: absolute;
  top: 280px;
  left: 120px;
}

</style>
</head>
<body>
<div class="container">
<!-- Image Hosted at imgbox -->
<!-- Base64 Image Below -->
<img src="https://images2.imgbox.com/05/91/Q2GwutBE_o.jpg" alt="Still Life"
style="width:1800px;height:970px;">
  
  	<div class="boiler">
    	<span class="temps" style="color:#D21404;">
      <span id="temperatureBoiler">%TEMPERATUREBOILER%</span>
      <sup class="units">&deg;F</sup>
      <br>
      <span class="labels">Boiler</span>
    </div>
    
  	<div class="head">
    	<span class="temps" style="color:#FF4500;">
      <span id="temperatureHead">%TEMPERATUREHEAD%</span>
    	<sup class="units">&deg;F</sup>
      <br>
      <span class="labels">Head</span>
    </div>
    
  	<div class="water">
    	<span class="temps" style="color:#82EEFD;">
      <span id="temperatureWater">%TEMPERATUREWATER%</span>
    	<sup class="units">&deg;F</sup>
      <br>
      <span class="labels">Water</span>
    </div>
    
  	<div class="rc-water">
    	<span class="temps" style="color:#FF4500;">
      <span id="temperatureRCWater">%TEMPERATURERCWATER%</span>
    	<sup class="units">&deg;F</sup>
      <br>
      <span class="labels">RC Return</span>
     </div>
    
    <div class="flow">
    	<span class="temps" style="color:#1338BE;">
      <span id="flowWater">%FLOWWATER%</span>
    	<sup class="units">L/Sec</sup>
      <br>
      <span class="labels">RC Flow Rate</span>
     </div>
</div>
</body>
</html>

<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperatureBoiler").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperatureBoiler", true);
  xhttp.send();
}, 500) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperatureHead").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperatureHead", true);
  xhttp.send();
}, 500) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperatureWater").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperatureWater", true);
  xhttp.send();
}, 500) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperatureRCWater").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperatureRCWater", true);
  xhttp.send();
}, 500) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("flowWater").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/flowWater", true);
  xhttp.send();
}, 500) ;
</script>
</html>)rawliteral";

/************************Processor Function********************************/
// Replaces temperature location placeholder (%TEMPERATUREF%) with actual DS18B20 values
String processor(const String &var) {
  if (var == "TEMPERATUREBOILER") {
    return temperatureBoiler;
  } else if (var == "TEMPERATUREHEAD") {
    return temperatureHead;
  } else if (var == "TEMPERATUREWATER") {
    return temperatureWater;
  } else if (var == "TEMPERATURERCWATER") {
    return temperatureRCWater;
  } else if (var == "FLOWWATER") {
    return flowWater;
  }
  return String();
}

/**************************************SETUP******************************/

void setup() {
  // Initialize the Serial Monitor for debugging purposes
  Serial.begin(115200);
  Serial.println();

  // Connect to Wi-Fi & print the IP address of the ESP32
  Serial.println("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP Acquired: ");
  Serial.println(WiFi.localIP());

  //Initialize the Temperature sensor
  // Start up the DS18B20 library
  sensors.begin();

  // Initialize the LCD
  lcd.begin();      // initialize the lcd
  lcd.backlight();  // open the backlight

  // Call the begin() method on those objects for the 2 buses
  vspi.begin();
  hspi.begin();

  // Set the SS pins as outputs
  //pinMode(VSPI_CS, OUTPUT);  //VSPI CS
  pinMode(HSPI_CS, OUTPUT);  //HSPI CS

  // Initialize NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +5 = 18800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(18000);

  // Initialize SD card on the HSPI bus
  Serial.println("Initializing SD card...");
  hspi.begin(HSPI_CS);
  if (!SD.begin(HSPI_CS, hspi, 4000000)) {
    Serial.println("Card Mount Failed");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.print("SD Card Size: ");
  Serial.print(cardSize);
  Serial.println("MB");

  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/data.txt");
  if (!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/data.txt", "Reading ID, Date, Hour, Temperature \r\n");
  }
  else {
    Serial.println("File already exists");
  }
  file.close();

  getReadings();
  getTimeStamp();
  logSDCard();
  
  // Increment readingID on every new reading
  readingID++;

  // Flow Sensor mode
  pinMode(flowSensor, INPUT_PULLUP);  // Set the sensor pin as input with internal pull-up

  pulseCount = 0;        // Initialize the pulse count
  flowRate = 0.0;        // Initialize the flow rate
  flowMilliLitres = 0;   // Initialize flow in milliliters
  totalMilliLitres = 0;  // Initialize total flow in milliliters
  previousMillis = 0;    // Initialize preveious time

  attachInterrupt(digitalPinToInterrupt(flowSensor), pulseCounter, FALLING);  // Attach interrupt to the sensor pin

  /**************************************WEB SERVER********************************************/

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {  //Make a request on the root URL
    request->send_P(200, "text/html", index_html, processor);    //Send the HTML text that is stored in the 'index_html' variable.
                                                                 //                                                             Pass the 'processor' function - This replaces all the placeholders with the correct values
  });
  server.on("/temperatureBoiler", HTTP_GET, [](AsyncWebServerRequest *request) {  //A Request is received on the /temperature.c URL
    request->send_P(200, "text/plain", temperatureBoiler.c_str());                //Send the updated Boiler temperature value. Plain text, so sent as a char - Use the 'c_str()'method.
  });
  server.on("/temperatureHead", HTTP_GET, [](AsyncWebServerRequest *request) {  //A Request is received on the /temperature.c URL
    request->send_P(200, "text/plain", temperatureHead.c_str());                //Send the updated Head temperature value. Plain text, so sent as a char - Use the 'c_str()'method.
  });
  server.on("/temperatureWater", HTTP_GET, [](AsyncWebServerRequest *request) {  //A Request is received on the /temperature.c URL
    request->send_P(200, "text/plain", temperatureWater.c_str());                //Send the updated Water temperature value. Plain text, so sent as a char - Use the 'c_str()'method.
  });
  server.on("/flowWater", HTTP_GET, [](AsyncWebServerRequest *request) {  //A Request is received on the /flowWater.c URL
    request->send_P(200, "text/plain", flowWater.c_str());                //Send the updated Flow Rate value. Plain text, so sent as a char - Use the 'c_str()'method.
  });
  // Start the server
  server.begin();
}

void loop() {
  // Read the temperatures from the sensors
  if ((millis() - lastTime) > timerDelay) {
    temperatureBoiler = readTemperatureBoiler();
    temperatureHead = readTemperatureHead();
    temperatureWater = readTemperatureWater();
    temperatureRCWater = readTemperatureRCWater();
    lastTime = millis();
  }
  //Read the flow from the flow sensor
  currentMillis = millis();  // Get the current time
  if (currentMillis - previousMillis > interval) {

    pulse1Sec = pulseCount;  // Copy the pulse count
    pulseCount = 0;          // Reset the pulse count

    /**********************Flow Rate Math*************************************************/
    // Because this loop may not complete in exactly 1 second intervals we calculate
    // the number of milliseconds that have passed since the last execution and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure (litres/minute in
    // this case) coming from the sensor.
    flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
    previousMillis = millis();  // update previous time

    /********************Convert to millilitres******************************************/
    // Divide the flow rate in litres/minute by 60 to determine how many litres have
    // passed through the sensor in this 1 second interval, then multiply by 1000 to
    // convert to millilitres.
    flowMilliLitres = (flowRate / 60) * 1000;

    // Add the millilitres passed in this second to the cumulative total
    totalMilliLitres += flowMilliLitres;

    /*************dtostrf Double To String Function = Change Float value to String**************************************/
    char flowVal[10];       //temporarily holds data from values called "flowVal"
    String flowWater = "";  //data on buffer is copied to this string called "flowWater"

    dtostrf(flowRate, 4, 2, flowVal);  // float value (flowRate) defined in meter at beginning is copied onto buffer called "flowVal"; 4 is minimum width, 3 is precision

    //Serial.print("flowVal: ");
    //Serial.print(flowVal);            // prints the char value "flowVal"
    //Serial.println();

    // Convert char arry to string
    flowWater += flowVal;
    Serial.print("flowWater: ");
    Serial.println(flowWater);  // prints the string value "flowWater"

    /*******************************Serial Plotter***************************/
    //Print Boiler Temperature
    Serial.print("Variable_1:");
    Serial.print(temperatureBoiler);
    Serial.print("',");
    Serial.print("\t");  // Print tab space

    //Print Head  Temperature
    Serial.print("Variable_2:");
    Serial.print(temperatureHead);
    Serial.print("',");
    Serial.print("\t");  // Print tab space

    //Print Water Temperature
    Serial.print("Variable_3:");
    Serial.print(temperatureWater);
    Serial.print("',");
    Serial.print("\t");  // Print tab space

    //Print K-Type thermocouple Temperature
    Serial.print("Variable_4:");
    Serial.println(temperatureRCWater);
    Serial.print("\t");  // Print tab space

    //Print water flow rate from G 1/2 sensor
    Serial.print("Variable_5:");
    Serial.println(flowWater);
    Serial.print("\t");  // Print tab space

    /****************************Print the Flow rate of the Flow Meter******************/
    // Print the flow rate for this second in litres / minute
    Serial.print("Flow rate: ");
    Serial.print(int(flowRate));  // Print the integer part of the variable flowRate
    Serial.print("L/min");
    Serial.print("\t");  // Print tab space

    // Print the cumulative total of litres flowed since starting
    Serial.print("Output Liquid Quantity: ");
    Serial.print(totalMilliLitres);  // Print Total Millilitres
    //Serial.print("mL / ");
    Serial.print(totalMilliLitres / 1000);  // Print total Litres
    Serial.println("L");

    /*****************************LCD Panel Display****************************/
    lcd.clear();
    lcd.setCursor(0, 0);           // display position
    lcd.print("Boiler");           // display the word "Boiler"
    lcd.setCursor(0, 1);           // display position
    lcd.print(temperatureBoiler);  // display the boiler temperature in Farhenheit
    lcd.print((char)223);          // display ° character
    lcd.print("F");
    lcd.setCursor(9, 0);         // display position
    lcd.print("Head");           // display the word "Head"
    lcd.setCursor(9, 1);         // display position
    lcd.print(temperatureHead);  // display the condensor head temperature in Farhenheit
    lcd.print((char)223);        // display ° character
    lcd.print("F");

    delay(1000);
  }
}
/* Function to get temperature
void getReadings() {
  sensors.requestTemperatures();
  //temperature = sensors.getTempCByIndex(0); // Temperature in Celsius
  temperature = sensors.getTempFByIndex(0);  // Temperature in Fahrenheit
  Serial.print("Temperature: ");
  Serial.println(temperature);
}
*/

// Function to get date and time from NTPClient
void getTimeStamp() {
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  Serial.println(dayStamp);
  // Extract time
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
  Serial.println(timeStamp);
}

// Write the sensor readings on the SD card
void logSDCard() {
  dataMessage = String(readingID) + "," + String(dayStamp) + "," + String(timeStamp) + "," + String(temperature) + "\r\n";
  Serial.print("Save data: ");
  Serial.println(dataMessage);
  appendFile(SD, "/data.txt", dataMessage.c_str());
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}
