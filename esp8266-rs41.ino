/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 by Valerio Di Giampietro
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <SoftwareSerial.h>
#include "OTAdual.h"
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#define SER2_TX D1
#define SER2_RX D2
#define BUFLEN  1024
//how many clients should be able to telnet to this ESP8266
#define MAX_SRV_CLIENTS 1
#define TCP_PORT (23)           // Choose any port you want

#ifndef min
#define min(x,y)  ((x)<(y)?(x):(y))
#endif

#define TXFREQ_DEFAULT   433.500 // defaul tx freq when no frequency has been saved on EEPROM
#define GETSERIALTIMEOUT 1200    // time used to read from RS41 serial 2000 is OK
#define SLOWTIME_CMD      800    // time to wait for each keypress to enter command mode, 2000 is OK
#define SLOWTIME          300    // time to wait for each keypress to select menu entries and input data 300 is ok
#define TIME2SLEEP        180    // default time from boot to go to deep sleep mode to save power. Wakeup on reset or power on
//#define DEBUG                  // if defined print debugging information on serial port
#define STS_ERROR  99            // status error
#define STS_OK    100            // status OK


// ------ Global variables
WiFiServer       tcpServer(TCP_PORT);
WiFiClient       tcpServerClients[MAX_SRV_CLIENTS];
ESP8266WebServer webServer(80); //Server on port 80
SoftwareSerial   Serial2(SER2_RX, SER2_TX); // RX, TX

char         buf[BUFLEN]; // used to store serial responses from RS-41
unsigned int buftail;     // point to the last char in buf
unsigned int buflastline; // point to the last Carriage Return in buf
unsigned int time2sleep;  // time, in seconds, after witch the ESP8266 will go to sleep to save power
int          status = 0;
float        txfreq = 0;
char         reg75[3],reg76[3],reg77[3];
char         eeprom_ssid[30], eeprom_password[30];

String urlDecode(String sin) {
  String s = sin;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}

inline void bridge_serial() {
  int n0=0;
  int m0=0;;
  int n1=0;
  int m1=0;
  int min0=0;
  int min1=0;

  while (( ((n0=Serial.available())  > 0)  and   ((m0=Serial2.availableForWrite()) > 0 ) )
     or  ( ((n1=Serial2.available()) > 0)  and   ((m1=Serial.availableForWrite())  > 0 ) )) {

    if (n0 > 0) {
      min0=min(n0,m0);
      min0=min(min0,BUFLEN);    
      if (min0 > 0) {
            Serial.readBytes(buf,min0);
            Serial2.write(buf,min0);
      }
    }

    if (n1 > 0) {
      min1=min(n1,m1);  
      min1=min(min1,BUFLEN);  
      if (min1 > 0) {
            Serial2.readBytes(buf,min1);
            Serial.write(buf,min1);
      }
    }
    n0=0;
    m0=0;
    n1=0;
    m1=0;
 }
}

inline void rs41_get_serial (int timeout) {
  // fill the buffer buf and set buftail
  // store miximum the last BUFLEN chars in the buf circular buffer
  int          bytesAvail, bytesIn, i;
  unsigned int startmillis;

  buftail     = 0;
  buflastline = 0;
  startmillis = millis();

  #ifdef DEBUG
  Serial.println("entering rs41_get_serial");
  #endif
  
  while ((millis() - startmillis) < timeout) {
    //check UART for data
    while ((bytesAvail = Serial2.available()) > 0) {
      if (buftail >= sizeof(buf)) {
	buftail = 0;
      }
      bytesIn = Serial2.readBytes(buf+buftail, min(sizeof(buf)-buftail, bytesAvail));
      delay(0);
      buftail += bytesIn;
      // Serial.print("bytesAvail/bytesIn: ");
      // Serial.println(bytesAvail);
      // Serial.println(bytesIn);
    }
    if ((buftail > 0) and (buf[buftail-1] == '>')) {
      break;
    } 
  }

  for (i=0; i < buftail; i++) {
    if (buf[i] == 13) {  // 13: Carriage Return
      buf[i]=10;         // 10: New Line
      buflastline=i;
    }
  }

  buf[buftail]=0;

  #ifdef DEBUG
  Serial.print("buftail:     ");
  Serial.println(buftail);
  Serial.print("buflastline: ");
  Serial.println(buflastline);
  Serial.println("Serial data from rs41:");
  #endif
  Serial.println("------------------------------------------------------------------");
  Serial.write(buf,buftail);
  Serial.println("");
  Serial.println("------------------------------------------------------------------");  
}

