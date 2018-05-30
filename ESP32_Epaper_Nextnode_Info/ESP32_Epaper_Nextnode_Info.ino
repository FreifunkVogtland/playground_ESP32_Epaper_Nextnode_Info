#include <GxEPD.h>
#include <GxGDEP015OC1/GxGDEP015OC1.cpp>
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
#include "pf_tempesta_seven5pt7b.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Time.h>

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;

// Network information
const char* ssid = "vogtland.freifunk.net";
const char* password = "";

GxIO_Class io(SPI, SS, 22, 21);
GxEPD_Class display(io, 16, 4);

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

void setup() {

  Serial.begin(115200);
  delay(10);

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  /*
    First we configure the wake up source
    We set our ESP32 to wake up every 60 seconds
  */
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                 " Seconds");

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  display.init();

  const GFXfont* f = &pf_tempesta_seven5pt7b;

  //display.setRotation(45);
  display.setFont(f);
  display.setTextColor(GxEPD_BLACK);
  display.fillRect(0, 0, 200, 0, GxEPD_WHITE);
  display.setCursor(0, 7);

  String out = "";

  delay(3000);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // get current date/time
  struct tm timeinfo;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  if (!getLocalTime(&timeinfo))
  {
    Serial.println(F(" Failed to obtain time"));

    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
  }

  char s[29];
  strftime(s, sizeof(s), "Date/Time: %d-%m-%y %H:%M:%S", &timeinfo);
  out += String(s) + "\n";

  Serial.println(&timeinfo, " %d %B %Y %H:%M:%S ");
  Serial.println("");

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  out += "IP Address: " + ip.toString() + "\n";

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");

  out += "Signal Strength: " + String(rssi) + " dBm\n";

  Serial.println(F("Connecting..."));

  // Connect to HTTP server
  HTTPClient client;

  client.begin("http://nextnode.ffv/cgi-bin/nodeinfo");

  int httpCode = client.GET();

  String payload;

  if (httpCode == HTTP_CODE_OK) {
    payload = client.getString();
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", client.errorToString(httpCode).c_str());

    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
  }

  client.end();

  // Allocate JsonBuffer
  // Use arduinojson.org/assistant to compute the capacity.
  const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(3) + JSON_ARRAY_SIZE(5) + 5 * JSON_OBJECT_SIZE(1) + 6 * JSON_OBJECT_SIZE(2) + 2 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8) + 1024;
  DynamicJsonBuffer jsonBuffer(bufferSize);

  JsonObject& root = jsonBuffer.parseObject(payload);

  if (!root.success()) {
    Serial.println(F("Parsing failed!"));

    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
  }

  // Extract values

  JsonObject& software = root["software"];

  const char* software_autoupdater_branch = software["autoupdater"]["branch"]; // "stable"
  bool software_autoupdater_enabled = software["autoupdater"]["enabled"]; // true

  const char* software_batman_adv_version = software["batman-adv"]["version"]; // "2017.0"
  int software_batman_adv_compat = software["batman-adv"]["compat"]; // 15

  const char* software_fastd_version = software["fastd"]["version"]; // "v18"
  bool software_fastd_enabled = software["fastd"]["enabled"]; // false

  const char* software_firmware_base = software["firmware"]["base"]; // "gluon-v2016.2.4-2"
  const char* software_firmware_release = software["firmware"]["release"]; // "b20170420-v"

  int software_status_page_api = software["status-page"]["api"]; // 1

  JsonObject& network = root["network"];

  JsonArray& network_addresses = network["addresses"];
  const char* network_addresses0 = network_addresses[0]; // "2a03:2260:200f:300:ea94:f6ff:feab:5da4"
  const char* network_addresses1 = network_addresses[1]; // "2a03:2260:200f:1337:ea94:f6ff:feab:5da4"
  const char* network_addresses2 = network_addresses[2]; // "2a03:2260:200f:100:ea94:f6ff:feab:5da4"
  const char* network_addresses3 = network_addresses[3]; // "2a03:2260:200f:400:ea94:f6ff:feab:5da4"

  const char* network_mesh_bat0_interfaces_wireless0 = network["mesh"]["bat0"]["interfaces"]["wireless"][0]; // "fe:82:53:ac:25:5a"

  JsonArray& network_mesh_bat0_interfaces_other = network["mesh"]["bat0"]["interfaces"]["other"];
  const char* network_mesh_bat0_interfaces_other0 = network_mesh_bat0_interfaces_other[0]; // "fe:82:53:ac:25:5b"
  const char* network_mesh_bat0_interfaces_other1 = network_mesh_bat0_interfaces_other[1]; // "fe:82:53:ac:25:58"
  const char* network_mesh_bat0_interfaces_other2 = network_mesh_bat0_interfaces_other[2]; // "fe:82:53:ac:25:5c"

  const char* network_mac = network["mac"]; // "e8:94:f6:ab:5d:a4"

  JsonObject& location = root["location"];
  float location_latitude = location["latitude"]; // 50.49749
  float location_longitude = location["longitude"]; // 12.138742
  int location_altitude = location["altitude"]; // 360

  const char* owner_contact = root["owner"]["contact"]; // "andre.fiedler@me.com"

  const char* system_site_code = root["system"]["site_code"]; // "ffv"

  const char* node_id = root["node_id"]; // "e894f6ab5da4"
  const char* hostname = root["hostname"]; // "PL-Raedel7-Indoor"

  const char* hardware_model = root["hardware"]["model"]; // "TP-Link TL-WR841N/ND v8"
  int hardware_nproc = root["hardware"]["nproc"]; // 1

  Serial.println(hostname);

  client.begin("http://nextnode.ffv/cgi-bin/dyn/statistics");

  httpCode = client.GET();

  if (httpCode  == HTTP_CODE_OK) {
    // get lenght of document (is -1 when Server sends no Content-Length header)
    int len = client.getSize();

    // create buffer for read
    uint8_t buff[1024] = { 0 };

    // get tcp stream
    WiFiClient * stream = client.getStreamPtr();

    // read all data from server
    while (client.connected() && (len > 0 || len == -1)) {
      // get available data size
      size_t size = stream->available();

      if (size) {
        // read up to 128 byte
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

        // write it to Serial
        //Serial.write(buff, c);

        payload = (char*)buff;

        int indexOfFirstWhiteSpace = payload.indexOf(' ');
        payload = payload.substring(indexOfFirstWhiteSpace + 1);

        if (len > 0) {
          len -= c;
        }
        else if (payload.length() > 10) {
          // breakup streaming
          client.end();

          //Serial.println(payload);
        }
      }
      delay(1);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", client.errorToString(httpCode).c_str());

    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
  }

  const size_t bufferSize_statistics = 2 * JSON_ARRAY_SIZE(1) + 3 * JSON_OBJECT_SIZE(1) + 6 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + 2 * JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(10) + JSON_OBJECT_SIZE(14) + 840;
  DynamicJsonBuffer jsonBuffer_statistics(bufferSize_statistics);

  JsonObject& root_statistics = jsonBuffer_statistics.parseObject(payload);

  JsonObject& wireless0 = root_statistics["wireless"][0];
  int wireless0_frequency = wireless0["frequency"]; // 2462
  int wireless0_noise = wireless0["noise"]; // 161
  long wireless0_active = wireless0["active"]; // 556908822
  long wireless0_busy = wireless0["busy"]; // 90059369
  long wireless0_rx = wireless0["rx"]; // 53844672
  long wireless0_tx = wireless0["tx"]; // 14457884

  int txpower0_frequency = root_statistics["txpower"][0]["frequency"]; // 2462
  int txpower0_mbm = root_statistics["txpower"][0]["mbm"]; // 1200

  JsonObject& clients = root_statistics["clients"];
  int clients_total = clients["total"]; // 2
  int clients_wifi = clients["wifi"]; // 2
  int clients_wifi24 = clients["wifi24"]; // 2
  int clients_wifi5 = clients["wifi5"]; // 0

  JsonObject& traffic = root_statistics["traffic"];

  long traffic_rx_packets = traffic["rx"]["packets"]; // 33223401
  long traffic_rx_bytes = traffic["rx"]["bytes"]; // 4099463798

  JsonObject& traffic_tx = traffic["tx"];
  long traffic_tx_packets = traffic_tx["packets"]; // 1539469
  long traffic_tx_bytes = traffic_tx["bytes"]; // 422038381
  long traffic_tx_dropped = traffic_tx["dropped"]; // 4109

  long traffic_forward_packets = traffic["forward"]["packets"]; // 38527
  long traffic_forward_bytes = traffic["forward"]["bytes"]; // 17973244

  long traffic_mgmt_rx_packets = traffic["mgmt_rx"]["packets"]; // 7835744
  long traffic_mgmt_rx_bytes = traffic["mgmt_rx"]["bytes"]; // 2709139564

  long traffic_mgmt_tx_packets = traffic["mgmt_tx"]["packets"]; // 19635903
  long traffic_mgmt_tx_bytes = traffic["mgmt_tx"]["bytes"]; // 6990713014

  const char* gateway = root_statistics["gateway"]; // "02:62:e7:ab:05:05"
  const char* gateway_nexthop = root_statistics["gateway_nexthop"]; // "fe:82:53:ac:25:5a"

  JsonObject& mesh_vpn_groups_backbone_peers = root_statistics["mesh_vpn"]["groups"]["backbone"]["peers"];

  float rootfs_usage = root_statistics["rootfs_usage"]; // 0.0883

  JsonObject& memory = root_statistics["memory"];
  int memory_total = memory["total"]; // 28244
  int memory_free = memory["free"]; // 7212
  int memory_buffers = memory["buffers"]; // 1316
  int memory_cached = memory["cached"]; // 2896

  float uptime = root_statistics["uptime"]; // 558460.89
  float idletime = root_statistics["idletime"]; // 487351.17
  float loadavg = root_statistics["loadavg"]; // 0.3

  int processes_running = root_statistics["processes"]["running"]; // 1
  int processes_total = root_statistics["processes"]["total"]; // 45


  out += "Node ID: " + String(node_id) + "\n";
  out += "Name: " + String(hostname) + "\n";
  out += "Firmware: " + String(software_firmware_release) + "\n";
  out += "Hardware: " + String(hardware_model) + "\n";
  out += "Clients: " + String(clients_total) + " (2.4: " + String(clients_wifi24) + ", 5: " + String(clients_wifi5) + ")\n";
  out += "Traffic RX: " + prettyBytes(traffic_rx_bytes * -1) + "\n";
  out += "Traffic TX: " + prettyBytes(traffic_tx_bytes) + "\n";
  out += "Geo: " + String(location_latitude, 4) + "  " + String(location_longitude, 4) + "\n";
  out += "Owner: " + String(owner_contact) + "\n";

  display.print(out);
  display.update();

  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
}

