/*

    Copyright (C) 2017 Darren Poulson <darren.poulson@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
#include "config.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SimpleTimer.h>
#include <ModbusMaster.h>
#include <PubSubClient.h>
#include <NTPClient.h>

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

int timerTask2, timerTask3;
int timerTask4, timerTask5, timerTask6;
int wifip = 0;
float ctemp, bvoltage, battChargeCurrent, btemp, bremaining, lpower, lcurrent, pvvoltage, pvcurrent, pvpower;
float batt_type, batt_cap, batt_highdisc, batt_chargelimit, batt_overvoltrecon, batt_equalvolt, batt_boostvolt, batt_floatvolt, batt_boostrecon;
float batt_lowvoltrecon, batt_undervoltrecon, batt_undervoltwarn, batt_lowvoltdisc;
uint8_t result;
bool mqtt_connected = false;
unsigned long startTime = 0;
int otaInit=0;


#ifdef UNSECURE_MQTT
WiFiClient espClient;
PubSubClient client(espClient);
#else
WiFiClientSecure espClient;     // <-- Change #1: Secure connection to MQTT Server
PubSubClient client(espClient);
#endif

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org"); // Always use UTC, adjust serverside if needed


// this is to check if we can write since rs485 is half duplex
bool rs485DataReceived = true;
ModbusMaster node;
SimpleTimer timer;

char buf[10];
char mptt_location[16];

// tracer requires no handshaking
void preTransmission() {}
void postTransmission() {}

// a list of the regisities to query in order
typedef void (*RegistryList[])();
RegistryList Registries = {
  AddressRegistry_3100,
  AddressRegistry_311A,
  AddressRegistry_9000,
};
// keep log of where we are
uint8_t currentRegistryNumber = 0;

// function to switch to next registry
void nextRegistryNumber() {
  currentRegistryNumber = (currentRegistryNumber + 1) % ARRAY_SIZE( Registries);
}

IPAddress local_IP(11,11,11,254);
IPAddress gateway(11,11,11,254);
IPAddress subnet(255,255,255,0);
WiFiServer server(8088);
//WiFiClient client=false;

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "" : "Failed!");
  Serial.println(WiFi.softAP("eBox-WIFI-01") ? "" : "Failed!");

  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());
    

  // Start Wifi, will be monitored by state-engine
//  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_networks[wifip], wifi_networks[wifip + 1]);

  setupModbus();


  espClient.setInsecure(); // TLS, but no certificate mgmt.
  client.setServer(mqtt_server, 8883);

  timeClient.begin();

  timerTask2 = timer.setInterval(10  * 1000, doRegistryNumber);
  timerTask3 = timer.setInterval(10  * 1100, nextRegistryNumber);
  timerTask4 = timer.setInterval(19 * 1000, doMQTTAlive);
  timerTask5 = timer.setInterval(60  * 1000, doWifiScan);

  timerTask6 = timer.setInterval(7  * 1000, doStateEngine); // dont run to fast, need time to connec to wifi a.o.t.

}
void setupModbus() {
  // Modbus slave ID 1
  node.begin(EPSOLAR_DEVICE_ID, Serial);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
}

void setupOTA() {
  // Enable OTA
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(OTA_HOSTNAME);

  // No authentication by default
  //ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

}

void doStateEngine() {
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Not connected to wifi...");
    //    delay(5000);
    //  WiFi.begin(wifi_networks[wifip], wifi_networks[wifip+1]);
    //    ESP.restart();
    WiFi.disconnect();
    delay(500);
    wifip += 2;
    if (strcmp(wifi_networks[wifip],"")==0) {
      wifip = 0;
    }
    Serial.print("Connecting to wifi:");
    Serial.print(wifip);
    Serial.print(":");
    Serial.print(wifi_networks[wifip]);
    Serial.print(":");
    Serial.println(wifi_networks[wifip + 1]);


    WiFi.begin(wifi_networks[wifip], wifi_networks[wifip + 1]);
  } else {
  if (otaInit==0) {
    setupOTA();
    otaInit=1;
  }
  }

  if (!client.connected()) {
    mqtt_connected = false;
    Serial.println("Connecting to mqtt");
    if (client.connect("EPSolar1", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqtt_connected = true;
    } else {
      Serial.print("Failed to connect");
      Serial.print(mqtt_user);
      Serial.print(":");
      Serial.println(mqtt_pass);
    }
  } else {
    mqtt_connected = true;
  }
}

void doMQTTAlive() {
  char tsFromLocal[32];
  timeClient.update();

  time_t     now;
  struct tm  ts;
  char       tbuf[80];

  //time(&now);
  now = timeClient.getEpochTime();
  if (startTime == 0 && now > 1000) {
    startTime = now;
  }
  ts = *localtime(&now);
  strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S %Z", &ts);

  client.publish("EPSolar/1/alive", "EPSolar1 1.002");
  client.publish("EPSolar/1/utctime", tbuf);
  client.publish("EPSolar/1/epochtime", ltoa(timeClient.getEpochTime(), buf, 10));
  client.publish("EPSolar/1/uptime", ltoa(timeClient.getEpochTime() - startTime, buf, 10));

}
void doWifiScan() {
  char wbuf[128];
  if (!mqtt_connected) return; // No reason to execute if not connected to mqtt
  long rssi = WiFi.RSSI();
  client.publish("EPSolar/1/rssi", ltoa(rssi, buf, 10));
  int numSsid = WiFi.scanNetworks();
  if (numSsid == -1)
  {
    Serial.println("Couldn't get a wifi connection");
    return;
  }
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    String xbuf = "{\"ssid\":\"" + WiFi.SSID(thisNet) + "\", \"rssi\":" + WiFi.RSSI(thisNet) + ",\"chan\":" + WiFi.channel(thisNet) + ",\"bssid\":\"" + WiFi.BSSIDstr(thisNet) + "\"}";
    xbuf.toCharArray(wbuf, sizeof(wbuf));
    client.publish("EPSolar/1/wifiscan", wbuf);
  }
}

void doRegistryNumber() {
  Registries[currentRegistryNumber]();
}

void AddressRegistry_3100() {
  result = node.readInputRegisters(0x3100, 10);
  if (result == node.ku8MBSuccess)
  {
    ctemp = (long)node.getResponseBuffer(0x11) / 100.0f;
    dtostrf(ctemp, 2, 3, buf );
    //mqtt_location = MQTT_ROOT + "/" + EPSOLAR_DEVICE_ID + "/ctemp";
    client.publish("EPSolar/1/ctemp", buf);

    bvoltage = (long)node.getResponseBuffer(0x04) / 100.0f;
    dtostrf(bvoltage, 2, 3, buf );
    client.publish("EPSolar/1/bvoltage", buf);

    lpower = ((long)node.getResponseBuffer(0x0F) << 16 | node.getResponseBuffer(0x0E)) / 100.0f;
    dtostrf(lpower, 2, 3, buf);
    client.publish("EPSolar/1/lpower", buf);

    lcurrent = (long)node.getResponseBuffer(0x0D) / 100.0f;
    dtostrf(lcurrent, 2, 3, buf );
    client.publish("EPSolar/1/lcurrent", buf);

    pvvoltage = (long)node.getResponseBuffer(0x00) / 100.0f;
    dtostrf(pvvoltage, 2, 3, buf );
    client.publish("EPSolar/1/pvvoltage", buf);

    pvcurrent = (long)node.getResponseBuffer(0x01) / 100.0f;
    dtostrf(pvcurrent, 2, 3, buf );
    client.publish("EPSolar/1/pvcurrent", buf);

    pvpower = ((long)node.getResponseBuffer(0x03) << 16 | node.getResponseBuffer(0x02)) / 100.0f;
    dtostrf(pvpower, 2, 3, buf );
    client.publish("EPSolar/1/pvpower", buf);

    battChargeCurrent = (long)node.getResponseBuffer(0x05) / 100.0f;
    dtostrf(battChargeCurrent, 2, 3, buf );
    client.publish("EPSolar/1/battChargeCurrent", buf);

  } else {
    rs485DataReceived = false;
  }
}

void AddressRegistry_311A() {
  result = node.readInputRegisters(0x311A, 2);
  if (result == node.ku8MBSuccess)
  {
    bremaining = node.getResponseBuffer(0x00) / 1.0f;
    dtostrf(bremaining, 2, 3, buf );
    client.publish("EPSolar/1/bremaining", buf);

    btemp = node.getResponseBuffer(0x01) / 100.0f;
    dtostrf(btemp, 2, 3, buf );
    client.publish("EPSolar/1/btemp", buf);

  } else {
    rs485DataReceived = false;
  }
}

void AddressRegistry_9000() {
  result = node.readHoldingRegisters(0x9000, 14);
  client.publish("EPSolar/1/loop1", "9000");
  if (result == node.ku8MBSuccess)
  {
    client.publish("EPSolar/1/loop1", "9000 result");
    batt_cap = node.getResponseBuffer(0x01) / 1.0f;
    dtostrf(batt_cap, 2, 3, buf);
    client.publish("EPSolar/1/batt_cap", buf);
    batt_highdisc = node.getResponseBuffer(0x03) / 100.0f;
    dtostrf(batt_highdisc, 2, 3, buf);
    client.publish("EPSolar/1/batt_highdisc", buf);

    batt_chargelimit = node.getResponseBuffer(0x04) / 100.0f;
    dtostrf(batt_chargelimit, 2, 3, buf);
    client.publish("EPSolar/1/batt_chargelimit", buf);

    batt_overvoltrecon = node.getResponseBuffer(0x05) / 100.0f;
    dtostrf(batt_overvoltrecon, 2, 3, buf);
    client.publish("EPSolar/1/batt_overvoltrecon", buf);

    batt_equalvolt = node.getResponseBuffer(0x06) / 100.0f;
    dtostrf(batt_equalvolt, 2, 3, buf);
    client.publish("EPSolar/1/batt_equalvolt", buf);

    batt_boostvolt = node.getResponseBuffer(0x07) / 100.0f;
    dtostrf(batt_boostvolt, 2, 3, buf);
    client.publish("EPSolar/1/batt_boostvolt", buf);

    batt_floatvolt = node.getResponseBuffer(0x08) / 100.0f;
    dtostrf(batt_floatvolt, 2, 3, buf);
    client.publish("EPSolar/1/batt_floatvolt", buf);

    batt_boostrecon = node.getResponseBuffer(0x09) / 100.0f;
    dtostrf(batt_boostrecon, 2, 3, buf);
    client.publish("EPSolar/1/batt_boostrecon", buf);

    batt_lowvoltrecon = node.getResponseBuffer(0x0A) / 100.0f;
    dtostrf(batt_lowvoltrecon, 2, 3, buf);
    client.publish("EPSolar/1/batt_lowvoltrecon", buf);

    batt_undervoltrecon = node.getResponseBuffer(0x0B) / 100.0f;
    dtostrf(batt_undervoltrecon, 2, 3, buf);
    client.publish("EPSolar/1/batt_undervoltrecon", buf);

    batt_undervoltwarn = node.getResponseBuffer(0x0C) / 100.0f;
    dtostrf(batt_undervoltwarn, 2, 3, buf);
    client.publish("EPSolar/1/batt_undervoltwarn", buf);

    batt_lowvoltdisc = node.getResponseBuffer(0x0D) / 100.0f;
    dtostrf(batt_lowvoltdisc, 2, 3, buf);
    client.publish("EPSolar/1/batt_lowvoltdisc", buf);
  }
}

void loop() {
  ArduinoOTA.handle();
  client.loop();
  timer.run();
 WiFiClient c2 = server.available();
  if (c2) {
    
    if (c2.connected()) {
      Serial.println("Connected to client");
      c2.write("w");
    }

    // close the connection:
    c2.stop();
  }
  
}
