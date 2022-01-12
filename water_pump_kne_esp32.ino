#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <FS.h>
#include <HTTPClient.h>
#include <WiFiMulti.h>         // Built-in

#include <string>

bool debugMode = true;
//Setup
float sensorD = 320.47; //sensor ke dasar
float sensorN = 190.47; //sensor ke nol blok
float minimum = -90;
float maximum = -70;
float benteng = -115;
//float calIn = 70;
float calIn = 120;
float calOut = 80;
//int idws = 1;

const int arIn = 10;
const int arOut = 10;
float avgInAr[arIn], avgOutAr[arOut] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
float avgIn, avgOut = 0;
int idws = 77;
//int log_time_unit  = 400;  // default is 1-minute between readings, 10=15secs 40=1min 200=5mins 400=10mins 2400=1hr
int log_time_unit  = 139;  // default is 1-minute between readings, 10=15secs 40=1min 200=5mins 400=10mins 2400=1hr

//declare ultrasonic [trig, echo]
const int ultrasonic1[2] = {5, 18};
const int ultrasonic2[2] = {2, 4};

int iteration = 0;

//const int waterPump[6] = {LED_BUILTIN, 13, 12, 14, 27, 26};
const int waterPump[4] = {0, 1, 2, 3};
const int gensetOn = 4; //relay 4
const int gensetStart = 5; //relay 5
bool starterGenset = false;

unsigned long siklusPompa = 5000;
//unsigned long siklusPompa = 7200000;
bool triggerPompa = true;
unsigned long timerPompa;
unsigned long pengali = 0;

//setting id water level dan php
String serverPath = "http://srs-ssms.com/post-wl-data-test.php";
//String serverPath = "http://srs-ssms.com/post-wl-data.php";

//define sound speed in cm/uS
#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701

