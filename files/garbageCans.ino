/*
 *
 *  Take out the garbage!
 *
 *  An open source hardware project to remind you to take out the garbage before the garbage trucks arrive!
 *
 *
 *
 * June, 2017 - SDM
 * Version: 0.01 - Alpha
 *
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define NEOPIN      15
#define NUMPIXELS   16
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, NEOPIN, NEO_GRB + NEO_KHZ800);

/*
 *   Define your WiFi SSID and password here
 */
char ssid[] = "your ssid here";      // your network SSID (name)
char pass[] = "your password here";    // your network password

/*
 *   Set your timezone here
 *   Each hour is 3600 seconds
 *   For daylight savings, take off another hour
 *   EST = GMT -5, or 3600*5 = 18,000
 *   EDT = EST - 3600, or 14,400
 *   I'm in California, so thats GMT -8
 *     (28,800 - DST = 25,200)
 */
const long timeZoneOffset = -25200;    // set your timezone here

unsigned int localPort = 2390;      // local port to listen for UDP packets

/* Don't hardwire the IP address or we won't get the benefits of the pool.
 *  Lookup the IP address for the host name instead */
IPAddress timeServer(10, 0, 0, 2);
//IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "10.0.0.2";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

int button = 2;    // this is the button under the sign

unsigned long ntpPreviousMillis = 0;

void setup()
{
  // setup button for input
  pinMode(button, INPUT_PULLUP);

  // start up the NeoPixel strip
  strip.begin();

  // start up the serial port
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println();

  // connect to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  // blink NeoPixels while connecting to WiFi
  while (WiFi.status() != WL_CONNECTED) {
    neoPixelsRed();
    delay(250);
    Serial.print(".");
    neoPixelsOff();
    delay(250);
  }

  // spit out connection details to serial port
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");
  Serial.println("Starting UDP");
  udp.begin(localPort);
  //Serial.print("Local port: ");
  //Serial.println(udp.localPort());

  // flash green when connected
  neoPixelsGreen();
  delay(500);
  neoPixelsOff();
}

void loop()
{
  // get current milliseconds -- used for timing
  unsigned long currentMillis = millis();
  
  //get a random server from the pool
  //WiFi.hostByName(ntpServerName, timeServerIP); 

  // check to see if 600 seconds has passed
  if ( ntpPreviousMillis + 600000 < currentMillis ) { // do this every 600 seconds
    ntpPreviousMillis = currentMillis;
    sendNTPpacket(timeServer); // send an NTP packet to a time server
    delay(500);
    checkNTPpacket();
  }

  // check to see if anyone pressed the button
  checkButtonPress();  
}

void checkButtonPress() {
  int buttonPress = 1;
  buttonPress = digitalRead(button);
// if someone presses the button, turn the lights green for 1 second
  if ( buttonPress < 1 ) {
    neoPixelsGreen();
    Serial.println("Button Pressed");
    delay(1000);
    neoPixelsOff();
  }
}

// make NeoPixels RED
void neoPixelsRed () {
  for(int i=0;i<NUMPIXELS;i++){
    strip.setPixelColor(i, strip.Color(150,0,0));  // RED
    strip.show();
  }
}

// make NeoPixel Green
void neoPixelsGreen () {
  for(int i=0;i<NUMPIXELS;i++){
    strip.setPixelColor(i, strip.Color(0,150,0));  // GREEN
    strip.show();
  }
}
// turn off NeoPixels (make them black)
void neoPixelsOff () {
  for(int i=0;i<NUMPIXELS;i++){
    strip.setPixelColor(i, strip.Color(0,0,0));  // BLACK
    strip.show();
  }
}

/*

 Udp NTP Client

 Get the time from a Network Time Protocol (NTP) time server
 Demonstrates use of UDP sendPacket and ReceivePacket
 For more on NTP time servers and the messages needed to communicate with them,
 see http://en.wikipedia.org/wiki/Network_Time_Protocol

 created 4 Sep 2010
 by Michael Margolis
 modified 9 Apr 2012
 by Tom Igoe
 updated for the ESP8266 12 Apr 2015
 by Ivan Grokhotkov

 This code is in the public domain.

*/

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address) {
  Serial.println("sending NTP packet...");
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
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void checkNTPpacket() {
    int cb = udp.parsePacket();
    if (!cb) {
    Serial.print(". ");
  } else {
    Serial.print("packet received, length=");
    Serial.println(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = " );
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    //Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears + timeZoneOffset;

    String datestring="";
    datestring += month(epoch);  datestring += "/";
    datestring += day(epoch);    datestring += "/";
    datestring += year(epoch);   datestring += " ";
    datestring += hour(epoch);   datestring += ":";
    datestring += minute(epoch); datestring += " ";
    datestring += weekday(epoch);

    /*
     * Sunday = 0
     * Monday = 1
     * Tuesday = 2
     * Wednesday = 3
     * Thursday = 4
     * Friday = 5
     * Saturday = 6
     * Sunday = 7
     */

// if it's Thursday after 8pm, turn on the red light
    if ( weekday(epoch) == 5 ) {
      // Thursday
      if ( hour(epoch) > 19 ) {
        // After 8pm
        datestring += " ON";
        neoPixelsRed();
      }
    }
// if it's Friday before 8am, turn on the red light
    else if ( weekday(epoch) == 6 ) {
      // Friday
      if ( hour(epoch) < 8 ) {
        // Before 8am
        datestring += " ON";
        neoPixelsRed();
      }
    }
// otherwise, turn off the lights
    else {
      neoPixelsOff();
    }
    Serial.println(datestring);
  }
}

