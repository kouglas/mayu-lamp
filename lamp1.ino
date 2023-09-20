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


const char* googleApiKey = SECRET_APIKEY;

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

}
