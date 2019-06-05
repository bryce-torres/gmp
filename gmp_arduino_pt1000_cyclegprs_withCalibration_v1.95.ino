/*
   GMP Control Kit
   This version includes
   - Calibration Mode (Android App Compatible, PuTTY Software Compatible)
   - Normal Operation
*/

#include <SoftwareSerial.h>
#include <SPI.h>
#include <SD.h>
#include <GMP_GPRS.h>

/** Constants and Variables **/
// constants for Atlas Scientific devices
#define ATLAS_RESERVE_BYTES 30
#define ATLAS_BAUDRATE 9600
#define TERMINATE_KEY 13           // carriage return
#define DEBUG_BAUDRATE 115200
#define DELAY_BETWEEN_READ 3600000 // in milliseconds previously 3600000
#define CSPIN 53

// constants for depth sensor
#define SWL_ADC_PIN 5  //ADC pin for depth sensor
#define ADC_ZERO_DEPTH 166   //ADC reading when above water
//#define DEPTH_SENSOR_SLOPE 136.17 //computed slope of line (5m)
//#define DEPTH_SENSOR_SLOPE 37.21  //computed slope of line (20m) - Version 1
#define DEPTH_SENSOR_SLOPE 37.14    //computed slope of line (20m) - Version 2
//#define DTG 5.83  //distance of depth sensor tip to ground in meters - Malabon
//#define DTG 3.42  //distance of depth sensor tip to ground in meters - Marikina
#define DTG 24 //distance of depth sensor tip to ground in meters - Alabang
//#define DTG 0.586   //Beer Tower

// constants for voltage sensor
#define BATT_ADC_PIN 3 //ADC pin for voltage sensor
//#define BATT_RAW_VALUE 504 // Malabon Voltage Sensor
#define BATT_RAW_VALUE 561 // Marikina Voltage Sensor
//#define BATT_VOLTAGE_VALUE 12.71 //Malabon Volatge Sensor
#define BATT_VOLTAGE_VALUE 13.15 //Marikina Volatge Sensor

#define BATT_INTERCEPT -2.0793
#define BATT_SLOPE 0.028

// constants for calibration
#define DELAY_FOR_CALMENU 20

/** Variables **/
// SD Card File
File myFile;
const char* FILENAME = "DATALOG.txt";

// strings for Atlas Scientific Sensors
String sensorstring_PH = "";
String sensorstring_ETS = "";
String sensorstring_Temp = "";

// arrays to be sorted
float pH_array[9];
float temp_array[9];
long EC_array[9];
long TDS_array[9];
long SAL_array[9];

// helper variables for sorting EC, TDS, and Salinity
char sensorstring_array[30];
long ETSMedian[3];
char *EC;
char *TDS;
char *SAL;

// strings for the median output
String ECMedian_string;
String TDSMedian_string;
String SALMedian_string;
String pHMedian_string;
String tempMedian_string;

// variables for depth sensor
String SWL = "";

// variables for voltage sensor
String voltage = "";

// variables for calibration
String sensorstring1 = "";                        //a string to hold the data from the Atlas Scientific product
String sensorstring2 = "";                        //a string to hold the data from the Atlas Scientific product
String sensorstring3 = "";                        //a string to hold the data from the Atlas Scientific product

// variables for network
//String APIkey = "api.thingspeak.com/update?api_key=Y11K5Y5FLAT8QQKF"; //Malabon API Key
//String APIkey = "api.thingspeak.com/update?api_key=T7A3ZOT76DFRO5H9"; //Marikina API Key
String APIkey = "api.thingspeak.com/update?api_key=C68INJCBUQYYEK1T"; //Alabang API Key

/** Functions **/
// Sorting Function
// qsort requires you to create a sort function
int sort_desc(const void *cmp1, const void *cmp2)
{
  // Need to cast the void * to int *
  int a = *((int *)cmp1);
  int b = *((int *)cmp2);
  // The comparison
  return a > b ? -1 : (a < b ? 1 : 0);
  // A simpler, probably faster way:
  //return b - a;
}

