#define vers "Version: 8-zeilig, 17.06.2024, Browser Ausgabe"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>                                     // WiFi

const char *mqtt_broker = "homeassistant.local";                   // Enter your WiFi or Ethernet IP
const char *topic1 = "Wasserstand";
const int   mqtt_port = 1883;
const char *mqtt_user = "mq-serve";
const char *mqtt_pass = "home";
WiFiClient espClient;

#include "ota.h" 
#include "Internetzeit.h"
#include <OneWire.h>
#include "user_interface.h" //für Ermittlung RSSI Wert
#include "Telnet2Serial.h" //ermoeglicht die Ausgabe von Werten via Telnet auf Port 23
#include "ADS1115-Driver.h" // Analog I2C Baustein
ADS1115 ads1115 = ADS1115(ADS1115_I2C_ADDR_GND); // AnalogWandler ueber I2C, Adresspin auf Ground
ADC_MODE(ADC_VCC);
#include <ESP8266WebServer.h> // Server 
ESP8266WebServer server(80); // create the webserver on port 80
char ssid[22]="hartmutblauWohnzimmer", password[18]="paulus11";
char buf[1000];


//---------------------------------
String BatAmp = "";
//--------------------------------
PubSubClient client(espClient);
void setup() {                  // Set software serial baud to 115200;
 Serial.begin(115200); 
 delay(500);Serial.printf("\nTeichfuellstand Monitoring, %s", vers);
 wifistart();  //WLAN starten
 ads1115.reset(); //Initialisierung I2C Analogwandler
 ads1115.setDeviceMode(ADS1115_MODE_SINGLE);
 ads1115.setDataRate(ADS1115_DR_32_SPS);
 ads1115.setPga(ADS1115_PGA_2_048);            //connecting to a mqtt broker
 
 
 client.setServer(mqtt_broker, mqtt_port);
 client.setCallback(callback); 
 mqtt("Hello From ESP");


}


void wifistart() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname("Fuellstand3"); 
  WiFi.begin(ssid, password); //Serial.println("\nConnecting to FRITZ!UWE");
  while (WiFi.status() != WL_CONNECTED) delay(500); 
  sprintf(buf,"\nconnected, address=%s; Hostname=%s, Version= %s\r\n",WiFi.localIP().toString().c_str(),WiFi.hostname().c_str(),vers);
  Serial.print(buf);
  server.on("/", handleRoot); // handleRoot() ist die Funktion die aufgerufen wird, wenn nur die IP ohne Zusatz eingegeben wird
  server.onNotFound(handleNotFound); //bei IP/irgendwas... wird die Funktion handleNotFound aufgerufen
  server.begin();
}



void callback(char *topic1, byte *payload, unsigned int length) {
 Serial.print("Message arrived in topic1: ");
 Serial.println(topic1);
 Serial.print("Message:");
 
 for (int i = 0; i < length; i++) {
  Serial.print((char) payload[i]);
//----------------------------------------------
BatAmp = (BatAmp+(char) payload[i]);
//---------------------------------------------------
 }
 Serial.print ("   test  ");
 float NumBatAmp = BatAmp.toFloat();
 Serial.println (BatAmp);
 Serial.println (NumBatAmp);
BatAmp = '\0';

 Serial.println();
 Serial.println(" - - - - - - - - - - - -");
}

