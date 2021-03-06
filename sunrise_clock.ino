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
unsigned long previousMillis = 0;
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
int onvaspar = 0;
int sunled[] = {12, 13, 14}; // les pins pour les lumieres (use mosfet)
int etat[] = {0, 0, 0, 0, 0, 0, 0}; // confir pour savoir quel etat est la led
int allumeH[] = {9, 6, 6, 6, 6, 6, 9}; // confir pour savoir quel heur allumer
int allumeM[] = {30, 30, 0, 0, 0, 0, 30}; // confir pour savoir quel minute allumer
int eteintH[] = {21, 21, 21, 21, 21, 23, 22}; // confir pour savoir quel heur eteindre
int eteintM[] = {0, 0, 0, 0, 0, 0, 0}; // confir pour savoir quel minute eteindre
int dureF = 45; // confir pour savoir duree total du fade  (fade duration)
unsigned int jourdesemaine = 0;
int rtcerr = 0;

RTClib RTC;
DS3231 Clock;
bool Century = false;
bool h12;
bool PM;
byte ADay, AHour, AMinute, ASecond, ABits;
bool ADy, A12h, Apm;
unsigned int manageur = 0;
String liens = "";

// stylesheet for web pages
String css;

void lecss() {
  css = "<style>\n"
        ".heure,.minute {width:30px;}"
        "input[name=\"ah0\"],input[name=\"am0\"],input[name=\"eh0\"],input[name=\"em0\"] {background-color:#AAFFEE;}"
        "input[name=\"ah6\"],input[name=\"am6\"],input[name=\"eh6\"],input[name=\"em6\"] {background-color:#AAFFEE;}"
        "input[name=\"ah";
  css += jourdesemaine;
  css += "\"],input[name=\"am";
  css += jourdesemaine;
  css += "\"],input[name=\"eh";
  css += jourdesemaine;
  css += "\"],input[name=\"em";
  css += jourdesemaine;
  css += "\"] {background-color:";
  if (etat[jourdesemaine] == 0) {
    css += "#FFCCCC;}";
  } else {
    css += "#CCCCFF;}";
  }
  css += "</style>\n";
  // end of stylesheet for web pages
}

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
  int DST = 0;
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
  if ((month > 3) && (month < 11)){
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
  DateTime now = RTC.now();
  int heureux = now.hour();
  int dayofweek = dayOfWeek(now.unixtime()); // de 1 a 7
  int i = dayofweek - 1;
  int lamin = now.minute();
  int lanee = now.year();
  int lemois = now.month();
  int lejour = now.day();
  if (heureux == 165) {
    Serial.println("RTC ERREUR !!!! Using NTP if possible");
    int epoch = timeClient.getEpochTime();
    heureux = hour(epoch);
    dayofweek = dayOfWeek(epoch); // de 1 a 7
    i = dayofweek - 1;
    jourdesemaine = i;
    lamin = minute(epoch);
    lanee = year(epoch);
    lemois = month(epoch);
    lejour = day(epoch);
  }
  String contenu = "<!DOCTYPE html>\n<html lang=\"en\" dir=\"ltr\" class=\"client-nojs\">\n<head>\n";
  contenu += "<meta charset=\"UTF-8\" />\n<title>Lampe Soleil</title>\n"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  contenu += css;
  contenu += "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/2.1.1/jquery.min.js\"></script>\n";
  contenu += "</head>\n<body>\n"
             "<div style=\"text-align:center;width:100%;\">\n"
             "<h1>";
  contenu += lejour;
  contenu += "/";
  contenu += lemois;
  contenu += "/";
  contenu += lanee;
  contenu += " - ";
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
    contenu += "<input class=\"heure\" type=\"number\" min=\"0\" max=\"23\" value=\"";
    contenu += allumeH[i];
    contenu += "\" name=\"ah";
    contenu += i;
    contenu += "\">\n";
    if (i != 6) {
      contenu += "&nbsp;";
    }
  }
  contenu += "<br>\n"
             "M:";
  for (int i = 0; i < 7; i = i + 1) {
    contenu += "<input class=\"minute\" type=\"number\" min=\"0\" max=\"59\" value=\"";
    contenu += allumeM[i];
    contenu += "\" name=\"am";
    contenu += i;
    contenu += "\">\n";
    if (i != 6) {
      contenu += "&nbsp;";
    }
  }
  contenu += "<br>\n"
             "<h2>Eteignage :</h2>\n"
             "H:";
  for (int i = 0; i < 7; i = i + 1) {
    contenu += "<input class=\"heure\" type=\"number\" min=\"0\" max=\"23\" value=\"";
    contenu += eteintH[i];
    contenu += "\" name=\"eh";
    contenu += i;
    contenu += "\">\n";
    if (i != 6) {
      contenu += "&nbsp;";
    }
  }
  contenu += "<br>\n"
             "M:";
  for (int i = 0; i < 7; i = i + 1) {
    contenu += "<input class=\"minute\" type=\"number\" min=\"0\" max=\"59\" value=\"";
    contenu += eteintM[i];
    contenu += "\" name=\"em";
    contenu += i;
    contenu += "\">\n";
    if (i != 6) {
      contenu += "&nbsp;";
    }
  }
  contenu += "<br><h3>Dur&eacute;e du fade:</h3><input class=\"minute\" type=\"number\" min=\"15\" max=\"59\" value=\"";
  contenu += dureF;
  contenu += "\" name=\"dure\">\n";
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
  String dur = server.arg("dure");
  if (dur != "") {
    dureF = dur.toInt();
    Serial.print("dure");
    Serial.print(" : ");
    Serial.println(dureF);
    preferences.putInt("duref", dureF);
  }
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
  onvaspar = 1;
}

