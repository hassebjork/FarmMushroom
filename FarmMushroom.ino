// A2:20:A6:0D:2E:02

/* OTA */
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
const char* host = "mushroom";
const char* ssid = "BJORK";
const char* password = "hasse.bjork_home.se";
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

/* Display */
#include "SSD1306.h"
#include "fontRoboto.h"
#define THERMO_width 11
#define THERMO_height 30
SSD1306  display(0x3c, 4, 5);

/* DHT */
#include "math.h"     // function isnan()
#include <DHT.h>
//DHT dht( D4, DHT21 ); // AM2301 (Black)
DHT dht( D4, DHT22 ); // AM2302 (White)
float temp;
float humid;

/* FAN */
#define FAN D3

/* TIME */
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
WiFiUDP Udp;
IPAddress timeServer;            // time.nist.gov NTP server address
const int timeZone = 1;         // Central European Time
unsigned int localPort = 8888;  // local port to listen for UDP packets
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
time_t t;

char charBuf[32];
IPAddress ip;
byte mac[6];

static char THERMO_bits[] = {
   0x60, 0x00, 0x90, 0x00, 0x97, 0x00, 0x90, 0x00, 0x90, 0x00, 0x90, 0x00,
   0x96, 0x00, 0x90, 0x00, 0x90, 0x00, 0x90, 0x00, 0x97, 0x00, 0x90, 0x00,
   0x90, 0x00, 0x90, 0x00, 0x96, 0x00, 0x90, 0x00, 0x90, 0x00, 0x90, 0x00,
   0x97, 0x00, 0xf0, 0x00, 0xf8, 0x01, 0xfc, 0x03, 0xfc, 0x03, 0xfe, 0x07,
   0xfe, 0x07, 0xfe, 0x07, 0xfe, 0x07, 0xfc, 0x03, 0xf8, 0x01, 0xf0, 0x00 
};

void setup() {
  /* Display */
  display.init();
  display.clear();
  display.setContrast( 255 );
  display.flipScreenVertically();
  display.setFont( Roboto_Black_16 );
  display.setTextAlignment( TEXT_ALIGN_LEFT );
  display.drawString( 0,  0, F("MushroomFarm") );
  display.drawString( 0, 15, F( __DATE__) );
  display.display();
  
/* OTA */
//  WiFi.mode( WIFI_STA );
  WiFi.mode( WIFI_AP_STA );
  WiFi.begin( ssid, password );
  while( WiFi.waitForConnectResult() != WL_CONNECTED ) {
    WiFi.begin( ssid, password );
    display.drawString( 0, 50, F("WiFi failed, retrying.") );
    display.display();
  }
  ip = WiFi.localIP();
  MDNS.begin( host );
  httpUpdater.setup( &httpServer );
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);
//  WiFi.macAddress( mac );
//  sprintf( charBuf, "%02X:%02X:%02X:%02X:%02X:%02X", 
//      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
//  display.drawString( 0, 30, charBuf );
  sprintf( charBuf, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3] );
  display.drawString( 0, 35, charBuf );
  display.drawString( 0, 50, String(  String( host ) + ".local/update") );
  display.display();

  /* DHT */
  dht.begin();
  
  /* Time */  
  setSyncInterval( 24*60*60 );  // Daily
  setSyncProvider( getNtpTime );
  
  /* FAN */
  pinMode( FAN, OUTPUT );
  digitalWrite( FAN, 0 );
}

void loop() {
    httpServer.handleClient();
    delay( 5000 );
    
    temp  = dht.readTemperature();
    humid = dht.readHumidity();
    t     = now();
    if ( summerTime( t ) )
      t += 3600;

    display.clear();
    display.drawXbm(0, 5, THERMO_width, THERMO_height, THERMO_bits);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    sprintf( charBuf, "%02d:%02d", hour( t ), minute( t ) );
    display.drawString( 20, 0, charBuf );
    display.drawString( 16, 15, "T: " 
        + ( isnan( temp  ) ? "- - -" : String( temp,  1 ) ) + "C " );
    display.drawString( 16, 30, "H: " 
        + ( isnan( humid ) ? "- - -" : String( humid, 1 ) ) + "% " );
    display.display();

    if ( humid > 95 || temp > 32 )
      digitalWrite( FAN, 1 );
    else if ( humid < 85 )
      digitalWrite( FAN, 0 );
    
    yield();
}

time_t getNtpTime() {
  Udp.begin(localPort);
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  WiFi.hostByName(ntpServerName, timeServer); 
  
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

bool summerTime( time_t t ) {
  if ( month(t)  < 3 || month(t) > 10 ) return false;  // Jan, Feb, Nov, Dec
  if ( month(t)  > 3 && month(t) < 10 ) return true;  // Apr, Jun; Jul, Aug, Sep
  if ( month(t) ==  3 && ( hour(t) + 24 * day(t) ) >= ( 3 +  24 * ( 31 - ( 5 * year(t) / 4 + 4 ) % 7 ) ) 
    || month(t) == 10 && ( hour(t) + 24 * day(t) ) <  ( 3 +  24 * ( 31 - ( 5 * year(t) / 4 + 1 ) % 7 ) ) )
    return true;
  else
    return false;
}

