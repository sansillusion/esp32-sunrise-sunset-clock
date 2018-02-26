/*
    A sunrise/sunset IoT RGB led strip controler by Steve Olmstead 2018
    7 day timer setup trough web interface at http://soleil.local
*/
#include <TimeLib.h>
#include <NTPClient.h>
#include <time.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <DS3231.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Preferences.h>
#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif
unsigned long previousMillisc = 0;
unsigned long previousMillist = 0;
unsigned long previousMillisfr = 0;
unsigned long previousMillisfg = 0;
unsigned long previousMillisfb = 0;
int configur = 0; // config pour savoir si first run
Preferences preferences;
WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.nrc.ca", -18000, 86400000);// timezone -5h et update aux 24h
long r = 0;
long g = 0;
long b = 0;
int rouge = 0;
int vert = 0;
int bleu = 0;

int growled[] = {12, 13, 14}; // les pins pour les lumieres (use mosfet)
int passer[] = {0, 0, 0, 0, 0, 0, 0}; // confir pour savoir quel est desactivee
int passerx[] = {0, 0, 0, 0, 0, 0, 0}; // confir pour savoir quel est desactivee par temperature
int etat[] = {0, 0, 0, 0, 0, 0, 0}; // confir pour savoir quel etat est la led
int allumeH[] = {9, 9, 6, 6, 6, 6, 6}; // confir pour savoir quel heur allumer
int allumeM[] = {30, 30, 0, 0, 0, 0, 0}; // confir pour savoir quel minute allumer
int eteintH[] = {22, 21, 21, 21, 21, 21, 23}; // confir pour savoir quel heur eteindre
int eteintM[] = {0, 30, 0, 0, 0, 0, 0}; // confir pour savoir quel minute eteindre
int dureF = 45; // confir pour savoir duree total du fade  (fade duration)

DS3231 Clock;
bool Century = false;
bool h12;
bool PM;
byte ADay, AHour, AMinute, ASecond, ABits;
bool ADy, A12h, Apm;
unsigned int manageur = 0;
String liens = "";

// stylesheet for web pages
const String css = "<style>\n"
                   ".heure,.minute {width:35px;}"
                   "input[name=\"ah0\"],input[name=\"am0\"],input[name=\"eh0\"],input[name=\"em0\"] {background-color:#AAFFEE;}"
                   "input[name=\"ah1\"],input[name=\"am1\"],input[name=\"eh1\"],input[name=\"em1\"] {background-color:#AAFFEE;}"
                   "</style>\n";
// end of stylesheet for web pages

void lesliens() {
  liens = "<a href=\"/\">Acceuil</a>";
}

void fadeR() {
  if (rouge < r) {
    rouge++;
  } else if (rouge > r) {
    rouge--;
  }
  ledcWrite(1, rouge);
}

// used to fade green
void fadeG() {
  if (vert < g) {
    vert++;
  } else if (vert > g) {
    vert--;
  }
  ledcWrite(2, vert);
}

// used to fade blue
void fadeB() {
  if (bleu < b) {
    bleu++;
  } else if (bleu > b) {
    bleu--;
  }
  ledcWrite(3, bleu);
}

char* string2char(String command) {
  if (command.length() != 0) {
    char *p = const_cast<char*>(command.c_str());
    return p;
  }
  return 0;
}

int heureEte(int year, int month, int dayOfMonth, int hour) {
  int DST;
  // ********************* Calculate offset for Sunday *********************
  int y = year;                          // DS3231 uses two digit year (required here)
  int x = (y + y / 4 + 2) % 7;    // remainder will identify which day of month
  // is Sunday by subtracting x from the one
  // or two week window.  First two weeks for March
  // and first week for November
  // *********** Test DST: BEGINS on 2nd Sunday of March @ 2:00 AM *********
  if (month == 3 && dayOfMonth == (14 - x) && hour >= 2)
  {
    DST = 1;                           // Daylight Savings Time is TRUE (add one hour)
  }
  if ((month == 3 && dayOfMonth > (14 - x)) || month > 3)
  {
    DST = 1;
  }
  // ************* Test DST: ENDS on 1st Sunday of Nov @ 2:00 AM ************
  if (month == 11 && dayOfMonth == (7 - x) && hour >= 2)
  {
    DST = 0;                            // daylight savings time is FALSE (Standard time)
  }
  if ((month == 11 && dayOfMonth > (7 - x)) || month > 11 || month < 3)
  {
    DST = 0;
  }
  return DST;
}

