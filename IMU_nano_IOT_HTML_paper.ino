#include <Arduino_LSM6DS3.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include <SD.h>

//////////////////////////////
// Variable Initializations //
//////////////////////////////
char current_position[20] = ""; // Current Sleeping Position Variable
float CounterBack = 0;  // Back Position Counter
float CounterBelly = 0; // Belly Position Counter
float CounterRightSide = 0; // Right Side Position Counter
float CounterLeftSide = 0; // Left Side Position Counter
float CounterStand = 0; // Stand Position Counter
float CounterNA = 0; // Switching Position Counter
float x, y, z; //Holds the Accelorometer Sensor Readings
///////////// /////////
// Threshold Values //
/////////////////////
const float ThresholdBack = 0.30;
const float ThresholdBelly = 0.30;
const float ThresholdSide1 = 0.35;
const float ThresholdSide2 = 0.35;
const float ThresholdStand = 0.2;
///////////////////////
// SD Logging Config //
///////////////////////
#define ENABLE_SD_LOGGING true // Default SD logging (can be changed via serial menu)
#define LOG_FILE_INDEX_MAX 999 // Max number of "logXXX.txt" files
#define LOG_FILE_PREFIX "log"  // Prefix name for log files
#define LOG_FILE_SUFFIX "txt"  // Suffix name for log files
#define SD_MAX_FILE_SIZE 5000000 // 5MB max file size, increment to next file before surpassing
#define SD_LOG_WRITE_BUFFER_SIZE 1024 // Experimentally tested to produce 100Hz logs
#define SD_CHIP_SELECT_PIN 10
/////////////////////
// SD Card Globals //
/////////////////////
bool sdCardPresent = false; // Keeps track of if SD card is plugged in
String logFileName; // Active logging file
String logFileBuffer; // Buffer for logged data. Max is set in config
//////////////////////////
// Network Information //
////////////////////////
char ssid[] = "";  // your network SSID (name)
char pass[] = "";  // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;  // your network key Index number (needed only for WEP)
int status = WL_IDLE_STATUS;
WiFiServer server(80);

void setup() {
  Serial.begin(9600);   //initialize serial

 
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }
  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  server.begin();
  // you're connected now, so print out the status:
  printWifiStatus();

  // Initialize the IMU chip
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }
  // Initialize the SD card logger
  if ( initSD() )
  {
    sdCardPresent = true;
    // Get the next, available log file name
    logFileName = nextLogFile();
  }
}

void loop() {

  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z); //Take the accelorometer readings and store in x, y and z 
    delay(1000); // take a reading every 1 second
  }
  
  // Determine the current sleeping position log
  // check if back
  if (z <= 1.05 && z >= (1.05 - ThresholdBack)) {
    CounterBack = CounterBack + 1;
    strcpy(current_position, "Back");
  }

  //check if belly
  else if (z >= -1.05 && z <= (-1.05 + ThresholdBelly)) {
    CounterBelly = CounterBelly + 1 ;
    strcpy(current_position, "Belly");
  }
  //check if right side
  else if (y <= 1.05 && y >= (1.05 - ThresholdSide1)) {
    CounterRightSide = CounterRightSide + 1;
    strcpy(current_position, "Side1");
  }
  //check if left side
  else if (y >= -1.05 && y <= (-1.05 + ThresholdSide2)) {
    CounterLeftSide = CounterLeftSide + 1;
    strcpy(current_position, "Side2");
  }
  //check if standing or sitting
  else if ((x >= -1.05 && x <= (-1.05 + ThresholdStand)) || (x <= 1.05 && x >= (1.05 - ThresholdStand))) {
    CounterStand = CounterStand + 1;
    strcpy(current_position, "Standing");
  }

  //all others are considered switching position
  else {
    CounterNA = CounterNA + 1;
    strcpy(current_position, "Switching Position");
  }

  //send current results over WIFI 
  // listen for incoming clients
  WiFiClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 10");  // refresh the page automatically every 10 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          //outout the resutls
          client.print("Current position is:  ");
          client.println(current_position);
          client.println("<br />");
          client.println("<br />");
          client.print("Back total sleep time is: ");
          client.print(CounterBack / 1);
          client.print(" s or ");
          client.print(CounterBack / 60, 2);
          client.print(" m or ");
          client.print(CounterBack / 3600, 2);
          client.print(" h");
          client.println("<br />");
          client.print("Belly total sleep time is: ");
          client.print(CounterBelly / 1);
          client.print(" s or ");
          client.print(CounterBelly / 60, 2);
          client.print(" m or ");
          client.print(CounterBelly / 3600, 2);
          client.print(" h");
          client.println("<br />");
          client.print("Side1 total sleep time is: ");
          client.print(CounterRightSide);
          client.print(" s or ");
          client.print(CounterRightSide / 60, 2);
          client.print(" m or ");
          client.print(CounterRightSide / 3600, 2);
          client.print(" h");
          client.println("<br />");
          client.print("Side2 total sleep time is: ");
          client.print(CounterLeftSide);
          client.print(" s or ");
          client.print(CounterLeftSide / 60, 2);
          client.print(" m or ");
          client.print(CounterLeftSide / 3600, 2);
          client.print(" h");
          client.println("<br />");
          client.print("Standing total time is: ");
          client.print(CounterStand);
          client.print(" s or ");
          client.print(CounterStand / 60, 2);
          client.print(" m or ");
          client.print(CounterStand / 3600, 2);
          client.print(" h");
          client.println("<br />");
          client.print("Changing Position total time is: ");
          client.print(CounterNA);
          client.print(" s or ");
          client.print(CounterNA / 60, 2);
          client.print(" m or ");
          client.print(CounterNA / 3600, 2);
          client.print(" h");
          client.println("<br />");
          client.println("<br />");
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  // SD logging of the results as a backup
  }
  if (sdCardPresent)
    logIMUData(); // Log new data
  }