// Setup Functions for Atlas Scientific Sensors
void setupPHProbe() {
  Serial1.print("C,0\r");
  delay(100);
  Serial1.print("L,0\r");
  delay(100);
  Serial1.readString(); // flush input buffer
}

void setupTempProbe() {
  Serial3.print("C,0\r");
  delay(100);
  Serial3.print("L,0\r");
  delay(100);
  Serial3.readString();
}

void setupECProbe() {
  Serial2.print("C,0\r");    // Continuous Reading Off
  delay(100);
  Serial2.print("L,0\r");    // LED off
  delay(100);
  Serial2.print("K,0.1\r");  // Set K value
  delay(100);
  Serial2.print("T,25\r");   // Set Temperature Compensation
  delay(100);
  Serial2.print("O,EC,1\r"); // Enable Parameter on Output String (EC, S, TDS)
  delay(100);
  Serial2.print("O,S,1\r");
  delay(100);
  Serial2.print("O,TDS,1\r");
  delay(100);
  Serial2.print("O,SG,0\r"); // Disable Parameter on Output String (SG)
  delay(100);
  Serial2.readString();      // flush input buffer
}

// Read Functions for Atlas Scientific Sensors
float readPH() {
  Serial1.print("R\r");
  delay(100);
  sensorstring_PH = Serial1.readStringUntil(TERMINATE_KEY);
  Serial1.readString();  //flush input buffer
  return sensorstring_PH.toFloat();
}

float readTemp() {
  Serial3.print("R\r");
  delay(100);
  sensorstring_Temp = Serial3.readStringUntil(TERMINATE_KEY);
  Serial3.readString();  //flush input buffer
  return sensorstring_Temp.toFloat();
}

String readETS() {
  Serial2.print("R\r");
  delay(100);
  sensorstring_ETS = Serial2.readStringUntil(TERMINATE_KEY);
  Serial2.readString();  //flush input buffer
  return sensorstring_ETS;
}

// Median-setting Functions for Atlas Scientific Sensors
void setMedian_ETS() {
  Serial.println("getting EC, TDS, and Salinity");
  for (int i = 0; i < 9; i++) {
    sensorstring_ETS = readETS();
    sensorstring_ETS.toCharArray(sensorstring_array, 30);   //convert the string to a char array
    EC = strtok(sensorstring_array, ",");               //let's pars the array at each comma
    TDS = strtok(NULL, ",");                            //let's pars the array at each comma
    SAL = strtok(NULL, ",");                            //let's pars the array at each comma
    EC_array[i] = atol(EC);
    TDS_array[i] = atol(TDS);
    SAL_array[i] = atol(SAL);
    Serial.print(i);
    Serial.print(" ");
    Serial.println(sensorstring_ETS);
  }

  qsort(EC_array, 9, sizeof(EC_array[0]), sort_desc);
  qsort(TDS_array, 9, sizeof(TDS_array[0]), sort_desc);
  qsort(SAL_array, 9, sizeof(SAL_array[0]), sort_desc);

  ECMedian_string = String(EC_array[4]);
  TDSMedian_string = String(TDS_array[4]);
  SALMedian_string = String(SAL_array[4]);
}

void setMedian_pH() {
  Serial.println("getting pH");
  for (int i = 0; i < 9; i++) {
    pH_array[i] = readPH();
    Serial.print(i);
    Serial.print(" ");
    Serial.println(pH_array[i]);
  }
  qsort(pH_array, 9, sizeof(pH_array[0]), sort_desc);
  pHMedian_string = String(pH_array[4], 2);
}

void setMedian_Temp() {
  Serial.println("getting temperature");
  for (int i = 0; i < 9; i++) {
    temp_array[i] = readTemp();
    Serial.print(i);
    Serial.print(" ");
    Serial.println(temp_array[i]);
  }
  qsort(temp_array, 9, sizeof(pH_array[0]), sort_desc);
  tempMedian_string = String(temp_array[4], 2);
}