inline void rs41_slow_write (char *buf, int maxlen, int msdelay) {
  int  c;
  int  i = 0;

  Serial.print("-----> rs41_slow_write: ");
  Serial.println(buf);
  while ((buf[i] != 0) and (i < maxlen)) {
    delay(msdelay);
    #ifdef DEBUG
    Serial.print(".");
    #endif
    Serial2.write(buf[i]);
    i++;
    delay(0);
  }
  #ifdef DEBUIG
  Serial.println("");
  #endif
}

inline bool rs41_enter_command_mode () {
  Serial.println("-----> Sending STwsv to enter command mode");
  //rs41_slow_write("\rSTwsv\r",10,SLOWTIME_CMD);
  rs41_slow_write("\rST",10,SLOWTIME_CMD);
  rs41_slow_write("wsv\r",10,SLOWTIME);
  rs41_get_serial(GETSERIALTIMEOUT);
  if (buf[buflastline+1] == '>') {
    Serial.println("-----> Entered Command mode");
    return(true);
  } else {
    return(false);
  }
}

inline bool rs41_exit_command_mode () {
  Serial.println("-----> Sending e to exit command mode");
  rs41_slow_write("e",5,SLOWTIME);
  rs41_get_serial(GETSERIALTIMEOUT);
  if ((buftail == 2) && (buf[0] == 'e')) {
    return(true);
  } else {
    return(false);
  }
}


// (S)ensors         Fre(q)uencies  (P)arameters    (A)lfa           TX p(o)wer
// TX (f)requency    T(X) state     (T)X registers  TX contin(u)ous  TX ran(d)om
// TX (c)arrier      (B)aud rate    Ser(i)al no     (R)ed LED info   (N)o menu
// (K)eep test mode  S(W) version   (M)easurements  (L)aunch/Drop    (E)xit
// >t
// Register number (00-7F) >76
// Register value B0 >02
// OK

inline bool rs41_set_register(char *reg, char *val) {
  char *p;

  Serial.println("-----> Sending t command");
  rs41_slow_write("t",5,SLOWTIME);
  rs41_get_serial(GETSERIALTIMEOUT);
  if (strncmp(buf+buflastline+1,"Register number (00-7F) >",25) == 0) {
    Serial.println("-----> Got register change prompt");
  } else {
    Serial.println("-----> ERROR missing register change prompt");
    p=buf+buflastline+1;
    Serial.print(":");Serial.print(p);Serial.print(":");
    return(false);
  }

  Serial.println("-----> Sending register number");
  rs41_slow_write(reg,3,SLOWTIME);
  rs41_slow_write("\r",3,SLOWTIME);
  rs41_get_serial(GETSERIALTIMEOUT);
  if ((strncmp(buf+buflastline+1,"Register value ",15) == 0) && (buf[buftail-1] == '>')) {
    Serial.println("-----> Got register enter value prompt");
  } else {
    Serial.println("-----> ERROR missing register enter value prompt");
    p=buf+buflastline+1;
    Serial.print(":");Serial.print(p);Serial.print(":");    
    return (false);
  }

  rs41_slow_write(val,3,SLOWTIME);
  rs41_slow_write("\r",3,SLOWTIME);
  rs41_get_serial(GETSERIALTIMEOUT);
  if ((buf[buflastline+1] == '>') && (buf[3] == 'O') && (buf[4] == 'K')) {
    Serial.println("-----> Got main menu prompt");
  } else {
    Serial.println("-----> ERROR missing main menu prompt");
    p=buf+buflastline+1;
    Serial.print(":");Serial.print(p);Serial.print(":");    
    return (false);
  }
  return(true);
}