void logIMUData(void)
{
// write the string that will be logged to SD card
  String imuLog = "";
  imuLog += "Current position is:  " + String(current_position) + "\n\n";
  imuLog += "Back total sleep time is: " + String(CounterBack) + " s or " + String(CounterBack / 60) + " m or " + String(CounterBack / 3600) + " h" + "\n\n";
  imuLog += "Belly total sleep time is: " + String(CounterBelly) + " s or " + String(CounterBelly / 60) + " m or " + String(CounterBelly / 3600) + " h" + "\n\n";
  imuLog += "Side1 total sleep time is: " + String(CounterRightSide) + " s or " + String(CounterRightSide / 60) + " m or " + String(CounterRightSide / 3600) + " h" + "\n\n";
  imuLog += "Side2 total sleep time is: " + String(CounterLeftSide) + " s or " + String(CounterLeftSide / 60) + " m or " + String(CounterLeftSide / 3600) + " h" + "\n\n";
  imuLog += "Standing total time is: " + String(CounterStand) + " s or " + String(CounterStand / 60) + " m or " + String(CounterStand / 3600) + " h" + "\n\n";
  imuLog += "Changing Position total time is: " + String(CounterNA) + " s or " + String(CounterNA / 60) + " m or " + String(CounterNA / 3600) + " h" + "\n\n";
  sdLogString(logFileBuffer); // Log SD buffer
  logFileBuffer = ""; // Clear SD log buffer
  // Add new line to SD log buffer
  logFileBuffer += imuLog;
}

// Log a string to the SD card
bool sdLogString(String toLog)
{
  // Open the current file name and replace the old data with new data:
  SD.remove(logFileName);
  File logFile = SD.open(logFileName, FILE_WRITE);

  // If the file will get too big with this new string, create
  // a new one, and open it.
  if (logFile.size() > (SD_MAX_FILE_SIZE - toLog.length()))
  {
    logFileName = nextLogFile();
    logFile = SD.open(logFileName, FILE_WRITE);
  }

  // If the log file opened properly, add the string to it.
  if (logFile)
  {
    logFile.print(toLog);
    logFile.close();
    return true; // Return success
  }

  return false; // Return fail
}

bool initSD(void)
{
  // SD.begin should return true if a valid SD card is present
  if ( !SD.begin(SD_CHIP_SELECT_PIN) )
  {
    return false;
  }

  return true;
}

String nextLogFile(void)
{
  // create a new text file once called and name the file from 1.text to 999.text 
  String filename;
  int logIndex = 0;
  for (int i = 0; i < LOG_FILE_INDEX_MAX; i++)
  {
    // Construct a file with PREFIX[Index].SUFFIX
    filename = String(LOG_FILE_PREFIX);
    filename += String(logIndex);
    filename += ".";
    filename += String(LOG_FILE_SUFFIX);
    // If the file name doesn't exist, return it
    if (!SD.exists(filename))
    {
      return filename;
    }
    // Otherwise increment the index, and try again
    logIndex++;
  }
  return "";
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
