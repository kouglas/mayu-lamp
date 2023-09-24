#include <WiFiManager.h>
#include <WebServer.h>
#include <Arduino.h>

#include <WiFi.h>

/*
if you're going to use the WIfiLocation library
make sure version 1.2.9 is installed or lower
becasue at time of writing (09.19.2023) this 
version is not compatible with the esp32

*/

#include <WifiLocation.h>

// new stuff starts here 9.24.2023

#include <AceTime.h>
using namespace ace_time;

#if ! defined(NTP_SERVER)
#define NTP_SERVER "pool.ntp.org"
#endif


// Value of time_t for 2000-01-01 00:00:00, used to detect invalid SNTP
// responses.
static const time_t EPOCH_2000_01_01 = 946684800;

// Number of millis to wait for a WiFi connection before doing a software
// reboot.
static const unsigned long REBOOT_TIMEOUT_MILLIS = 15000;

//-----------------------------------------------------------------------------


// C library day of week uses Sunday==0.
static const char* const DAYS_OF_WEEK[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

// Print the UTC time from time_t, using C library functions.
void printNowUsingCLibrary(time_t now) {
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);

  int year = timeinfo.tm_year + 1900; // tm_year starts in year 1900 (!)
  int month = timeinfo.tm_mon + 1; // tm_mon starts at 0 (!)
  int day = timeinfo.tm_mday; // tm_mday starts at 1 though (!)
  int hour = timeinfo.tm_hour;
  int mins = timeinfo.tm_min;
  int sec = timeinfo.tm_sec;
  int day_of_week = timeinfo.tm_wday; // tm_wday starts with Sunday=0
  const char* dow_string = DAYS_OF_WEEK[day_of_week];

  Serial.printf("%04d-%02d-%02dT%02d:%02d:%02d %s",
      year, month, day, hour, mins, sec, dow_string);
  Serial.println(F(" (C lib)"));
}

//-----------------------------------------------------------------------------

// Define 2 zone processors to handle 2 timezones (America/Los_Angeles,
// Europe/Paris) efficiently. It is possible to use only 1 to save memory, at
// the cost of slower performance. These are heavy-weight objects so should be
// created during the initialization of the app.
ExtendedZoneProcessor zoneProcessorLosAngeles;
ExtendedZoneProcessor zoneProcessorParis;
ExtendedZoneProcessor zoneProcessorChicago;

// Print the UTC time, America/Los_Angeles time, and Europe/Paris time using
// the AceTime library. TimeZone objects are light-weight and can be created on
// the fly.
void printNowUsingAceTime(time_t now) {
  // Utility to convert ISO day of week with Monday=1 to human readable string.
  DateStrings dateStrings;

  // Convert to UTC time.
  LocalDateTime ldt = LocalDateTime::forUnixSeconds64(now);
  ldt.printTo(Serial);
  Serial.print(' ');
  Serial.print(dateStrings.dayOfWeekLongString(ldt.dayOfWeek()));
  Serial.println(F(" (AceTime)"));

  // Convert Unix time to Los Angeles time.
  TimeZone tzLosAngeles = TimeZone::forZoneInfo(
      &zonedbx::kZoneAmerica_Los_Angeles,
      &zoneProcessorLosAngeles);
  ZonedDateTime zdtLosAngeles = ZonedDateTime::forUnixSeconds64(
      now, tzLosAngeles);
  zdtLosAngeles.printTo(Serial);
  Serial.print(' ');
  Serial.println(dateStrings.dayOfWeekLongString(zdtLosAngeles.dayOfWeek()));
  
  // Convert Unix time to Chicago time.
  TimeZone tzChicago = TimeZone::forZoneInfo(
      &zonedbx::kZoneAmerica_Chicago,
      &zoneProcessorChicago);
  ZonedDateTime zdtChicago = ZonedDateTime::forUnixSeconds64(
      now, tzChicago);
  zdtChicago.printTo(Serial);
  Serial.print(' ');
  Serial.println(dateStrings.dayOfWeekLongString(zdtChicago.dayOfWeek()));

  // Convert Los Angeles time to Paris time.
  TimeZone tzParis = TimeZone::forZoneInfo(
      &zonedbx::kZoneEurope_Paris,
      &zoneProcessorParis);
  ZonedDateTime zdtParis = zdtLosAngeles.convertToTimeZone(tzParis);
  zdtParis.printTo(Serial);
  Serial.print(' ');
  Serial.println(dateStrings.dayOfWeekLongString(zdtParis.dayOfWeek()));
}







const char* googleApiKey = "AIzaSyBb3czYcUFpQnm25rdUXz7I4iqh4wjLgMg";

WifiLocation location (googleApiKey);


// Set time via NTP, as required for x.509 validation
void setClock () {
    configTime (0, 0, "pool.ntp.org", "time.nist.gov");

    Serial.print ("Waiting for NTP time sync: ");
    time_t now = time (nullptr);
    while (now < 8 * 3600 * 2) {
        delay (500);
        Serial.print (".");
        now = time (nullptr);
    }
    struct tm timeinfo;
    gmtime_r (&now, &timeinfo);
    Serial.print ("\n");
    Serial.print ("Current time: ");
    Serial.print (asctime (&timeinfo));
}

// Setup the SNTP client. Set the local time zone to be UTC, with no DST offset,
// because we will be using AceTime to perform the timezone conversions. The
// built-in timezone support provided by the ESP8266/ESP32 API has a number of
// deficiencies, and the API can be quite confusing.
void setupSntp() {
  Serial.print(F("Configuring SNTP"));
  configTime(0 /*timezone*/, 0 /*dst_sec*/, NTP_SERVER);

  // Wait until SNTP stabilizes by ignoring values before year 2000.
  unsigned long startMillis = millis();
  while (true) {
    Serial.print('.'); // Each '.' represents one attempt.
    time_t now = time(nullptr);
    if (now >= EPOCH_2000_01_01) {
      Serial.println(F(" Done."));
      break;
    }

    // Detect timeout and reboot.
    unsigned long nowMillis = millis();
    if ((unsigned long) (nowMillis - startMillis) >= REBOOT_TIMEOUT_MILLIS) {
    #if defined(ESP8266)
      Serial.println(F(" FAILED! Rebooting..."));
      delay(1000);
      ESP.reset();
    #elif defined(ESP32)
      Serial.println(F(" FAILED! Rebooting..."));
      delay(1000);
      ESP.restart();
    #else
      Serial.print(F(" FAILED! But cannot reboot. Continuing"));
      startMillis = nowMillis;
    #endif
    }

    delay(500);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(10);

  WiFiManager wifiManager;


  // If you've previously connected to your WiFi with this ESP32,
  // WiFi manager will more than likely not do anything.
  // Uncomment this if you want to force it to delete your old WiFi details.
  wifiManager.resetSettings();

  //Tries to connect to last known WiFi details
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("Sartaj_lamp_wifiManager", "")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  setupSntp();
  


  setClock ();
  location_t loc = location.getGeoFromWiFi();

    Serial.println("Location request data");
    Serial.println(location.getSurroundingWiFiJson()+"\n");
    Serial.println ("Location: " + String (loc.lat, 7) + "," + String (loc.lon, 7));
    //Serial.println("Longitude: " + String(loc.lon, 7));
    Serial.println ("Accuracy: " + String (loc.accuracy));
    Serial.println ("Result: " + location.wlStatusStr (location.getStatus ()));
}

void loop() {
  // put your main code here, to run repeatedly:
  time_t now = time(nullptr);
  printNowUsingCLibrary(now);
  printNowUsingAceTime(now);
  Serial.println();

  delay(5000);

}