inline void bridge_tcp () {
  uint8_t i;
  int     bytesAvail, bytesIn;

  // bridge_serial();
  // ---------------------------------------------------------------------------
  //check if there are any new clients
  if (tcpServer.hasClient()) {
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      //find free/disconnected spot
      if (!tcpServerClients[i] || !tcpServerClients[i].connected()) {
        if (tcpServerClients[i]) tcpServerClients[i].stop();
        tcpServerClients[i] = tcpServer.available();
        Serial.print("New client: "); Serial.println(i);
        continue;
      }
    }
    //no free/disconnected spot so reject
    WiFiClient tcpServerClient = tcpServer.available();
    tcpServerClient.stop();
  }

  //check clients for data
  for (i = 0; i < MAX_SRV_CLIENTS; i++) {
    if (tcpServerClients[i] && tcpServerClients[i].connected()) {
      //get data from the telnet client and push it to the UART
      while ((bytesAvail = tcpServerClients[i].available()) > 0) {
        bytesIn = tcpServerClients[i].readBytes(buf, min(sizeof(buf), bytesAvail));
        if (bytesIn > 0) {
          Serial2.write(buf, bytesIn);
          delay(0);
        }
      }
    }
  }

  //check UART for data
  while ((bytesAvail = Serial2.available()) > 0) {
    bytesIn = Serial2.readBytes(buf, min(sizeof(buf), bytesAvail));
    if (bytesIn > 0) {
      //push UART data to all connected telnet clients
      for (i = 0; i < MAX_SRV_CLIENTS; i++) {
        if (tcpServerClients[i] && tcpServerClients[i].connected()) {
          tcpServerClients[i].write((uint8_t*)buf, bytesIn);
          delay(0);
        }
      }
    }
  }
}

void handleRoot() {                          // When URI / is requested, send a web page with a form
  String s;
  char   buf[35];
  sprintf(buf,"%7.3f",txfreq);
  
  s="<html>"
    " <head>"
    "   <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>"
    "   <title>RS41 Set Frequency</title>"
    " </head>"
    " <body>"
    " <h1>Select RS41 TX frequency</h1>"
    " <p>Current frequency is ";
  s.concat(buf);
  s.concat(" Mhz</p> ");
  s.concat("<p>Current SSID to connecto to: ");
  s.concat(eeprom_ssid);
  s.concat("</p>");

  // ----- Form for RS41 frequency and time2sleep
  s.concat("<form id=\"regvalues\" action=\"/form\" method=\"get\" target=\"_top\">"
	   "<fieldset class=\"table\">"
	   "<legend>RS41 Set Frequency using registers</legend>"
	   "<div class=\"tr\">"
	   "<div class=\"td\"><label for=\"freq\">Frequency in Mhz</label></div>"
	   "<div class=\"td\"><input id=\"freq\" name=\"freq\" "
	   "type=\"number\" value=\"");
  s.concat(buf);
  s.concat("\" step=\"0.005\" min=\"208\" max=\"831\"></div>"
	   "</div>");

  s.concat("<div class=\"tr\">"
	   "<div class=\"td\"><label for=\"time2sleep\">Time to deep sleep in seconds (to save power)</label></div>"
	   "<div class=\"td\"><input id=\"time2sleep\" name=\"time2sleep\" "
	   "type=\"number\" value=\"");
  sprintf(buf,"%i",time2sleep);
  s.concat(buf);
  s.concat("\" step=\"60\" min=\"180\" max=\"10800\"></div>"
	   "</div>");
  
  s.concat("<div class=\"tr\">"
	   "<div class=\"td\"><input type=\"submit\" value=\"Send\"></div>"
	   "</div>"
	   "</fieldset></form>");
  
  // -------- end of form for RS41 frequency and time to sleep

  // -------- form for SSID and Password to connect to
  s.concat("<form id=\"wifiparams\" action=\"/setwifi\" method=\"get\" target=\"_top\">"
  	   "<fieldset class=\"table\">"
  	   "<legend>Set or Change Wifi Parameters</legend>"
  	   "<div class=\"tr\">"
  	   "<div class=\"td\"><label for=\"ssid\">SSID to connect to</label></div>"
  	   "<div class=\"td\"><select id=\"ssid\" name=\"ssid\">");
  s.concat(ssidList);
  s.concat("</select></div>"
  	   "</div>");

  s.concat("<div class=\"tr\">"
  	   "<div class=\"td\"><label for=\"ssidpass\">SSID password</label></div>"
  	   "<div class=\"td\"><input id=\"ssidpass\" name=\"ssidpass\" "
  	   "type=\"password\" value=\"");
  sprintf(buf,"%s",eeprom_password);
  s.concat(buf);
  s.concat("\"></div>"
  	   "</div>");
  
  s.concat("<div class=\"tr\">"
  	   "<div class=\"td\"><input type=\"submit\" value=\"Send\"></div>"
  	   "</div>"
  	   "</fieldset></form>");

  // -------- end of form for SSID and Password to connect to
  s.concat(" <body>"
	   " </body>"
	   "</html>"
	   );

  webServer.send(200, "text/html",s);

}