// gets the depth measured by sensor
float getDepth() {
  //Serial.println(analogRead(SWL_ADC_PIN));
  return float(analogRead(SWL_ADC_PIN) - ADC_ZERO_DEPTH) / DEPTH_SENSOR_SLOPE;
}

// gets the surface water level of the well
String getSWL() {
  SWL = String(DTG - getDepth());
  return SWL;
}

// gets the battery voltage
String getVoltage() {
  //voltage = String (map (analogRead(BATT_ADC_PIN), 0, 1023, 10, 15));
  //voltage = String((analogRead(BATT_ADC_PIN) * BATT_VOLTAGE_VALUE) / BATT_RAW_VALUE);
  voltage = String((analogRead(BATT_ADC_PIN) * BATT_SLOPE) + BATT_INTERCEPT);
  return voltage;
}

// Calibration Prompt
void calibrationPrompt() {

  calibrationMainMenuHelp();
  for (int i = DELAY_FOR_CALMENU - 1; i >= 0; i--)
  {
    Serial.print(i);
    Serial.print("...");

    if (Serial.available() > 0) {
      byte var = Serial.read();    // read the incoming byte:
      if (var == 'Y' || var == 'y') {
        Serial.println();
        Serial.println();
        calibrationLoop();         // load calibration menu

        i = DELAY_FOR_CALMENU;     // reset countdown to defined wait time
        Serial.println();
        calibrationMainMenuHelp(); // display initial instruction
      }
      else if (var == 27) {        // 'ESC' key
        i = 0;                     // skip countdown
      }
    }
    delay(1000);
  }
  Serial.println();

}

void calibrationLoop()
{
  calibrationMenuHelp();
  while (true) {
    if (Serial.available() > 0) {

      byte var = Serial.read();

      if (var == 27)                      // 'ESC' key
        break;
      else if (var == 49) {               // '1' key
        calibrateSensor("pH");
        calibratePH();
        calibrationMenuHelp();
      }
      else if (var == 50) {               // '2' key
        calibrateSensor("Temperature");
        calibrateTemp();
        calibrationMenuHelp();
      }
      else if (var == 51) {               // '3' key
        calibrateSensor("EC");
        calibrateEC();
        calibrationMenuHelp();
      }
      else if (var == 52) {
        Serial.println();
        Serial.println("READ MODE");
        while (Serial.read() != 27)
        {
          String phvalue = (String) readPH();
          String tempvalue = (String) readTemp();
          Serial.println();
          Serial.println("PH:" + phvalue);
          Serial.println("EC,TDS,SAL:" + readETS());
          Serial.println("TEMP:" + tempvalue);
        }
      }
      calibrationMenuHelp();

    }
  }
}

void calibrationMainMenuHelp() {
  Serial.println("GMP Control Box");
  Serial.println("Do you wish to enter calibration mode? Press Y to Enter and ESC to Skip ");
  Serial.println("It will automatically load in " + (String)DELAY_FOR_CALMENU + " seconds");
}

void calibrationMenuHelp() {
  Serial.println("Calibration Mode");
  Serial.println("Press the respective keys to calibrate the respective sensors or ESC to exit");
  Serial.println("1 - pH, 2 - Temperature, 3 - EC");
}

void calibrateSensor(String sensor) {
  Serial.println();
  Serial.println("Calibrating " + sensor + " Sensor...");
}

// Sensor Specific Calibration Process
void calibratePH() {
  Serial1.print("cal,clear\r");
  delay(100);

  Serial.println ("Place pH probe in pH 7 solution. Press ENTER to start calibrating");
  dispResultPH();
  Serial1.print("cal,mid,7.00\r");
  confirmPH();

  Serial.println ("Place pH probe in pH 4 solution. Press ENTER to start calibrating");
  dispResultPH();
  Serial1.print("cal,low,4.00\r");
  confirmPH();

  Serial.println ("Place pH probe in pH 10 solution. Press ENTER to start calibrating");
  dispResultPH();
  Serial1.print("cal,high,10.00\r");
  confirmPH();

  while (Serial1.available() > 0)
  {
    Serial1.read();
  }
  delay(500);
}