//pengukuran ultrasonic
float distanceCmIn[10], distanceCmOut[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
String distanceStrIn, distanceStrOut = "";
float distanceNowIn, distanceNowOut = 0;

//password wifi1 default
//const char* ssid = "asdf";
//const char* password = "asdfasdf";
const char* ssid = "wifi";
const char* password = "wifi2021";

String ssidStr, pwdStr; //password wifi2 dari SPIFF

AsyncWebServer server(80); //inisialisasi webserver

WiFiMulti wifiMulti; //inisialisasi wifi

//Delete line di TXT
#define isSpace(x) (x == ' ')
#define isComma(x) (x == ',')
template<class T> inline Print &operator <<(Print &str, T arg)
{
  str.print(arg);
  return str;
}

struct LinesAndPositions
{
  int NumberOfLines; // number of lines in file
  int SOL[50]; // start of line in file
  int EOL[50]; // end of line in file
};

//Jam
#include "RTClib.h"
RTC_DS3231 rtc;
String timeArr[10] = {"null", "null", "null", "null", "null", "null", "null", "null", "null", "null"};
String timeStr, timeNow = "";
//interrupt
int timer_cnt, log_interval, log_count;


void setup() {
  delay(1000); //delay nunggu wifi nyala


  //set awal interrupt
  log_count = 0;
  log_interval = log_time_unit * 10;
  timer_cnt    = log_interval + 1;

  Serial.begin(115200); // Starts the serial communication
  Serial2.begin(115200); // Starts the serial communication

  //set pin ultrasonic
  pinMode(ultrasonic1[0], OUTPUT); // Sets the trigPin as an Output
  pinMode(ultrasonic1[1], INPUT); // Sets the echoPin as an Input
  pinMode(ultrasonic2[0], OUTPUT); // Sets the trigPin as an Output
  pinMode(ultrasonic2[1], INPUT); // Sets the echoPin as an Input

  debug("inisalisasi ultrasonik");

  //set awal jam
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  debug("inisalisasi RTC");

  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  debug("inisalisasi SPIFF");

  // Initialize ssid wifi
  ssidStr = readLine(SPIFFS, "/wifi.txt");
  pwdStr = readLine(SPIFFS, "/pass.txt");
  debug("inisalisasi password");

  // Connect to Wi-Fi
  connectToWifi();
  debug("inisalisasi konek wifi");

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html");
  });
  server.on("/code.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/code.min.js", "text/javascript");
  });
  server.on("/distancein", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceNowIn).c_str());
  });
  server.on("/distanceout", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceNowOut).c_str());
  });
  server.on("/timenow", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", timeNow.c_str());
  });
  server.on("/distancein0", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmIn[0]).c_str());
  });
  server.on("/distancein1", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmIn[1]).c_str());
  });
  server.on("/distancein2", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmIn[2]).c_str());
  });
  server.on("/distancein3", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmIn[3]).c_str());
  });
  server.on("/distancein4", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmIn[4]).c_str());
  });
  server.on("/distancein5", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmIn[5]).c_str());
  });
  server.on("/distancein6", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmIn[6]).c_str());
  });
  server.on("/distancein7", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmIn[7]).c_str());
  });
  server.on("/distancein8", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmIn[8]).c_str());
  });
  server.on("/distancein9", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmIn[9]).c_str());
  });
  server.on("/distanceout0", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmOut[0]).c_str());
  });
  server.on("/distanceout1", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmOut[1]).c_str());
  });
  server.on("/distanceout2", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmOut[2]).c_str());
  });
  server.on("/distanceout3", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmOut[3]).c_str());
  });
  server.on("/distanceout4", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmOut[4]).c_str());
  });
  server.on("/distanceout5", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmOut[5]).c_str());
  });
  server.on("/distanceout6", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmOut[6]).c_str());
  });
  server.on("/distanceout7", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmOut[7]).c_str());
  });
  server.on("/distanceout8", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmOut[8]).c_str());
  });
  server.on("/distanceout9", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(distanceCmOut[9]).c_str());
  });
  server.on("/time0", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(timeArr[0]).c_str());
  });
  server.on("/time1", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(timeArr[1]).c_str());
  });
  server.on("/time2", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(timeArr[2]).c_str());
  });
  server.on("/time3", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(timeArr[3]).c_str());
  });
  server.on("/time4", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(timeArr[4]).c_str());
  });
  server.on("/time5", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(timeArr[5]).c_str());
  });
  server.on("/time6", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(timeArr[6]).c_str());
  });
  server.on("/time7", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(timeArr[7]).c_str());
  });
  server.on("/time8", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(timeArr[8]).c_str());
  });
  server.on("/time9", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(timeArr[9]).c_str());
  });

  // Start server
  server.begin();

  debug("inisalisasi server");

}