void loop() {
  //This is not going to be called
}

void printLine(int line, String str) {

}

void showPartialUpdate(float temperature)
{
  /*
    String temperatureString = String(temperature,1);
    const char* name = "FreeSansBold24pt7b";
    const GFXfont* f = &FreeSansBold24pt7b;

    uint16_t 0 = 60;
    uint16_t 0 = 60;
    uint16_t 200 = 90;
    uint16_t 200 = 100;
    uint16_t cursor_y = 0 + 16;

    display.setRotation(45);
    display.setFont(f);
    display.setTextColor(GxEPD_BLACK);

    display.fillRect(0, 0, 200, 200, GxEPD_WHITE);
    display.setCursor(0, cursor_y+38);
    display.print(temperatureString);
    display.updateWindow(0, 0, 200, 200, true);
  */
}

String prettyBytes(long bytes) {
  char buf[128] = { 0 };
  int i = 0;
  const char* units[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
  while (bytes > 1024) {
    bytes /= 1024;
    i++;
  }
  sprintf(buf, "%.*f %s", i, (float)bytes, units[i]);
  return (char*)buf;
}


/*
  Method to print the reason by which ESP32
  has been awaken from sleep
*/
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
    case 1  : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case 3  : Serial.println("Wakeup caused by timer"); break;
    case 4  : Serial.println("Wakeup caused by touchpad"); break;
    case 5  : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.println("Wakeup was not caused by deep sleep"); break;
  }
}