void calculateRegisters(float f) {
  float          fref=26.0 / 3.0;
  unsigned int   hbsel;
  unsigned int   fb;
  unsigned int   fc;

  Serial.println("-----> calculateRegisters");

  if (f >= 416.0) {
    hbsel = 1;
  } else {
    hbsel = 0;
  }

  fb = floor(f/(hbsel+1)*30.0/260-24);
  fc = round(64000.0*f/(fref*(hbsel+1)) - 64000.0*fb - 64000.0*24);
  Serial.print("frequency: ");Serial.println(f);
  Serial.print("hbsel:     ");Serial.println(hbsel);
  Serial.print("fb:        ");Serial.println(fb);
  Serial.print("fc:        ");Serial.println(fc);

  sprintf(reg75,"%02X",fb + hbsel * 32);
  sprintf(reg76,"%02X",fc/256);
  sprintf(reg77,"%02X",fc % 256);

  Serial.print("reg75      ");Serial.println(reg75);
  Serial.print("reg76      ");Serial.println(reg76);
  Serial.print("reg77      ");Serial.println(reg77);
}

void handleForm() {
  float f;
  char  sf[15];
  
  String freq = webServer.arg("freq");
  String t2s  = webServer.arg("time2sleep");
  time2sleep = t2s.toInt();
  
  f = freq.toFloat();
  txfreq = f;
  calculateRegisters(f);
  EEPROM.put(0,f);
  EEPROM.put(sizeof(f),time2sleep);
  EEPROM.commit();
  
  sprintf(sf,"%9.3f",f);
  String s = "<html>"
    "<head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>"
    "<style>"
    "table, th, td {"
    "border: 1px solid black;"
    "border-collapse: collapse;"
    "}"
    "</style>"
    
    "<title>RS41 Set Frequency</title>"
    "</head>"
    "<body>"
    "<h1>Set Frequency using registers</h1>"
    "<p>Current time2sleep value: ";
  s.concat(t2s);
  s.concat("</p>");
  s.concat("<table>"
	   "<tr><th>Frequency:</td><td>");
  s.concat(sf);
  s.concat("</td></tr><tr><th>Register 75</td><td>");
  s.concat(reg75);
  s.concat("</td></tr><tr><th>Register 76</td><td>");
  s.concat(reg76);
  s.concat("</td></tr><tr><th>Register 77</td><td>");
  s.concat(reg77);
  s.concat("</td></tr></table>"
	   "<p><strong>The new frequency has been stored in EEPROM, it will be the default from next reboot</strong></p>"
	   "<p><a href=\"/setfreq\">Click here to change to this frequency now, without rebooting</a>"
	   " please be patient, it will take time</p>"
	   "<p>"
	   "<a href='/'> Go Back </a></p>"
	   "</body>"
	   "</html>");
  webServer.send(200, "text/html", s); //Send web page
}