// timer page
void handleTimer() {
  int heureux = Clock.getHour(h12, PM);
  if (h12 == 1) { //check si le rtc est en mode 12h
    if (PM == 1) { //si oui et que c'est pm
      heureux = heureux  + 12; // ajoute 12h
    }
  }
  int lamin = Clock.getMinute();
  String contenu = "<!DOCTYPE html>\n<html lang=\"en\" dir=\"ltr\" class=\"client-nojs\">\n<head>\n";
  contenu += "<meta charset=\"UTF-8\" />\n<title>Timer 420</title>\n"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  contenu += css;
  contenu += "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/2.1.1/jquery.min.js\"></script>\n";
  contenu += "</head>\n<body>\n"
             "<div style=\"text-align:center;width:100%;\">\n"
             "<h1>";
  contenu += heureux;
  contenu += ":";
  if (lamin <= 9) {
    contenu += "0";
  }
  contenu += lamin;
  contenu += "</h1>\n"
             "<form>\n"
             "<h2>Allumage :</h2>\n"
             "H:";
  for (int i = 0; i < 7; i = i + 1) {
    if (passer[i] == 0) {
      contenu += "<input class=\"heure\" type=\"number\" min=\"0\" max=\"23\" value=\"";
      contenu += allumeH[i];
      contenu += "\" name=\"ah";
      contenu += i;
      contenu += "\">\n";
      if (i != 6) {
        contenu += " - ";
      }
    }
  }
  contenu += "<br>\n"
             "M:";
  for (int i = 0; i < 7; i = i + 1) {
    if (passer[i] == 0) {
      contenu += "<input class=\"minute\" type=\"number\" min=\"0\" max=\"59\" value=\"";
      contenu += allumeM[i];
      contenu += "\" name=\"am";
      contenu += i;
      contenu += "\">\n";
      if (i != 6) {
        contenu += " - ";
      }
    }
  }
  contenu += "<br>\n"
             "<h2>Eteignage :</h2>\n"
             "H:";
  for (int i = 0; i < 7; i = i + 1) {
    if (passer[i] == 0) {
      contenu += "<input class=\"heure\" type=\"number\" min=\"0\" max=\"23\" value=\"";
      contenu += eteintH[i];
      contenu += "\" name=\"eh";
      contenu += i;
      contenu += "\">\n";
      if (i != 6) {
        contenu += " - ";
      }
    }
  }
  contenu += "<br>\n"
             "M:";
  for (int i = 0; i < 7; i = i + 1) {
    if (passer[i] == 0) {
      contenu += "<input class=\"minute\" type=\"number\" min=\"0\" max=\"59\" value=\"";
      contenu += eteintM[i];
      contenu += "\" name=\"em";
      contenu += i;
      contenu += "\">\n";
      if (i != 6) {
        contenu += " - ";
      }
    }
  }
  contenu += "</form><br>\n";
  contenu += liens;
  contenu += "</div>\n"
             "<script>"
             "var timerh;"
             "var timerm;"
             "$(\".heure\").change(function(event){\n"
             "event.preventDefault();\n"
             "clearTimeout(timerh);"
             "var lenom = $(this).attr('name');\n"
             "var laval = $(this).val();\n"
             "if (laval >= 24){\n"
             "laval = 23;\n"
             "$(this).val(laval);\n"
             "alert(\"max 23\");\n"
             "}\n"
             "timerh = setTimeout(function(){"
             "$.get( \"/changer?\"+lenom+\"=\"+laval);"
             "},500);"
             "});"
             "$(\".minute\").change(function(event){\n"
             "event.preventDefault();\n"
             "clearTimeout(timerm);"
             "if ($(this).val() >> 59){\n"
             "$(this).val(59);\n"
             "}\n"
             "var lenom = $(this).attr('name');\n"
             "var laval = $(this).val();\n"
             "if (laval >= 60){\n"
             "laval = 59;\n"
             "$(this).val(laval);\n"
             "alert(\"max 59\");\n"
             "}\n"
             "timerm = setTimeout(function(){"
             "$.get( \"/changer?\"+lenom+\"=\"+laval);"
             "},500);"
             "});\n"
             "</script>\n"
             "</body></html>\n";
  server.send(200, "text/html", contenu);
}