void calibrateEC()
{
  Serial2.print("cal,clear\r");
  delay(100);

  Serial.println ("Place EC probe in dry area. Press ENTER to start calibrating");
  dispResultEC();
  Serial2.print("cal,dry\r");
  confirmEC();

  Serial.println ("Place EC probe in 1,413 uS solution. Press ENTER to start calibrating");
  dispResultEC();
  Serial2.print("cal,low,1413\r");
  confirmEC();

  Serial.println ("Place EC probe in 12,880 uS solution. Press ENTER to start calibrating");
  dispResultEC();
  Serial2.print("cal,high,12880\r");
  confirmEC();

  while (Serial2.available() > 0)
  {
    Serial2.read();
  }
  delay(500);
}

void calibrateTemp()
{
  Serial3.print("cal,clear\r");
  delay(100);

  Serial.println("Enter temperature of the environment where probe is placed");
  String tempPt = Serial.readStringUntil(TERMINATE_KEY);
  Serial3.print("cal," + tempPt + "\r");
  
  /*
  Serial.println ("Place Temperature Probe in Room Temperature - 27 C. Press ENTER to start calibrating");
  dispResultTemp();
  Serial3.print("cal,mid,27\r");
  confirmTemp();

  Serial.println ("Place Temperature Probe on Ice - 0 C. Press ENTER to start calibrating");
  dispResultTemp();
  Serial3.print("cal,low,0\r");
  confirmTemp();

  Serial.println ("Place Temperature Probe in Hot Water - 50 C. Press ENTER to start calibrating");
  dispResultTemp();
  Serial3.print("cal,high,50\r");
  confirmTemp();
``*/

  while (Serial3.available() > 0)
  {
    Serial3.read();
  }
  delay(500);
}

// Result Display Functions for Atlas Scientific Sensors
void dispResultPH() {
  Serial.println ("When values have stabilized, Press ENTER again to confirm calibration");
  while (Serial.read() != TERMINATE_KEY)
  {
    Serial1.read();
  }
  delay(100);

  //Data Display
  while (Serial.read() != TERMINATE_KEY)
  {
    float PHVal = readPH();
    Serial.println(PHVal);
  }
}

void dispResultEC() {
  Serial.println ("When values have stabilized, Press ENTER again to confirm calibration");
  while (Serial.read() != TERMINATE_KEY)
  {
    Serial2.read();
  }
  delay(100);

  //Data Display
  while (Serial.read() != TERMINATE_KEY)
  {
    Serial2.print("R\r");
    delay(500);
    if (Serial2.available() > 0) {                       //if the hardware serial port_1 receives a char
      String etsVal = readETS();
      char sensorstring_array[30];                        //we make a char array
      char *EC;                                           //char pointer used in string parsing
      char *TDS;                                          //char pointer used in string parsing
      char *SAL;                                          //char pointer used in string parsing
      
      etsVal.toCharArray(sensorstring_array, 30);         //convert the string to a char array
      EC = strtok(sensorstring_array, ",");               //let's pars the array at each comma
      TDS = strtok(NULL, ",");                            //let's pars the array at each comma
      SAL = strtok(NULL, ",");                            //let's pars the array at each comma
      
      Serial.print("EC:");                                //we now print each value we parsed separately
      Serial.println(EC);                                 //this is the EC value

      Serial.print("TDS:");                               //we now print each value we parsed separately
      Serial.println(TDS);                                //this is the TDS value

      Serial.print("SAL:");                               //we now print each value we parsed separately
      Serial.println(SAL);                                //this is the salinity value
    }
  }
}