void loop() {

  //kirim data aktual ke webserver
  distanceNowIn = getDistance(ultrasonic1, calIn);
  distanceNowOut = getDistance(ultrasonic2, calOut);
  Serial.println("DistanceIn (cm): " + String(distanceNowIn));
  Serial.println("DistanceOut (cm) : " + String(distanceNowOut));
  debug("ukur realtime ultrasonik");

  avgInFn(distanceNowIn);
  avgOutFn(distanceNowOut);
//  debug(String(avgInAr[0]) + ", " + String(avgInAr[1]) + ", " + String(avgInAr[2]) + ", " + String(avgInAr[3]) + ", " + String(avgInAr[4]) + ", " +
//  String(avgInAr[5]) + ", " + String(avgInAr[6]) + ", " + String(avgInAr[7]) + ", " + String(avgInAr[8]) + ", " + String(avgInAr[9]) + ", " );
//    
  debug("Avg in : " + String(avgIn));
  debug("Avg out : " + String(avgOut));
  timeNow = getTime();
  debug("dapatkan waktu");

  //implementasi interrupt
  if (timeNow != "1970-1-1 00:00:00" and timer_cnt >= log_interval) {
    timer_cnt = 0;  // log_interval values are 10=15secs 40=1min 200=5mins 400=10mins 2400=1hr
    log_count += 1; // Increase loggin event count
    debug("interrupt");

    logging();
    debug("logging data");

    //if buka tutup
    if (distanceNowIn > maximum) {
      Serial.println("pompa nyala");
      //nyala
      if (triggerPompa) {
        timerPompa = millis();
        triggerPompa = false;
        debug("trigger pompa jadi false");
      }
      long currentMillis = millis();
      if ((timerPompa + pengali) < millis()) {
        debug("bandingkan waktu");
        switch (iteration) {
          //            for (i = 1; i <= 5; i++) {
          //              if (i != 5) {
          //              case i:
          //                relay(i, i - 1);
          //                break;
          //              } else {
          //                relay(1, 0);
          //                break;
          //              }
          //            }
          case 0:
            relayTrig(gensetOn, true);
            delay(10000);
            if (!starterGenset) {
              relayTrig(gensetStart, true);
              delay(1000);
              relayTrig(gensetStart, false);
              starterGenset = true;
            }
            relay(1, 0);
            debug("iteration 0");
            break;
          case 1: relay(2, 1);               debug("iteration 1"); break;
          case 2: relay(3, 2);               debug("iteration 2"); break;
          case 3: relay(0, 3); triggerPompa = true;                debug("iteration 3"); break;
            //          case 4: relay(5, 4); break;
            //          case 5: relay(0, 5); triggerPompa = true; break;
        }
      }
    } else {
      for (int i = 0; i < sizeof(waterPump); i++) {
        relayTrig(waterPump[i], false);
        delay(5000);
      }
      starterGenset = false;
      relayTrig(gensetOn, false);
      iteration = 0;
      //mati
      Serial.println("pompa mati");
    }
  }
  timer_cnt += 1; // Readings set by value of log_interval each 40 = 1min

  //periksa ada data di dalam txt atau ga

  String payload = readLine(SPIFFS, "/log.txt");
  if (payload != "") {
    int code = kirimDataKeServer(payload);
    if (code == 1) {
      Serial.println("Sukses");
      DeleteLineFromFile("/log.txt", 1);
    } else {
      Serial.println("Gagal");
    }
  }

  delay(500);
}

bool cekPompa(long mil, int times) {
  bool cek = false;
  cek = (timerPompa + (siklusPompa * times)) < mil  & iteration == times;
  return cek;
}

void avgInFn(float nilaiInBaru){
  float totalIn = 0;
  for (int i=arIn-1;i>0;i--){
    avgInAr[i] = avgInAr[i-1];
    totalIn += avgInAr[i];
  }
  avgInAr[0] = nilaiInBaru;
  totalIn += nilaiInBaru;
  avgIn = totalIn/arIn;
}

void avgOutFn(float nilaiOutBaru){
  float totalOut = 0;
  for (int i=arOut-1;i>0;i--){
    avgOutAr[i] = avgOutAr[i-1];
    totalOut += avgOutAr[i];
  }
  avgOutAr[0] = nilaiOutBaru;
  totalOut += nilaiOutBaru;
  avgOut = totalOut/arOut;
}

void relayTrig(int pin, bool kondisi) {
  if (kondisi) {
    Serial.println("Trigger nyala pin=" + String(pin));
    Serial2.println(pin);
  } else {
    int pinMati = pin * -1;
    Serial.println("Trigger mati pin=" + String(pinMati));
    Serial2.println(pinMati);
  }
}

void debug(String noDebug) {
  if (debugMode) {
    Serial.println("debug " + noDebug);
  }
}


void relay(int iterationIn, int waterPumpOff) {
  iteration = iterationIn;
  waterPumpExc(waterPumpOff);
  pengali = (iteration + 1) * siklusPompa;
}