void handleChange() {
  String addy = server.client().remoteIP().toString();
  Serial.println("");
  Serial.println(addy);
  Serial.println("Change");
  for (int i = 0; i < 7; i = i + 1) {
    String llah = "ah";
    llah += i;
    String llam = "am";
    llam += i;
    String lleh = "eh";
    lleh += i;
    String llem = "em";
    llem += i;
    String lah = server.arg(llah);
    String lam = server.arg(llam);
    String leh = server.arg(lleh);
    String lem = server.arg(llem);
    if (lah != "") {
      allumeH[i] = lah.toInt();
      Serial.print("ah");
      Serial.print(i);
      Serial.print(" :");
      Serial.println(lah);
      String ahs = "ah";
      ahs += i;
      int iah = allumeH[i];
      preferences.putInt(string2char(ahs), iah);
    }
    if (lam != "") {
      allumeM[i] = lam.toInt();
      Serial.print("am");
      Serial.print(i);
      Serial.print(" :");
      Serial.println(lam);
      String ams = "am";
      ams += i;
      int iam = allumeM[i];
      preferences.putInt(string2char(ams), iam);
    }
    if (leh != "") {
      eteintH[i] = leh.toInt();
      Serial.print("eh");
      Serial.print(i);
      Serial.print(" :");
      Serial.println(leh);
      int ieh = eteintH[i];
      String ehs = "eh";
      ehs += i;
      preferences.putInt(string2char(ehs), ieh);
    }
    if (lem != "") {
      eteintM[i] = lem.toInt();
      Serial.print("em");
      Serial.print(i);
      Serial.print(" :");
      Serial.println(lem);
      String ems = "em";
      ems += i;
      int iem = eteintM[i];
      preferences.putInt(string2char(ems), iem);
    }
  }
  String contenu = "<!DOCTYPE html>\n<html lang=\"en\" dir=\"ltr\" class=\"client-nojs\">\n<head>\n";
  contenu += "<meta http-equiv=\"refresh\" content=\"0;url=/\" />\n";
  contenu += "<meta charset=\"UTF-8\" />\n<title>Que la lumiere soit</title>\n"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  contenu += css;
  contenu += "</head>\n<body>\n"
             "Merci</body></html>\n";
  server.send(200, "text/html", contenu);
}

void handleNotFound() {
  String htmlmessage = "<!DOCTYPE html>\n<html lang=\"en\" dir=\"ltr\" class=\"client-nojs\">\n<head>\n";
  htmlmessage += "<meta charset=\"UTF-8\" />\n<title>404 Not found</title>\n"
                 "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  htmlmessage += css;
  htmlmessage += "</head>\n<body>\n"
                 "<div style=\"text-align:center;width:100%;\">\n";
  htmlmessage += "<h1>404 File Not Found</h1><br>\n";
  htmlmessage += "URI: ";
  htmlmessage += server.uri();
  htmlmessage += "<br>\nMethod: ";
  htmlmessage += (server.method() == HTTP_GET) ? "GET" : "POST";
  htmlmessage += "<br>\nArguments: ";
  htmlmessage += server.args();
  htmlmessage += "<br>\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    htmlmessage += " " + server.argName(i) + ": " + server.arg(i) + "<br>\n";
  }
  htmlmessage += "<br>\n";
  htmlmessage += liens;
  htmlmessage += "<br>\n</div></body></html>\n";
  server.send(404, "text/html", htmlmessage);
}

