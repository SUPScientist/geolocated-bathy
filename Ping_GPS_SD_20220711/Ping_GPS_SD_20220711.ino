/*
 Ping Sonar (Blue Robotics) reader
 
 Modified by Phil Bresnahan to include datalogging and GPS
 UNCW

 GPS and SD code, as well as modification to enable a second hardware serial line, comes from Adafruit

 TODO: add altitude to saved params
*/


#include <SPI.h>
#include <SD.h>
#include "wiring_private.h" // pinPeripheral() function and Uart Serial2 (renamed to pingSerial here)
#include "ping1d.h"

// *********************** SD card settings ********************
#define chipSelect 4 
File myFile;
char filename[] = "YYMMDD00.csv"; // template filename (year, month, day, 00–99 file number for that day)
bool ledState = false;
bool filenameCreated = false;


// ************************ GPS ********************************
#include <Adafruit_GPS.h>
#define GPSSerial Serial1
Adafruit_GPS GPS(&GPSSerial);
uint32_t timer = millis();


// ************************ Ping Sonar *************************
// Add a second hardware UART
// https://learn.adafruit.com/using-atsamd21-sercom-to-add-more-spi-i2c-serial-ports/creating-a-new-serial
Uart pingSerial (&sercom1, 11, 10, SERCOM_RX_PAD_0, UART_TX_PAD_2);
static Ping1D ping { pingSerial };
void SERCOM1_Handler()
{
  pingSerial.IrqHandler();
}


void setup() {
  Serial.begin(9600); //Opens the main serial port to communicate with the computer
  pingSerial.begin(9600);

  // For Ping Sonar
  // Assign pins 10 & 11 SERCOM functionality for 2nd UART
  pinPeripheral(10, PIO_SERCOM);
  pinPeripheral(11, PIO_SERCOM);
  
  Serial.println("Blue Robotics ping1d-simple.ino");
  
  while (!ping.initialize()) {
      Serial.println("\nPing device failed to initialize!");
      delay(2000);
  }
  pinMode(LED_BUILTIN, OUTPUT);
  
  delay(3000); // give serial monitor time to open and establish comms.
  Serial.println(" Demo of AN-126 Software Serial and K-40 Sensor");


  
  // Set up GPS
  GPS.begin(9600);
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For parsing data, we don't suggest using anything but either RMC only or RMC+GGA since
  // the parser doesn't care about other sentences at this time
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // 1 Hz update rate
  // For the parsing code to work nicely and have time to sort thru the data, and
  // print it out we don't suggest using anything higher than 1 Hz
  
  // Start SD stuff
  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    while (1);
  }
  Serial.println("card initialized.");
  
}

void loop() {
  // read data from the GPS in the 'main loop'
  char c = GPS.read();
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences!
    // so be very wary if using OUTPUT_ALLDATA and trying to print out data
    if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
      return; // we can fail to parse a sentence in which case we should just wait for another
  }

  // approximately every 2 seconds or so, print out the current stats
  if (millis() - timer > 2000) {
    timer = millis(); // reset the timer

    Serial.print("\nTime: ");
    if (GPS.hour < 10) { Serial.print('0'); }
    Serial.print(GPS.hour, DEC); Serial.print(':');
    if (GPS.minute < 10) { Serial.print('0'); }
    Serial.print(GPS.minute, DEC); Serial.print(':');
    if (GPS.seconds < 10) { Serial.print('0'); }
    Serial.print(GPS.seconds, DEC); Serial.print('.');
    if (GPS.milliseconds < 10) {
      Serial.print("00");
    } else if (GPS.milliseconds > 9 && GPS.milliseconds < 100) {
      Serial.print("0");
    }
    Serial.println(GPS.milliseconds);
    Serial.print("Date: ");
    Serial.print(GPS.month, DEC); Serial.print('/');
    Serial.print(GPS.day, DEC); 
    Serial.print("/20");
    Serial.println(GPS.year, DEC);
    Serial.print("Fix: "); Serial.print((int)GPS.fix);
    Serial.print(" quality: "); Serial.println((int)GPS.fixquality);

    // If GPS gets a fix, print out and save good data
    if (GPS.fix) {
      // Blink to let us know you're alive
      ledState = !ledState;
      digitalWrite(LED_BUILTIN, ledState);   // turn the LED on (HIGH is the voltage level)
      
      Serial.print("Location: ");
      Serial.print(GPS.latitude, 4); Serial.print(GPS.lat);
      Serial.print(", ");
      Serial.print(GPS.longitude, 4); Serial.println(GPS.lon);
      Serial.print("Speed (knots): "); Serial.println(GPS.speed);
      Serial.print("Angle: "); Serial.println(GPS.angle);
      Serial.print("Altitude: "); Serial.println(GPS.altitude);
      Serial.print("Satellites: "); Serial.println((int)GPS.satellites);

      if(!filenameCreated){
        // Get year, month, and day for filename
        int filenum = 0; // start at zero and increment by one if file exists
        sprintf(filename, "%02d%02d%02d%02d.csv", GPS.year, GPS.month, GPS.day, filenum); 
        
        // Check for existence of filename with current filenum
        while (SD.exists(filename)) {
          filenum++;
          sprintf(filename, "%02d%02d%02d%02d.csv", GPS.year, GPS.month, GPS.day, filenum); 
        }
        filenameCreated = true;
      }
      Serial.println(filename);

    Serial.print(", seconds elapsed = ");
    Serial.println(millis()/1000);
    
    // Create filename
    // Open the file: SPI SD comms
    File dataFile = SD.open(filename, FILE_WRITE);
      
    // if the file is available, write to it:
    if (dataFile) {
      // Date
      dataFile.print(GPS.month, DEC); 
      dataFile.print('/');
      dataFile.print(GPS.day, DEC); 
      dataFile.print("/20");
      dataFile.print(GPS.year, DEC);
      dataFile.print(",");

      // Time
      dataFile.print(GPS.hour, DEC); 
      dataFile.print(':');
      if (GPS.minute < 10) { dataFile.print('0'); }
      dataFile.print(GPS.minute, DEC); dataFile.print(':');
      if (GPS.seconds < 10) { dataFile.print('0'); }
      dataFile.print(GPS.seconds, DEC);
      dataFile.print(",");

      // Elapsed Time
      dataFile.print(millis()/1000);
      dataFile.print(",");

      // Location
      dataFile.print(GPS.latitude, 4); 
      dataFile.print(",");
      dataFile.print(GPS.lat); // N or S
      dataFile.print(",");
      dataFile.print(GPS.longitude, 4); 
      dataFile.print(",");
      dataFile.print(GPS.lon); // E or W

      dataFile.print(",");

      // Ping sonar
      if (ping.update()) {
        dataFile.print(ping.distance());
        dataFile.print(",");
        dataFile.println(ping.confidence());
      } 
      else {
        dataFile.println(",");
      }

      dataFile.close();
    }
    // if the file isn't open, pop up an error:
    else {
      Serial.println("error opening datalog.txt");
    }
    }

  }

  

}
