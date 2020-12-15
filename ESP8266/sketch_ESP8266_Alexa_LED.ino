#include <ESP8266WiFi.h>

// AWS
#include "sha256.h"
#include "Utils.h"

// WEBSockets
#include <Hash.h>
#include <WebSocketsClient.h>

// MQTT PubSubClient lib
#include <PubSubClient.h>

// AWS MQTT Websocket
#include "Client.h"
#include "AWSWebSocketClient.h"
#include "CircularByteBuffer.h"

// JSON
#include "Arduino_JSON.h"


// AWS root certificate - expires 2037
const char ca[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIEkjCCA3qgAwIBAgITBn+USionzfP6wq4rAfkI7rnExjANBgkqhkiG9w0BAQsF
ADCBmDELMAkGA1UEBhMCVVMxEDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNj
b3R0c2RhbGUxJTAjBgNVBAoTHFN0YXJmaWVsZCBUZWNobm9sb2dpZXMsIEluYy4x
OzA5BgNVBAMTMlN0YXJmaWVsZCBTZXJ2aWNlcyBSb290IENlcnRpZmljYXRlIEF1
dGhvcml0eSAtIEcyMB4XDTE1MDUyNTEyMDAwMFoXDTM3MTIzMTAxMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaOCATEwggEtMA8GA1UdEwEB/wQFMAMBAf8wDgYDVR0PAQH/
BAQDAgGGMB0GA1UdDgQWBBSEGMyFNOy8DJSULghZnMeyEE4KCDAfBgNVHSMEGDAW
gBScXwDfqgHXMCs4iKK4bUqc8hGRgzB4BggrBgEFBQcBAQRsMGowLgYIKwYBBQUH
MAGGImh0dHA6Ly9vY3NwLnJvb3RnMi5hbWF6b250cnVzdC5jb20wOAYIKwYBBQUH
MAKGLGh0dHA6Ly9jcnQucm9vdGcyLmFtYXpvbnRydXN0LmNvbS9yb290ZzIuY2Vy
MD0GA1UdHwQ2MDQwMqAwoC6GLGh0dHA6Ly9jcmwucm9vdGcyLmFtYXpvbnRydXN0
LmNvbS9yb290ZzIuY3JsMBEGA1UdIAQKMAgwBgYEVR0gADANBgkqhkiG9w0BAQsF
AAOCAQEAYjdCXLwQtT6LLOkMm2xF4gcAevnFWAu5CIw+7bMlPLVvUOTNNWqnkzSW
MiGpSESrnO09tKpzbeR/FoCJbM8oAxiDR3mjEH4wW6w7sGDgd9QIpuEdfF7Au/ma
eyKdpwAJfqxGF4PcnCZXmTA5YpaP7dreqsXMGz7KQ2hsVxa81Q4gLv7/wmpdLqBK
bRRYh5TmOTFffHPLkIhqhBGWJ6bt2YFGpn6jcgAKUj6DiAdjd4lpFw85hdKrCEVN
0FE6/V1dN2RMfjCyVSRCnTawXZwXgWHxyvkQAiSr6w10kY17RSlQOYiypok1JR4U
akcjMS9cmvqtmg5iUaQqqcT5NJ0hGA==
-----END CERTIFICATE-----
)EOF";


// AWS IoT Core config
const char *aws_iam_key = "XXXXXXXXXXXXXXXXXXXX";
const char *aws_iam_secret_key = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
const char *aws_endpoint = "xxxxxxxxxxxxxxxxxx.iot.us-west-2.amazonaws.com";
const char *aws_region = "us-west-2";
const int aws_port = 443; // Important !

const char *aws_topic_shadow = "$aws/things/ESP8266/shadow/update";
const char *aws_topic_shadow_delta = "$aws/things/ESP8266/shadow/update/delta";


// MQTT config
const int maxMQTTpackageSize = 512;
const int maxMQTTMessageHandlers = 1;


AWSWebSocketClient awsWSclient(1000);
PubSubClient mqttClient(awsWSclient);


// WiFi config
#define WIFI_SSID "XXXXXXXXXXXXX"
#define WIFI_PASS "xxxxxxxxxxxxx"

// LED Pin #
#define LED 4



void setup() {
  
  /*** Serial Initialization ***/
  Serial.begin(115200);

  /*** GPIO Initialization ***/
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  /*** WiFi Initialization ***/
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.println();
  Serial.print("Waiting for WiFi connection.");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" Finished !");

  delay(100);

  /*** NTP Time sync to vaidate ca ***/
  syncTime();
  
  /*** AWS IoT Core Initialization ***/
  awsWSclient.setAWSRegion(aws_region);
  awsWSclient.setAWSDomain(aws_endpoint);
  awsWSclient.setAWSKeyID(aws_iam_key);
  awsWSclient.setAWSSecretKey(aws_iam_secret_key);
  awsWSclient.setUseSSL(true);
  awsWSclient.setCA(ca);
  awsWSclient.setUseAmazonTimestamp(false);

  connectAWSIoTCore();
  
  delay(500);

  /*** Report Initial LED State ***/
  reportLedState(false);
  
}


void loop() {

  if ( mqttClient.connected() ) {
    mqttClient.loop (); // Important !
  } else {
    connectAWSIoTCore();
  }

}


void syncTime() {
  
  configTzTime("JST-9", "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
  
  Serial.print("Waiting for NTP Time sync.");
  time_t t = time(NULL);
  while (t < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    t = time(NULL);
  }
  Serial.println(" Finished !");
  struct tm *timeinfo = localtime(&t);
  Serial.println( asctime(timeinfo) );
  
}


void connectAWSIoTCore() {
  
  if ( mqttClient.connected() ) {
    mqttClient.disconnect();
  }
  
  Serial.print("Waiting for AWS IoT Core connection.");

  delay(10000); // Important ! To Avoid Exception (9)
  
  mqttClient.setServer(aws_endpoint, aws_port);
  if ( ! mqttClient.connect("HogeHoge") ){
    Serial.println(" Failed !");
    return;
  }
  Serial.println(" Finished !");

  mqttClient.setCallback(mqttCallback);
  mqttClient.subscribe(aws_topic_shadow_delta);
  Serial.println("Subscribed !");
  
}


void mqttCallback(char *topic, byte *payload, unsigned int length) {

  if (strcmp(topic, aws_topic_shadow_delta) == 0) {
    // Parse received message (JSON Format)
    JSONVar json = JSON.parse( (char *)payload );
    Serial.println( JSON.stringify(json) );
    JSONVar state = json["state"];

    if ( state.hasOwnProperty("led") ){
      // Obtain desired LED state
      bool led = state["led"];
      // LED ON/OFF
      digitalWrite(LED, led ? HIGH : LOW);
      // Report current LED state
      reportLedState(led);
    }
  }

}


void reportLedState(bool led) {
  
  char buf[100];
  sprintf(buf, "{\"state\":{\"reported\":{\"led\": %s}}}", led ? "true" : "false");
  mqttClient.publish(aws_topic_shadow, buf);
  
}