void allume() {
  r = 4095;
  g = 4095;
  b = 4095;
  unsigned long dureB = dureF / 3 * 60000 / 4095;
  unsigned long dureG = dureF / 2 * 60000 / 4095;
  unsigned long dureR = dureF * 60000 / 4095;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisfr >= dureR) {
    previousMillisfr = currentMillis;
    fadeR();
  }
  if (currentMillis - previousMillisfg >= dureG) {
    previousMillisfg = currentMillis;
    fadeG();
  }
  if (currentMillis - previousMillisfb >= dureB) {
    previousMillisfb = currentMillis;
    fadeB();
  }
}

void eteint() {
  r = 0;
  g = 0;
  b = 0;
  unsigned long dureB = dureF / 3 * 60000 / 4095;
  unsigned long dureG = dureF / 2 * 60000 / 4095;
  unsigned long dureR = dureF * 60000 / 4095;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisfr >= dureR) {
    previousMillisfr = currentMillis;
    fadeR();
  }
  if (currentMillis - previousMillisfg >= dureG) {
    previousMillisfg = currentMillis;
    fadeG();
  }
  if (currentMillis - previousMillisfb >= dureB) {
    previousMillisfb = currentMillis;
    fadeB();
  }
}

void lumiereloop() {
  int heureux = Clock.getHour(h12, PM);
  int dayofweek = Clock.getDoW();// de 1 a 7
  int i = dayofweek - 1;
  if (h12 == 1) { //check si le rtc est en mode 12h
    if (PM == 1) { //si oui et que c'est pm
      heureux = heureux  + 12; // ajoute 12h
    }
  }
  int lamin = Clock.getMinute();
  float tempC;
  if (passer[i] == 0) {
    if (allumeH[i] < eteintH[i]) {
      if (heureux >= allumeH[i] && heureux < eteintH[i] ) { //Allume H ok
        if (heureux == allumeH[i]) {
          if (lamin >= allumeM[i]) {
            if (passer[i] == 0) {
              allume(); //Allume la lumiere
              etat[i] = 1;
            }
          } else {
            eteint(); //Eteint la lumiere
            etat[i] = 0;
          }
        } else {
          if (passer[i] == 0) {
            allume(); //Allume la lumiere
            etat[i] = 1;
          }
        }
      } else if (heureux >= eteintH[i]) {
        if (heureux == eteintH[i]) {
          if (lamin >= eteintM[i]) {
            eteint(); // Eteint la lumiere a reviser
            etat[i] = 0;
          } else {
            if (passer[i] == 0) {
              allume(); //Allume la lumiere
              etat[i] = 1;
            }
          }
        } else {
          eteint(); // Eteint la lumiere a reviser
          etat[i] = 0;
        }
      } else {
        eteint(); // Eteint la lumiere
        etat[i] = 0;
      }
    }
    if (allumeH[i] > eteintH[i]) {
      if (heureux >= allumeH[i] && heureux <= 23) {                //Start
        if (heureux == allumeH[i]) {
          if (lamin >= allumeM[i]) {
            if (passer[i] == 0) {
              allume(); //Allume la lumiere
              etat[i] = 1;
            }
          } else {
            eteint(); //Eteint la lumiere
            etat[i] = 0;
          }
        } else {
          if (heureux > eteintH[i]) {
            if (passer[i] == 0) {
              allume(); //Allume la lumiere
              etat[i] = 1;
            }
          } else {
            if (passer[i] == 0) {
              allume(); //Allume la lumiere
              etat[i] = 1;
            }
          }
        }
      }
      else if (heureux < eteintH[i]) {
        if (passer[i] == 0) {
          allume(); //Allume la lumiere
          etat[i] = 1;
        }
      }
      else if (heureux >= eteintH[i] && heureux < allumeH[i]) {
        if (heureux == eteintH[i]) {
          if (lamin >= eteintM[i]) {
            eteint(); //Eteint la lumiere
            etat[i] = 0;
          } else {
            if (passer[i] == 0) {
              allume(); //Allume la lumiere
              etat[i] = 1;
            }
          }
        } else {
          eteint(); //Eteint la lumiere
          etat[i] = 0;
        }
      }
    }
    if (allumeH[i] == eteintH[i]) {
      if (allumeM[i] < eteintM[i]) {
        if (lamin < eteintM[i]) {
          if (lamin >= allumeM[i]) {
            if (passer[i] == 0) {
              allume(); //Allume la lumiere
              etat[i] = 1;
            }
          }
        } else {
          eteint(); //Eteint la lumiere
          etat[i] = 0;
        }
      }
      if (allumeM[i] > eteintM[i]) {
        if (lamin < eteintM[i]) {
          if (passer[i] == 0) {
            allume(); //Allume la lumiere
            etat[i] = 1;
          }
        } else {
          if (lamin >= allumeM[i]) {
            if (passer[i] == 0) {
              allume(); //Allume la lumiere
              etat[i] = 1;
            }
          } else {
            eteint(); //Eteint la lumiere
            etat[i] = 0;
          }
        }
      }
      if (allumeM[i] == eteintM[i]) { // le meme heure d'allumage et d'eteignage == eteint permanent
        eteint(); //Eteint la lumiere
        etat[i] = 0;
      }
    }
  } else {
    eteint(); //Eteint la lumiere car elle est desactivee
    etat[i] = 0;
  }
}