void loop() {
  //static int tm_tm_hour=17,testcnt=1;if(timer(1,10000)==true) {if(tm.tm_hour<23) tm.tm_hour+=1;else tm.tm_hour =0;} //zum Test tm.tm_hour im ganzen loop Bereich ersetzen z.B. durch tm.tm_hour
  uwes_ota(); //Over the air (ota) update durchführen?
  server.handleClient(); //gibt es eine Anfrage an den Server?
  if(timer(0,5000)==true) {  
    static int erster_durchlauf=0;
    if(erster_durchlauf<10) { //Am Anfang Version usw. ausgeben....
      erster_durchlauf++;
      sprintf(buf,"\n%d, connected, address=%s; Hostname=%s, Version= %s\r\n",erster_durchlauf, WiFi.localIP().toString().c_str(),WiFi.hostname().c_str(),vers);
      telnet_write(buf);
    }
    if (WiFi.status() != WL_CONNECTED) {delay(60000); wifistart();}
    showTime(); //aktuelles Datum und zeit holen (aus Internetzeit.h)  
    uint32_t value0 = readValue(ADS1115_MUX_AIN0_GND); //Analogwert am Eingang 0 einlesen
    if(value0>40000) value0=0;
    if(value0>20000) value0=29999;
    unsigned long tiefe1=900*(value0-4300)/(14870-4300); //Spannungswert in Millimeter gemaess eigener Messreihe umrechnen, wobei 0,4300 Volt 0mm und 1,4870 Volt 900mm Tiefe entsprechen
    static unsigned int tiefe[8][6]={ {0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0} }, //fuer 8 Tage,6 Werte/Tag 
                        tag_cnt=8, tm_hour_alt=0, datum[8] {99,99,99,99,99,99,99,99};
    if(tm.tm_hour!=tm_hour_alt && tm.tm_hour%4==0 ) {
      tm_hour_alt=tm.tm_hour;
      if(tm_hour_alt==0) {
        tag_cnt++;
        for(int n=1; n<6; n+=1) tiefe[tag_cnt%8][n]=0; //aktuellen Tag von 4 -20 Uhr im Array löschen
      }
      tiefe[tag_cnt%8][tm.tm_hour/4]=tiefe1;
      datum[tag_cnt%8]=tm.tm_mday;
    }
    
    sprintf(buf,  "\rF U E L L S T A N D S A N Z E I G E\n\r");
    sprintf(buf+strlen(buf),"===================================\n\r");
    sprintf(buf+strlen(buf),"\n%02d.%02d.%04d-%02d:%02d:%02d; 0_Uhr   4_hr    8_Uhr  12_Uhr  16_Uhr  20_Uhr\n\r",tm.tm_mday,tm.tm_mon, tm.tm_year, tm.tm_hour, tm.tm_min, tm.tm_sec);
    for(int n=7; n>=0; n--) sprintf(buf+strlen(buf), "Tag %2d.          %5d;  %5d;  %5d;  %5d;  %5d;  %5d\n\r", 
                                    /*tag_cnt%8-n*/ datum[(tag_cnt-n)%8],  tiefe[(tag_cnt-n)%8][0],tiefe[(tag_cnt-n)%8][1],tiefe[(tag_cnt-n)%8][2],tiefe[(tag_cnt-n)%8][3],tiefe[(tag_cnt-n)%8][4],tiefe[(tag_cnt-n)%8][5]);
    sprintf(buf+strlen(buf), "A K T U E L L: Stunde=%02d; %01d.%04dV;  %02dmm; RSSI;%03d;EspVcc;%d\n\r\n\n", tm.tm_hour, value0/10000, value0%10000, tiefe1, wifi_station_get_rssi(),ESP.getVcc() );
    Serial.print(buf);
    telnet_write(buf);

    

    Serial.print("Send ");
    Serial.print(String(tiefe1));
    mqtt(String(tiefe1));

  }
}
//*********************************************************************
uint16_t readValue(uint8_t input) {
  ads1115.setMultiplexer(input);
  ads1115.startSingleConvertion();
  delayMicroseconds(25); // The ADS1115 needs to wake up from sleep mode and usually it takes 25 uS to do that
  while (ads1115.getOperationalStatus() == 0);
  return ads1115.readConvertedValue();
}
//*********************************************************************
//mit dauer=0 wird timer initialisiert, mit dauer=1 wird Timer gestartet, nach Ablauf findet kein Rücksetzen bzw. neu "aufziehen" statt
//mit dauer=2 wie bei 1, kommt aber nur einmal mit true zurück
//wenn die Zeit "dauer" in ms um ist, return true, sonst false
boolean timer(int nr, unsigned long dauer) { 
  static int fl_timer[4];
  static unsigned long ti_timer[4];
  if(dauer<=2) {
    fl_timer[nr] = int (dauer);
    ti_timer[nr] = millis();
    return(false);
  }
  else {
    if((ti_timer[nr]+dauer) > millis()) return (false);
    else {
      if(fl_timer[nr]<=2) {
        if(fl_timer[nr]==2) fl_timer[nr]=3;
        if(fl_timer[nr]==0) ti_timer[nr]=millis();
        return(true);
      }
    return (false);
    }
  }
}
//*********************************************************************
void handleRoot() {
  int laenge=strlen(buf);
  for(int n=0;n<laenge;n++) if(buf[n]=='\r') buf[n]=' '; // \r eleminieren, da eine zusätzliche Leerzeile erzeugt wird
  server.send(200, "text/plain", buf); //ohne grosses HTML Gedoens auf den Internetbroser ausgeben
}
//*********************************************************************
//*********************************************************************
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    yield();
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void mqtt(String messsage) {
if (!client.connected()) {
 String client_id = "esp8266-client-";
 client_id += String(WiFi.macAddress());

 
 if (client.connect(mqtt_user, mqtt_user, mqtt_pass)) {
  Serial.println("Wasserstandssonde ist mit Server verbunden");
 } else {
  Serial.print("Fehler: ");
  Serial.print(client.state());
  delay(2000);
 }
 Serial.print(messsage);
 client.publish(topic1, messsage.c_str());
 client.subscribe(topic1);

}
}