void handleSetfreq () {
  char buf[15];
  sprintf(buf,"%10.3f",txfreq);
  String s = "<html>"   
    "<head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>"
    "<title>RS41 Set Frequency Now</title>"
    "</head>"
    "<body>"
    "<h1>RS41 Set Frequency Now</h1>";

    digitalWrite(LED_BUILTIN, LOW);

    if (rs41_enter_command_mode()        &&
	rs41_set_register("75",reg75)    &&
	rs41_set_register("76",reg76)    &&
	rs41_set_register("77",reg77)    &&
	rs41_exit_command_mode()) {

      Serial.println("=====> Successfully initialized the RS41 Registers");
      status = STS_OK;
      s.concat("<p>Successfully set the frequency ");
      s.concat(buf);
      s.concat(" Mhz</p>");
    } else {
      Serial.println("======> Error in initializing the RS41 Registers");
      status = STS_ERROR;
      s.concat("<p>ERROR failed to set the frequency");
      s.concat(buf);
      s.concat(" Mhz</p>");
    }

    s.concat("<p><a href='/'> Go Back </a></p>");
    s.concat("</body></html>");
    digitalWrite(LED_BUILTIN, HIGH);
    webServer.send(200, "text/html", s); //Send web page

}

void handleSetWifi() {

  int eaddr = sizeof(txfreq) + sizeof(time2sleep);
  String ssid      = urlDecode(webServer.arg("ssid"));
  String ssidpass  = urlDecode(webServer.arg("ssidpass"));

  ssid.toCharArray(eeprom_ssid, sizeof(eeprom_ssid));
  ssidpass.toCharArray(eeprom_password, sizeof(eeprom_password));
  
  EEPROM.put(eaddr,eeprom_ssid);      eaddr+=sizeof(eeprom_ssid);
  EEPROM.put(eaddr,eeprom_password);
  EEPROM.commit();
  
  String s = "<html>"
    "<head>"
    "   <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>"
    "<style>"
    "table, th, td {"
    "border: 1px solid black;"
    "border-collapse: collapse;"
    "}"
    "</style>"
    
    "<title>WIFI Parameters</title>"
    "</head>"
    "<body>"
    "<h1>WIFI Parameters</h1>"
    "<p>SSID: ";
  s.concat(eeprom_ssid);
  s.concat("</p>");
  s.concat("<p>SSID password: ");
  s.concat(eeprom_password);
  s.concat("</p>");
  s.concat("<p>This parameters have been written to the EEPROM</p>");
  
  s.concat("<a href='/'> Go Back </a></p>"
	   "</body>"
	   "</html>");
  webServer.send(200, "text/html", s); //Send web page
}