void loop1(void *pvParameters) {
  while (1) {
    if (configur == 1) {
      lumiereloop();
    }
    vTaskDelay( 128 ); // wait / yield time to other tasks
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {
  manageur = 1;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  ledcAttachPin(growled[0], 1);
  ledcAttachPin(growled[1], 2);
  ledcAttachPin(growled[2], 3);
  pinMode(growled[0], OUTPUT);
  pinMode(growled[1], OUTPUT);
  pinMode(growled[2], OUTPUT);
  digitalWrite(growled[0], LOW);
  digitalWrite(growled[1], LOW);
  digitalWrite(growled[2], LOW);
  ledcSetup(1, 12000, 12); // 12 kHz PWM, 8-bit resolution
  ledcSetup(2, 12000, 12);
  ledcSetup(3, 12000, 12);
  preferences.begin("soleil", false);
  for (int i = 0; i < 7; i = i + 1) {
    String ahs = "ah";
    ahs += i;
    String ams = "am";
    ams += i;
    String ehs = "eh";
    ehs += i;
    String ems = "em";
    ems += i;
    int iah = allumeH[i];
    int iam = allumeM[i];
    int ieh = eteintH[i];
    int iem = eteintM[i];
    int ahi = preferences.getInt(string2char(ahs), iah);
    int ami = preferences.getInt(string2char(ams), iam);
    int ehi = preferences.getInt(string2char(ehs), ieh);
    int emi = preferences.getInt(string2char(ems), iem);
    allumeH[i] = ahi;
    allumeM[i] = ami;
    eteintH[i] = ehi;
    eteintM[i] = emi;
  }
  configur = preferences.getInt("configur", 0);
  Serial.println("Pref loaded!");
  xTaskCreatePinnedToCore(loop1, "loop1", 8192, NULL, 1, NULL, 0);
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback); //cree un callback pour savoir si passer par wifimanager
  wifiManager.setTimeout(240);
  if (!wifiManager.autoConnect("soleil")) {
    delay(3000);
    ESP.restart();
    delay(5000);
  }
  delay(500);
  if (manageur == 1) { // reboot si nouvelle config de wifimanager pour eviter bug de webserver qui roule pas
    ESP.restart();
    delay(5000);
  }
  MDNS.begin("soleil");// use preferance pour le nom
  lesliens();
  if (configur == 0) {
    preferences.putInt("configur", 1);
    configur = 1;
  }
  server.on("/", handleTimer);
  server.on("/changer", handleChange);
  server.onNotFound(handleNotFound);
  server.begin();
  MDNS.addService("_http", "_tcp", 80);
  timeClient.begin();
  delay(500);
  Clock.setClockMode(false);
  if (timeClient.update()) {
    int ntpheure = timeClient.getHours();
    int ntpmins = timeClient.getMinutes();
    int heureux = Clock.getHour(h12, PM);
    int epoch = timeClient.getEpochTime();
    if (h12 == 1) { //check si le rtc est en mode 12h
      if (PM == 1) { //si oui et que c'est pm
        heureux = heureux + 12; // ajoute 12h
      }
    }
    int lamin = Clock.getMinute();
    int lanee = Clock.getYear();
    int lemois = Clock.getMonth(Century);
    int lejour = Clock.getDate();
    if (heureEte(lanee, lemois, lejour, heureux)) {
      Serial.println("heure d'ete");
      ntpheure = ntpheure + 1;
    }
    if ( lanee != year(epoch) % 100) {
      Clock.setYear(year(epoch) % 100);
      Serial.print("Anée : ");
      Serial.println(year(epoch) % 100);
    }
    if ( lemois != month(epoch)) {
      Clock.setMonth(month(epoch));
      Serial.print("Mois : ");
      Serial.println(month(epoch));
    }
    if ( lejour != day(epoch)) {
      Clock.setDate(day(epoch));
      Serial.print("Jour : ");
      Serial.println(day(epoch));
    }
    if (heureux != ntpheure) {
      Clock.setHour(ntpheure);
      Serial.print("Heures : ");
      Serial.println(ntpheure);
    }
    if (lamin != ntpmins) {
      Clock.setMinute(ntpmins);
      Serial.print("Minutes : ");
      Serial.println(ntpmins);
    }
  }
}

void loop() {
  server.handleClient();
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisc >= 10000) {
    previousMillisc = currentMillis;
    int status =  WiFi.status();
    if (status != 3) {
      WiFi.reconnect();
      delay(6000);
    }
  }
  if (currentMillis - previousMillist >= 60000) { // met le RTC a jour avec le NTP chaques minutes si changements
    previousMillist = currentMillis;
    if (timeClient.update()) {
      int ntpheure = timeClient.getHours();
      int ntpmins = timeClient.getMinutes();
      int heureux = Clock.getHour(h12, PM);
      int epoch = timeClient.getEpochTime();
      if (h12 == 1) { //check si le rtc est en mode 12h
        if (PM == 1) { //si oui et que c'est pm
          heureux = heureux + 12; // ajoute 12h
        }
      }
      int lamin = Clock.getMinute();
      int lanee = Clock.getYear();
      int lemois = Clock.getMonth(Century);
      int lejour = Clock.getDate();
      if (heureEte(lanee, lemois, lejour, heureux)) {
        Serial.println("heure d'ete");
        ntpheure = ntpheure + 1;
      }
      if ( lanee != year(epoch) % 100) {
        Clock.setYear(year(epoch) % 100);
        Serial.print("Anée : ");
        Serial.println(year(epoch) % 100);
      }
      if ( lemois != month(epoch)) {
        Clock.setMonth(month(epoch));
        Serial.print("Mois : ");
        Serial.println(month(epoch));
      }
      if ( lejour != day(epoch)) {
        Clock.setDate(day(epoch));
        Serial.print("Jour : ");
        Serial.println(day(epoch));
      }
      if (heureux != ntpheure) {
        Clock.setHour(ntpheure);
        Serial.print("Heures : ");
        Serial.println(ntpheure);
      }
      if (lamin != ntpmins) {
        Clock.setMinute(ntpmins);
        Serial.print("Minutes : ");
        Serial.println(ntpmins);
      }
    }
  }
}
