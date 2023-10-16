#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>


// WIFI
const char* ssid = "EETECHF";
const char* password = "EETECHFLAT";
uint8_t newMACAddress[6] = {0x32, 0xAE, 0xA4, 0x07, 0x0D, 0x69}; //67 - 1   68 - 2
/*
IPAddress local_IP(192, 168, 10, 100);
IPAddress gateway(192, 168, 10, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional
*/

// web
HTTPClient http;

// MQTT
WiFiClient espClient;
PubSubClient client(espClient);
const char* mqtt_server = "192.168.10.2";  // 10.240.40.143
int mqtt_port = 9001;
long lastMsg = 0;
long debounceVal = 100;              // 1 - sec
char msg[50];
const char* deviceId = "iot/random";  //iot/random2 

// MITUTOYO
int req = D8;                         //D8; //mic REQ line goes to pin 5 through q1 (arduino high pulls request line low)
int dat = D6;                         //D6; //mic Data line goes to pin 2
int clk = D7;                         //D7; //mic Clock line goes to pin 3
int i = 0;
int j = 0;
int k = 0;
int signCh = 8;
int sign = 0;
int decimal;
float dpp;
int units;
byte mydata[14];
String value_str;
long value_int; //was an int, could not measure over 32mm
float value = 0;



// ================
// WIFI
// ================
void setup_wifi() 
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  /*
  // ip
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  */

  // connect
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Old MAC address: ");
  Serial.println(WiFi.macAddress());

  // mac
  wifi_set_macaddr(0, const_cast<uint8*>(newMACAddress));
  Serial.print("New MAC address: ");
  Serial.println(WiFi.macAddress());
}


// ================
// MQTT MSG RECEIVED
// ================
void callback(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}


// ================
// MQTT RECONNECT
// ================
void reconnect() 
{
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("MQTT connecting...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT connected");
      // Once connected, publish an announcement...
      //client.publish(deviceId, "hello world");
      // ... and resubscribe
      client.subscribe(deviceId);
    } else {
      Serial.print("MQTT failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


// ================
// ARDUINO - SETUP
// ================
void setup() 
{
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(9600);
  setup_wifi();
  //client.setServer(mqtt_server, mqtt_port);
  //client.setCallback(callback);

  pinMode(req, OUTPUT);
  pinMode(clk, INPUT_PULLUP);
  pinMode(dat, INPUT_PULLUP);
  digitalWrite(req,HIGH); // set request at high
  Serial.println(" Flatness starting... ");
}


// ================
// ARDUINO - LOOP
// ================
void loop() 
{
  //Serial.println("aaa");
  
  // MITUTOYO - SIGNAL ON
  digitalWrite(req, LOW); // generate set request


  // MITUTOYO - READ
  for( i = 0; i < 13; i++ ) 
  {
    k = 0;
    for (j = 0; j < 4; j++) {
      
      while( digitalRead(clk) == LOW) {
        yield();
      } // hold until clock is high
      
      while( digitalRead(clk) == HIGH) {
        yield();
      } // hold until clock is low
      
      bitWrite(k, j, (digitalRead(dat) & 0x1));
    }
  
    mydata[i] = k;
  }
  

  // MITUTOYO - LOGIC
  sign = mydata[4];
  value_str = String(mydata[5]) + String(mydata[6]) + String(mydata[7]) + String(mydata[8]) + String(mydata[9]) + String(mydata[10]);
  decimal = mydata[11];
  units = mydata[12];
  value_int = value_str.toInt();


  // MITUTOYO - DECIMAL
  if (decimal == 0) dpp = 1.0;
  if (decimal == 1) dpp = 10.0;
  if (decimal == 2) dpp = 100.0;
  if (decimal == 3) dpp = 1000.0;
  if (decimal == 4) dpp = 10000.0;
  if (decimal == 5) dpp = 100000.0;
  value = value_int / dpp;


  // MITUTOYO - SIGN
  Serial.print("Reading: ");
  if (sign == 0) {
    value = value * 1.0000;
    Serial.println(value,decimal);
  }
  if (sign == 8) {
    value = value * -1.0000;
    Serial.print("-"); Serial.println(value,decimal);
  }


  // MITUTOYO - SIGNAL OFF
  digitalWrite(req,HIGH);
  delay(100);


  /*
  // MQTT - SUBSCRIBE
  if (!client.connected()) 
  {
    Serial.println("MQTT Disconnected...");
    reconnect();
  }
  client.loop();
  */


  // MQTT - PUBLISH
  long now = millis();
  if (now - lastMsg > debounceVal) 
  {
    lastMsg = now;

    if (value < -32 || value > 32)
    {
      return;     
    }

    if (!isDigit(value)) {
      return;
    }
    
    if (http.begin(espClient, "http://192.168.10.2/module/production/wr.php?d=" + String(value, 4)))
    {
      int httpCoded = http.GET();
      //Serial.print("SENT: ");
      //Serial.println(httpCoded);
    }
    
    /*
    Serial.print("MQTT Publish message: ");
    Serial.println(value);
    char buffer[10];
    dtostrf(value, 5, 4, buffer);
    client.publish(deviceId, buffer);
    */
  }
}