void setup() {
  int eaddr;   //EEPROM address
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // ----- Setup EEPROM size, we store txfreq, eeprom_ssid, eeprom_password
  EEPROM.begin(sizeof(txfreq) + sizeof(time2sleep) + sizeof(eeprom_ssid) + sizeof(eeprom_password));
  
  Serial.begin(115200);
  Serial2.begin(9600);
  Serial.println();
  Serial.println();

  // ----- Read data from EEPROM (txfreq, time2sleep, eeprom_ssid, eeprom_password)
  eaddr = 0;
  EEPROM.get(eaddr, txfreq);          eaddr += sizeof(txfreq);
  EEPROM.get(eaddr, time2sleep);      eaddr += sizeof(time2sleep);
  EEPROM.get(eaddr, eeprom_ssid);     eaddr += sizeof(eeprom_ssid);
  EEPROM.get(eaddr, eeprom_password);
  
  eeprom_ssid[sizeof(eeprom_ssid)-1]=0;         //zero terminated string, if invalid eeprom data
  eeprom_password[sizeof(eeprom_password)-1]=0; //zero terminated string, if invalid eeprom data

  Serial.print("Read txfreq     from EEPROM: "); Serial.println(txfreq);
  Serial.print("Read time2sleep from EEPROM: "); Serial.println(time2sleep);
  Serial.print("Read ssid       from EEPROM: "); Serial.println(eeprom_ssid);
  Serial.print("Read password   from EEPROM: "); Serial.println(eeprom_password);

  if ((txfreq < 208.0) or (txfreq > 831.0)) {
    Serial.print("Invalid txfreq value, setting to ");
    Serial.println(TXFREQ_DEFAULT);
    txfreq = TXFREQ_DEFAULT;
  }
  
  if ((time2sleep < 180) or (time2sleep > 10800)) { // from 3 min to 3 hours
    Serial.print("Invalid time2sleep value, setting to ");
    Serial.println(TIME2SLEEP);
    time2sleep = TIME2SLEEP;
  }
  
  calculateRegisters(txfreq);

  if (rs41_enter_command_mode()        &&
      rs41_set_register("75",reg75)    &&
      rs41_set_register("76",reg76)    &&
      rs41_set_register("77",reg77)    &&
      rs41_exit_command_mode()) {

    Serial.println("=====> Successfully initialized the RS41 Registers");
    status = STS_OK;
  } else {
    Serial.println("======> Error in initializing the RS41 Registers");
    status = STS_ERROR;
  }

  digitalWrite(LED_BUILTIN, HIGH);
  displayLog("Setting WiFi");
  setupOTA("esp8266-rs41",eeprom_ssid,eeprom_password);

  // Start TCP listener on port TCP_PORT
  tcpServer.begin();
  tcpServer.setNoDelay(true);
  Serial.print("Ready! Use 'telnet or nc ");
  if (getWifiMode() == WIFICLIENT) {
    Serial.print(WiFi.localIP());
  } else {
    Serial.print(getAPip());
  }
  Serial.print(' ');
  Serial.print(TCP_PORT);
  Serial.println("' to connect");

  webServer.on("/",        handleRoot);
  webServer.on("/form",    handleForm);
  webServer.on("/setfreq", handleSetfreq);
  webServer.on("/setwifi", handleSetWifi);
  webServer.begin();
}

void displayLog(char s[]) {
  Serial.println(s);
}

void loop() {
  static unsigned int lastmillis = 0;
  static uint8_t  ledstatus = 0;
  int blinktime = 1000;
  ArduinoOTA.handle();

  if (status == STS_ERROR) {
    blinktime = 50;
  }

  bridge_tcp();
  webServer.handleClient();
  if ((millis() - lastmillis) >= blinktime) {
    lastmillis=millis();
    if (status == STS_ERROR) {
      if (ledstatus == 0) {
	digitalWrite(LED_BUILTIN, LOW);
      } else {
	digitalWrite(LED_BUILTIN, HIGH);
      }
    } else {
	digitalWrite(LED_BUILTIN, ledstatus & 1);
    }
    ledstatus ++;
    ledstatus = ledstatus & 3;
  }

  if (millis() > time2sleep * 1000) {
    digitalWrite(LED_BUILTIN, HIGH); // power off the led
    Serial.println("=====> GOING TO DEEP SLEEP MODE TO SAVE ENERGY - RESET TO WAKEUP");
    delay(500);
    ESP.deepSleep(0);
  }
	
}