void dispResultTemp() {
  Serial.println ("When values have stabilized, Press ENTER again to confirm calibration");
  while (Serial.read() != TERMINATE_KEY)
  {
    Serial3.read();
  }
  delay(100);

  //Data Display
  while (Serial.read() != TERMINATE_KEY)
  {
    float tempVal = readTemp();
    Serial.println(tempVal);
  }
}

// Confirmation of Calibration Functions
void confirmPH() {
  delay(1000);
  Serial1.flush();

  while (Serial1.available() > 0) {                    //if the hardware serial port_1 receives a char
    sensorstring1 = Serial1.readStringUntil(13);       //read the string until we see a <CR>
    Serial.println(sensorstring1);
    sensorstring1 = "";
    delay(200);
  }
  Serial.println();
}

void confirmEC() {
  delay(1000);
  Serial2.flush();

  while (Serial2.available() > 0) {                    //if the hardware serial port_1 receives a char
    sensorstring2 = Serial2.readStringUntil(13);       //read the string until we see a <CR>
    Serial.println(sensorstring2);
    sensorstring2 = "";
    delay(200);
  }
  Serial.println();
}

void confirmTemp() {
  delay(1000);
  Serial3.flush();

  while (Serial3.available() > 0) {                    //if the hardware serial port_1 receives a char
    sensorstring3 = Serial3.readStringUntil(13);       //read the string until we see a <CR>
    Serial.println(sensorstring3);
    sensorstring3 = "";
    delay(200);
  }
  Serial.println();
}

/** Main Functions **/
void setup() {
  Serial.begin(DEBUG_BAUDRATE);

  // set serial for PH
  Serial1.begin(ATLAS_BAUDRATE);
  sensorstring_PH.reserve(ATLAS_RESERVE_BYTES);

  // set serial for EC
  Serial2.begin(ATLAS_BAUDRATE);
  sensorstring_ETS.reserve(ATLAS_RESERVE_BYTES);

  // set serial for Temperature
  Serial3.begin(ATLAS_BAUDRATE);
  sensorstring_Temp.reserve(ATLAS_RESERVE_BYTES);

  setupPHProbe();
  setupECProbe();
  setupTempProbe();

  calibrationPrompt();

  Serial.println();
  Serial.println("Starting up...");

  if (!SD.begin(CSPIN)) {
    Serial.println("SD init failed");
    //while(1);
  } else {
    Serial.println("SD initialized... ");
  }

}

void loop() {

  //Serial.println(analogRead(SWL_ADC_PIN));
  setMedian_Temp();
  setMedian_pH();
  setMedian_ETS();

  Serial.println("Temp: " + tempMedian_string);
  Serial.println("PH: " + pHMedian_string);
  Serial.println("EC: " + ECMedian_string);
  Serial.println("TDS: " + TDSMedian_string);
  Serial.println("SAL: " + SALMedian_string);
  Serial.println("SWL: " + getSWL());
  Serial.println("BattLvl: " + getVoltage());
  Serial.println("Timestamp: " + currentTimestamp);

  Serial.flush();
  delay(100);

  myFile = SD.open(FILENAME, FILE_WRITE);

  Serial.println("Please check if GPRS is on...");
  GPRSSoftPower();

  GPRSSerial.begin(GPRS_BAUDRATE);
  GPRSSerial.print(GSM_TEXTMODE);

  delay(120000);

  setCurrentTimestamp();

  if (myFile) {
    myFile.println(pHMedian_string + ", " + ECMedian_string + ", " + tempMedian_string + ", " + getSWL() + ", " + TDSMedian_string + ", " + SALMedian_string + ", " + getVoltage() + ", " + currentTimestamp);
    myFile.close();
  }
  
  SubmitHttpRequest(pHMedian_string, ECMedian_string, TDSMedian_string, SALMedian_string, tempMedian_string, getSWL(), getVoltage(), APIkey);

  delay(DELAY_BETWEEN_READ);
  GPRSSoftPower();

}