void waterPumpExc(int off) {
  //    for (int i = 0; i < sizeof(waterPump); i++) {
  //      if (i == off) {
  //        digitalWrite(waterPump[i], LOW);
  //        continue;
  //      }
  //      digitalWrite(waterPump[i], HIGH);
  //    }
  //  digitalWrite(waterPump[0], HIGH);
  //  digitalWrite(waterPump[1], HIGH);
  //  digitalWrite(waterPump[2], HIGH);
  //  digitalWrite(waterPump[3], HIGH);
  //  digitalWrite(waterPump[4], HIGH);
  //  digitalWrite(waterPump[5], HIGH);
  //  digitalWrite(waterPump[off], LOW);

  switch (off) {

    case 0: relayTrig(waterPump[0], false); delay(2000); relayTrig(waterPump[1], true); delay(2000); relayTrig(waterPump[2], true); delay(2000); relayTrig(waterPump[3], true); break;
    case 1: relayTrig(waterPump[0], true); relayTrig(waterPump[1], false); relayTrig(waterPump[2], true); relayTrig(waterPump[3], true); break;
    case 2: relayTrig(waterPump[0], true); relayTrig(waterPump[1], true); relayTrig(waterPump[2], false); relayTrig(waterPump[3], true); break;
    case 3: relayTrig(waterPump[0], true); relayTrig(waterPump[1], true); relayTrig(waterPump[2], true); relayTrig(waterPump[3], false); break;

      //    case 0: digitalWrite(waterPump[0], LOW); digitalWrite(waterPump[1], HIGH); digitalWrite(waterPump[2], HIGH); digitalWrite(waterPump[3], HIGH); digitalWrite(waterPump[4], HIGH); digitalWrite(waterPump[5], HIGH); break;
      //    case 1: digitalWrite(waterPump[0], HIGH); digitalWrite(waterPump[1], LOW); digitalWrite(waterPump[2], HIGH); digitalWrite(waterPump[3], HIGH); digitalWrite(waterPump[4], HIGH); digitalWrite(waterPump[5], HIGH); break;
      //    case 2: digitalWrite(waterPump[0], HIGH); digitalWrite(waterPump[1], HIGH); digitalWrite(waterPump[2], LOW); digitalWrite(waterPump[3], HIGH); digitalWrite(waterPump[4], HIGH); digitalWrite(waterPump[5], HIGH); break;
      //    case 3: digitalWrite(waterPump[0], HIGH); digitalWrite(waterPump[1], HIGH); digitalWrite(waterPump[2], HIGH); digitalWrite(waterPump[3], LOW); digitalWrite(waterPump[4], HIGH); digitalWrite(waterPump[5], HIGH); break;
      //    case 4: digitalWrite(waterPump[0], HIGH); digitalWrite(waterPump[1], HIGH); digitalWrite(waterPump[2], HIGH); digitalWrite(waterPump[3], HIGH); digitalWrite(waterPump[4], LOW); digitalWrite(waterPump[5], HIGH); break;
      //    case 5: digitalWrite(waterPump[0], HIGH); digitalWrite(waterPump[1], HIGH); digitalWrite(waterPump[2], HIGH); digitalWrite(waterPump[3], HIGH); digitalWrite(waterPump[4], HIGH); digitalWrite(waterPump[5], LOW); triggerPompa = true; break;
  }
}


void connectToWifi() {

  if (wifiMulti.run() != WL_CONNECTED) {

    //mengubah ssid dan pwd dari string ke char array
    int p = ssidStr.length();
    char ssidArr[p + 1];
    strcpy(ssidArr, ssidStr.c_str());
    int l = pwdStr.length();
    char pwdArr[l + 1];
    strcpy(pwdArr, pwdStr.c_str());

    wifiMulti.addAP(ssid, password);  // add Wi-Fi networks you want to connect to, it connects strongest to weakest
    wifiMulti.addAP(ssidArr, pwdArr);  // Adjust the values in the Network tab
    Serial.println("Connecting ...");
    int i = 0;
    while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
      delay(250);
      Serial.print('.');
      i++;
      if (i > 15) {
        Serial.println("");
        break;
      }
    }
    if (wifiMulti.run() == WL_CONNECTED) {
      // Print ESP32 Local IP Address
      Serial.println(WiFi.localIP());
    }
  }
}

float getDistance(const int * ultrasonic, float cal) {
  float dt = 0;
  dt = (getDistanceAct(ultrasonic, cal) - sensorN) * -1;
  return dt;
}

