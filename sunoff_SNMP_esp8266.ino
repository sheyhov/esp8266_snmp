/*

  Sonoff switch MQTT

  Connects to MQTT server and accepts commands of 0 turn off, 1 turn on
  Updates off and on with button press and sends notice to server

  Mike Braden
  2018-feb-11

  v4

*/

#include <WiFiUdp.h> // Для SNMP
#include <Arduino_SNMP.h> // Для SNMP

#include <ESP8266WiFi.h>
#include <Bounce2.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager


#define CONNECT_MSG "sonoff02: Connected"
#define RELAY_PIN 12
#define LED_PIN 13
#define SWITCH_PIN 0

bool switchState = LOW;

long lastMsg = 0;
char msg[50];
int value = 0;

WiFiClient netClient;
Bounce debouncer = Bounce();

// function protoypes
void setupWifi();
void setupSNMP();
void loop();
void extButton();
void switchOn();
void switchOff();


// *snmp start
WiFiUDP udp;
SNMPAgent snmp = SNMPAgent("public");  // Starts an SMMPAgent instance with the community string 'public'

int changingNumber = 1;
int settableNumber1 = 0;
int tensOfMillisCounter = 0;

TimestampCallback* timestampCallbackOID;
// *snmp end


void setup() {
  pinMode(RELAY_PIN, OUTPUT);     // Set the relay pin as an output
  pinMode(SWITCH_PIN, INPUT);     // Set the button pin as an input
  pinMode(LED_PIN, OUTPUT);       // Set the led pin as an output

  Serial.begin(115200);

  //Serial.println("delay start");
  //delay(10000);                   // delay to allow catching start in serial monitor

  EEPROM.begin(512);              // Begin eeprom to store on/off state

  switchState = EEPROM.read(0);

  Serial.print("switchState from EEPROM: ");
  Serial.println(switchState);

  // update outputs directly, can't use swithon/off until mqtt is setup
  if (switchState == 0) {
    digitalWrite(LED_PIN, HIGH);  // Turn the LED off with high
    digitalWrite(RELAY_PIN, LOW);     // Turn the relay off (open) with low
  }
  else {
    digitalWrite(LED_PIN, LOW);   // Turn the LED on with low
    digitalWrite(RELAY_PIN, HIGH);    // Turn the relay on (closed) with high
  }

  debouncer.attach(SWITCH_PIN);   // Use the bounce2 library to debounce the built in button
  debouncer.interval(50);         // interval of 50 ms

  setupWifi();
  setupSNMP();

}

void setupSNMP() {
    // *snmp start
      // give snmp a pointer to the UDP object
    snmp.setUDP(&udp);
    snmp.begin();
   
    // add 'callback' for an OID - pointer to an integer
    snmp.addIntegerHandler(".1.3.6.1.4.1.5.0", &changingNumber);
   
    // Using your favourite snmp tool:
    // snmpget -v 1 -c public <IP> 1.3.6.1.4.1.5.0
    timestampCallbackOID = (TimestampCallback*)snmp.addTimestampHandler(".1.3.6.1.2.1.1.3.0", &tensOfMillisCounter);
    // you can accept SET commands with a pointer to an integer (or string)
    snmp.addIntegerHandler(".1.3.6.1.4.1.2566.10.2.2.1.20.0.2", &settableNumber1, true);
    //snmp.addIntegerHandler(".1.3.6.1.4.1.2566.10.2.2.1.20.0.9", &settableNumber2, true);
    //.1.3.6.1.4.1.2566.10.2.2.1.20.0.2
    //.1.3.6.1.4.1.2566.10.2.2.1.20.0.9
    // snmpset -v 1 -c public <IP> 1.3.6.1.4.1.5.0 i 99
  // *snmp end
}

void setupWifi() {

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //exit after config instead of connecting
  wifiManager.setBreakAfterConfig(true);

  //reset settings - for testing
  //wifiManager.resetSettings();


  //tries to connect to last known settings
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP" with password "password"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("AutoConnectAP", "123456789")) {
    Serial.println("failed to connect, we should reset as see if it connects");
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");


  Serial.println("local ip");
  Serial.println(WiFi.localIP());


  // once connected print status info - IP address, gateway, DNS, signal strength
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.printf("Subnet mask: %s\n", WiFi.subnetMask().toString().c_str());
  Serial.printf("Gataway IP: %s\n", WiFi.gatewayIP().toString().c_str());

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("RSSI:");
  Serial.println(rssi);

  Serial.printf("DNS 1 IP:  %s\n", WiFi.dnsIP(0).toString().c_str());
  Serial.printf("DNS 2 IP:  %s\n", WiFi.dnsIP(1).toString().c_str());

  Serial.println("SNMP-OID-SWITCH-RELAY: .1.3.6.1.4.1.2566.10.2.2.1.20.0.2");
}


//  // Switch the LED and relay if first character recieved is 0 or 1
//  if ((char)payload[0] == '0') {
//    mqttClient.publish(outTopic, "Switch command received");
//    switchOff();
//  } else if ((char)payload[0] == '1') {
//    mqttClient.publish(outTopic, "Switch command received");
//    switchOn();
//  }


void loop() {

  //snmp start
      snmp.loop(); // must be called as often as possible 
    if(snmp.setOccurred){
        if (settableNumber1==1) {
           Serial.printf("Number has been set to value: %i", settableNumber1);
           settableNumber1=1 ;
           switchOn();
//           digitalWrite(LED_BUILTIN, HIGH);
           Serial.printf("\nPower: %i", settableNumber1);
           } else {
           Serial.printf("Number has been set to value: %i", settableNumber1);
           settableNumber1=0 ;
           switchOff();
//           digitalWrite(LED_BUILTIN, LOW);
           Serial.printf("\nPower: %i", settableNumber1);
           }
        snmp.resetSetOccurred();
    }
    changingNumber++;
  //snmp end

  // update the button
  extButton();
  
}

void extButton() {
  debouncer.update();

  // Call code if Bounce fell (transition from HIGH to LOW) :
  if ( debouncer.fell() ) {
    Serial.println("Debouncer fell");
    // Toggle relay state :
    switchState = !switchState;

    Serial.print("switchState changed button press: ");
    Serial.println(switchState);

    if (switchState == 0) {
      switchOff();
    }
    else if (switchState == 1) {
      switchOn();
    }
  }
}

void switchOn() {
  // set the led, relay and save the setting to eeprom
  // state is switch state 0=low=off, 1=high=on
  Serial.println("switchOn");
  digitalWrite(LED_PIN, LOW);   // Turn the LED on with low
  digitalWrite(RELAY_PIN, HIGH);  // Turn the relay on (closed) with high
  switchState = HIGH;   // state is switch state 0=low=off, 1=high=on
  EEPROM.write(0, switchState);    // Write state to EEPROM addr 0
  EEPROM.commit();
}

void switchOff() {
  // set the led, relay and save the setting to eeprom
  // state is switch state 0=low=off, 1=high=on
  Serial.println("switchOff");
  digitalWrite(LED_PIN, HIGH);   // Turn the LED off with high
  digitalWrite(RELAY_PIN, LOW);  // Turn the relay off (open) with low
  switchState = LOW;   // state is switch state 0=low=off, 1=high=on
  EEPROM.write(0, switchState);    // Write state to EEPROM addr 0
  EEPROM.commit();
}