void allumex() {
  r = 4095;
  g = 4095;
  b = 4095;
  unsigned long dureB = dureF / 3 * 60000 / 4095;
  unsigned long dureG = dureF / 2 * 60000 / 4095;
  unsigned long dureR = dureF * 60000 / 4095;
  if (dureF == 0) {
    dureB = 1;
    dureG = 1;
    dureR = 1;
  }
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
  onvaspar = 0;
}

void eteintx() {
  r = 0;
  g = 0;
  b = 0;
  unsigned long dureB = dureF / 3 * 60000 / 4095;
  unsigned long dureG = dureF / 2 * 60000 / 4095;
  unsigned long dureR = dureF * 60000 / 4095;
  if (dureF == 0) {
    dureB = 1;
    dureG = 1;
    dureR = 1;
  }
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
  DateTime now = RTC.now();
  int heureux = now.hour();
  int dayofweek = dayOfWeek(now.unixtime()); // de 1 a 7
  int i = dayofweek - 1;
  int lamin = now.minute();
  if (heureux == 165) {
    Serial.println("RTC ERREUR !!!! Using NTP if possible");
    rtcerr++;
    if (rtcerr == 10) {
      Serial.println("10 erreures on reset le RTC");
      rtcerr = 0;
      resetrtc();
    }
    int epoch = timeClient.getEpochTime();
    heureux = hour(epoch);
    dayofweek = dayOfWeek(epoch); // de 1 a 7
    i = dayofweek - 1;
    lamin = minute(epoch);
  }
  jourdesemaine = i;
  lecss();
  if (allumeH[i] < eteintH[i]) {
    if (heureux >= allumeH[i] && heureux < eteintH[i] ) { //Allume H ok
      if (heureux == allumeH[i]) {
        if (lamin >= allumeM[i]) {
          allume(); //Allume la lumiere
          etat[i] = 1;
        } else {
          eteint(); //Eteint la lumiere
          etat[i] = 0;
        }
      } else {
        allume(); //Allume la lumiere
        etat[i] = 1;
      }
    } else if (heureux >= eteintH[i]) {
      if (heureux == eteintH[i]) {
        if (lamin >= eteintM[i]) {
          eteint(); // Eteint la lumiere a reviser
          etat[i] = 0;
        } else {
          allume(); //Allume la lumiere
          etat[i] = 1;
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
          allume(); //Allume la lumiere
          etat[i] = 1;
        } else {
          eteint(); //Eteint la lumiere
          etat[i] = 0;
        }
      } else {
        if (heureux > eteintH[i]) {
          allume(); //Allume la lumiere
          etat[i] = 1;
        } else {
          allume(); //Allume la lumiere
          etat[i] = 1;
        }
      }
    }
    else if (heureux < eteintH[i]) {
      allume(); //Allume la lumiere
      etat[i] = 1;
    }
    else if (heureux >= eteintH[i] && heureux < allumeH[i]) {
      if (heureux == eteintH[i]) {
        if (lamin >= eteintM[i]) {
          eteint(); //Eteint la lumiere
          etat[i] = 0;
        } else {
          allume(); //Allume la lumiere
          etat[i] = 1;
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
          allume(); //Allume la lumiere
          etat[i] = 1;
        }
      } else {
        eteint(); //Eteint la lumiere
        etat[i] = 0;
      }
    }
    if (allumeM[i] > eteintM[i]) {
      if (lamin < eteintM[i]) {
        allume(); //Allume la lumiere
        etat[i] = 1;
      } else {
        if (lamin >= allumeM[i]) {
          allume(); //Allume la lumiere
          etat[i] = 1;
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
}

void loop1(void *pvParameters) {
  while (1) {
    if (configur == 1) {
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= 2000) {
        previousMillis = currentMillis;
        lumiereloop();
      }
      if (onvaspar == 1) {
        allumex();
      } else {
        eteintx();
      }
    }
    vTaskDelay( 156 ); // wait / yield time to other tasks
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {
  manageur = 1;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(sunled[0], OUTPUT);
  pinMode(sunled[1], OUTPUT);
  pinMode(sunled[2], OUTPUT);
  digitalWrite(sunled[0], LOW);
  digitalWrite(sunled[1], LOW);
  digitalWrite(sunled[2], LOW);
  ledcAttachPin(sunled[0], 1);
  ledcAttachPin(sunled[1], 2);
  ledcAttachPin(sunled[2], 3);
  ledcSetup(1, 12000, 12); // 12 kHz PWM, 12-bit resolution
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
  dureF = preferences.getInt("duref", dureF);
  configur = preferences.getInt("configur", 0);
  Serial.println("Pref loaded!");
  xTaskCreatePinnedToCore(loop1, "loop1", 8192, NULL, 2, NULL, 0);
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
  lecss();
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
    int dayofweek = Clock.getDoW();// de 1 a 7
    int jj = dayofweek - 1;
    jourdesemaine = jj;
    lecss();
    int lamin = Clock.getMinute();
    int lanee = Clock.getYear();
    int lemois = Clock.getMonth(Century);
    int lejour = Clock.getDate();
    if (heureEte(lanee, lemois, lejour, heureux)) {
      Serial.println("heure d'ete");
      ntpheure = ntpheure + 1;
    }
    if ( dayofweek != dayOfWeek(epoch)) {
      Clock.setDoW(dayOfWeek(epoch));
      jourdesemaine = dayOfWeek(epoch) - 1;
      lecss();
      Serial.print("Jour de la semaine : ");
      Serial.println(dayOfWeek(epoch));
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

void resetrtc() {
  Wire.reset();
  Clock.setClockMode(false);
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
      int dayofweek = Clock.getDoW();// de 1 a 7
      jourdesemaine = dayofweek - 1;
      int lamin = Clock.getMinute();
      int lanee = Clock.getYear();
      int lemois = Clock.getMonth(Century);
      int lejour = Clock.getDate();
      if (lejour == 165) {
        rtcerr++;
        if (rtcerr == 10) {
          Serial.println("10 erreures on reset le RTC");
          rtcerr = 0;
          resetrtc();
        }
      }
      if (heureEte(lanee, lemois, lejour, heureux)) {
        Serial.println("heure d'ete");
        ntpheure = ntpheure + 1;
      }
      if ( dayofweek != dayOfWeek(epoch)) {
        Clock.setDoW(dayOfWeek(epoch));
        jourdesemaine = dayOfWeek(epoch) - 1;
        Serial.print("Jour de la semaine : ");
        Serial.println(dayOfWeek(epoch));
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