float getDistanceAct(const int * ultrasonic, float cal) {
  long duration;
  float dt = 0;
  // Clears the trigPin
  digitalWrite(ultrasonic[0], LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(ultrasonic[0], HIGH);
  delayMicroseconds(10);
  digitalWrite(ultrasonic[0], LOW);
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(ultrasonic[1], HIGH);
  duration * SOUND_SPEED / 2;

  dt = duration * SOUND_SPEED / 2;
  dt += cal;
  return dt;
}

String getTime() {
  DateTime now = rtc.now();
  String timeS = String(now.year(), DEC) + "-" + String(now.month(), DEC) + "-" + String(now.day(), DEC) + " " + String(now.hour(), DEC) + ":" + String(now.minute(), DEC) + ":" + String(now.second(), DEC);
  return timeS;
}

//log data ke dalam SPIFFS
void logging() {

  //geser array nilai aktual
  for (int j = 9; j > 0; j--) {
    distanceCmIn[j] = distanceCmIn[j - 1];
    distanceCmOut[j] = distanceCmOut[j - 1];
    timeArr[j] = timeArr[j -  1];
  }
  distanceCmIn[0] = getDistance(ultrasonic1, calIn);
  distanceCmOut[0] = getDistance(ultrasonic2, calOut);
  timeArr[0] = getTime();

  //geser array nilai string
  distanceStrIn = "[";
  distanceStrOut = "[";
  timeStr = "[";
  for (int i = 0; i < 10; i++) {
    distanceStrIn += distanceCmIn[i];
    distanceStrOut += distanceCmOut[i];
    timeStr += timeArr[i];
    if (i < 9) {
      distanceStrIn += ", ";
      distanceStrOut += ", ";
      timeStr += ",";
    }
  }
  distanceStrIn += "]";
  distanceStrOut += "]";
  timeStr += "]";

  // Prints the distance in the Serial Monitor
  Serial.println("DistanceIn (cm): " + String(distanceCmIn[0]));
  Serial.println("Distance ArrayIn: " + String(distanceStrIn));
  Serial.println("DistanceOut (cm): " + String(distanceCmOut[0]));
  Serial.println("Distance ArrayOut: " + String(distanceStrOut));
  Serial.println("Time: " + String(timeArr[0]));
  Serial.println("Time Array: " + String(timeStr));


  if (wifiMulti.run() == WL_CONNECT_FAILED) {
    connectToWifi();
  }
  
  //  String payload = "lvl_in=" + String(distanceCmIn[0]) + "&lvl_out=" + String(distanceCmOut[0]) + "&d=" + String(timeArr[0]);
  String payload = "lvl_in=" + String(avgIn) + "&lvl_out=" + String(avgOut) + "&d=" + String(timeArr[0]);
  int code = kirimDataKeServer(payload);
  if (code == 1) {

    Serial.println("Sukses");
  } else {
    Serial.println("Gagal");
    payload += "\n";
    int n = payload.length();
    char payArr[n + 1];
    strcpy(payArr, payload.c_str());
    appendFile(SPIFFS, "/log.txt",  payArr);
    readFile(SPIFFS, "/log.txt");
  }
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

String readLine(fs::FS &fs, const char * path) {
  String line = "";
  Serial.printf("Reading line: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return  line;
  }

  Serial.println("- read from line:");
  line = file.readStringUntil('\n');
  Serial.println(line);

  file.close();
  return line;
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\r\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("- failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("- message appended");
  } else {
    Serial.println("- append failed");
  }
  file.close();
}

void deleteFile(fs::FS &fs, const char * path) {
  Serial.printf("Deleting file: %s\r\n", path);
  if (fs.remove(path)) {
    Serial.println("- file deleted");
  } else {
    Serial.println("- delete failed");
  }
}

struct LinesAndPositions FindLinesAndPositions(char* filename)
{
  File myFile;
  LinesAndPositions LNP;

  myFile = SPIFFS.open(filename);
  if (myFile)
  {
    LNP.NumberOfLines = 0;
    LNP.SOL[0] = 0; //the very first start-of-line index is always zero
    int i = 0;

    while (myFile.available())
    {
      if (myFile.read() == '\n') // read until the newline character has been found
      {
        LNP.EOL[LNP.NumberOfLines] = i; // record the location of where it is in the file
        LNP.NumberOfLines++; // update the number of lines found
        LNP.SOL[LNP.NumberOfLines] = i + 1; // the start-of-line is always 1 character more than the end-of-line location
      }
      i++;
    }
    LNP.EOL[LNP.NumberOfLines] = i; // record the last locations
    LNP.NumberOfLines += 1; // record the last line

    myFile.close();
  }

  return LNP;
}

void CopyFiles(char* ToFile, char* FromFile)
{
  File myFileOrig;
  File myFileTemp;

  File file = SPIFFS.open(ToFile);

  if (!file || file.isDirectory()) {
    SPIFFS.remove(ToFile);
  }

  myFileTemp = SPIFFS.open(ToFile, FILE_WRITE);
  myFileOrig = SPIFFS.open(FromFile);
  if (myFileOrig)
  {
    while (myFileOrig.available())
    {
      myFileTemp.write( myFileOrig.read() ); // make a complete copy of the original file
    }
    myFileOrig.close();
    myFileTemp.close();
    //Serial.println("done.");
  }
}

void DeleteLineFromFile(char * filename, int Line)
{
  DeleteMultipleLinesFromFile(filename, Line, Line);
}

void DeleteSelectedLinesFromFile(char * filename, char * StrOfLines)
{
  byte offset = 0;
  Serial << "Deleting multiple lines, please wait";
  for (unsigned short i = 0, j = strlen(StrOfLines), index = 0; i <= j; i++)
  {
    char C = (*StrOfLines++);
    if (isComma(C) || (i == j))
    {
      DeleteLineFromFile(filename, index - offset);
      offset++;
      index = 0;
    }
    else if (isSpace(C)) continue;
    else
      index = (index * 10) + C - '0';
    if ((i % 2) == 0)
      Serial << ".";
  }
  Serial.println();
}

void DeleteMultipleLinesFromFile(char * filename, int SLine, int ELine)
{
  File myFileOrig;
  File myFileTemp;

  // If by some chance it exists, remove the temp file from the card
  File file = SPIFFS.open("/tempFile.txt");
  if (!file || file.isDirectory()) {
    SPIFFS.remove("/tempFile.txt");
  }

  // Get the start and end of line positions from said file
  LinesAndPositions FileLines = FindLinesAndPositions(filename);

  if ((SLine > FileLines.NumberOfLines) || (ELine > FileLines.NumberOfLines)) {
    return;
  }

  myFileTemp = SPIFFS.open("/tempFile.txt", FILE_WRITE);
  myFileOrig = SPIFFS.open(filename);
  int position = 0;

  if (myFileOrig)
  {
    while (myFileOrig.available())
    {
      char C = myFileOrig.read();

      // Copy the file but exclude the entered lines by user
      if ((position < FileLines.SOL[SLine - 1]) || (position > FileLines.EOL[ELine - 1]))
        myFileTemp.write(C);

      position++;
    }
    myFileOrig.close();
    myFileTemp.close();
  }

  //copy the contents of tempFile back to the original file
  CopyFiles(filename, "/tempFile.txt");

  // Remove the tempfile from the FS card
  File fl = SPIFFS.open("/tempFile.txt");
  if (!fl || fl.isDirectory()) {
    SPIFFS.remove("/tempFile.txt");
  }
}

int kirimDataKeServer(String postRequest)
{
  digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
  int success = 0;
  WiFiClient client;
  HTTPClient http; //Declare object of class HTTPClient
  Serial.println("send begin..");
  http.begin(client, serverPath);
  debug("http begin");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  debug("masukkan header");
  String isi = postRequest + "&idwl=" + idws;
  debug("isi:" + isi);
  int httpCode = http.POST(isi); //Send the request
  debug("ekseskusi post");
  String payload = http.getString();     //Get the response payload
  debug("ambil payload");
  Serial.println(String(httpCode) + " - " + payload); //Print HTTP return code
  http.end();
  Serial.println("http code:" + String(httpCode));
  if (httpCode == 200 )
    success = 1;
  else
    success = 0;
  Serial.println("send end..");
  digitalWrite(LED_BUILTIN, LOW); // turn the LED off by making the voltage LOW
  return success;
}
