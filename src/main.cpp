
#include "FS.h"
#include "esp_system.h"
#include <esp_wifi.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include <stdint.h>
#include <math.h>
#include <EEPROM.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <ESP32WebServer.h>
#include <string.h>
#include <Preferences.h> // WiFi storage
#include "index.h" 

// Set your Static IP address
IPAddress local_IP(192, 168, 0, 90);
// Set your Gateway IP address
IPAddress gateway(192, 168, 0, 1);

IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 0, 1);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

const char* rssiSSID;        // NO MORE hard coded set AP, all SmartConfig
const char* password;
String PrefSSID, PrefPassword;   // used by preferences storage

/* change it with your ssid-password */
// //const char* ssid = "BORKAN1";
// const char* ssid = "AKKTUSTAKKI";
// const char* password = "mickeljunggrensnetwork";

int WFstatus;
int UpCount = 0;
int32_t rssi;         // store WiFi signal strength here
String getSsid;
String getPass;
String MAC;

// SSID storage
  Preferences preferences;       // declare class object
// END SSID storage

/* DEVELOPMENT */
#define idx_on_off 32
#define idx_man_auto 33
#define idx_set_temp  31
#define idx_act_temp 73
#define idx_heater  37
#define idx_filter  35
#define idx_ozone  36
#define idx_setpoint  34
#define idx_bubble  38

/* STUGAN */
//#define idx_on_off 100
//#define idx_man_auto 106
//#define idx_set_temp  107
//#define idx_act_temp 73
//#define idx_heater 104
//#define idx_ozone 101
//#define idx_setpoint 34
//#define idx_bubble 103

#define spa_ctrl_serial 0
#define spa_main_serial 1

String power = "off";
String heater = "off";
String bubble = "off";
String ozone = "off";
String last_power = "off";
String last_heater = "off";
String startup_status = "off";

/* Function Declarations */
void spa_on_off(void);
void heater_on_off(void);
void set_temp(int);
void mode_auto(void);
void mode_manual(void);
void filter_on_off(void);
void o3_on_off(void);
void bubble_on(void);
void bubble_off(void);
void jet(void);
void start_sequence(void);
void handleNotFound(void);
void WiFiReset(void);
void receivedCallback(char* topic, byte* payload, unsigned int length);
void mqttconnect(void);
void setup(void);
String createJsonResponse(void);
void handleRoot(void);
void updateStatus(void);
void handleTemp(void);
void handlePower(void);
void handleHeater(void);
void set_temp(int set_temp);
void update_selector(int, int);
void IP_info(void);
void wifiInit(void);
bool checkPrefsStore(void);
void initSmartConfig(void);
String getSsidPass(String);
String getMacAddress(void);
void IP_info(void);
int32_t getRSSI(const char*);
int getWifiStatus(int);
String getMacAddress(void);

ESP32WebServer server(80);

/* this is the IP of PC/raspberry where you installed MQTT Server
on Wins use "ipconfig"
on Linux use "ifconfig" to get its IP address */
const char* mqtt_server = "192.168.0.51";

int temperature = 38;
int act_temp = 0;
int difftemp = 0;


/* EEPROM adress */
//int temp_addr = 0;

/* create an instance of PubSubClient client */
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

/*DB GPIO pin*/
const char DB0 = 25; //ON_OFF
const char DB1 = 32; //HEATER
const char DB2 = 27; //TEMP UPP
const char DB3 = 26; //TEMP NER
const char DB4 = 33; //FILTER
const char DB5 = 18; //O3
const char DB6 = 19; //BUBBLE
const char DB7 = 21; //HEATER?

/* topics */
#define DOMOTICZ_OUT    "domoticz/out"
#define DOMOTICZ_IN     "domoticz/in"

#define EEPROM_SIZE 64

byte ctrl_startByte, ctrl_statusByte1, ctrl_statusSeq;
byte main_startByte, main_statusByte1, main_statusSeq;

/* Debug Serial*/
HardwareSerial Main_debug(1); //GPIO 2 TxD, GPIO 4 RxD
HardwareSerial Ctrl_debug(2); //GPIO 17 TxD, GPIO 16 RxD

long lastMsg = 0;

void receivedCallback(char* topic, byte* payload, unsigned int length) {
	Serial.print("Message received: ");
	Serial.println(topic);

	Serial.print("payload: ");
	for (int i = 0; i < length; i++) {
		Serial.print((char)payload[i]);
	}
	Serial.println();

	switch ((char)payload[0]) {
	case '0': // SPA ON/OFF
		spa_on_off();
		break;
	case '1': // HEATER ON/OFF
		heater_on_off();
		break;
	case '2': // TEMP -> 20 GRADER
		set_temp(20);
		break;
	case '3': // TEMP -> 38 GRADER
		set_temp(38);
		break;
	case '4': // TEMP -> 40 GRADER
		set_temp(40);
		break;
	case '5': // AUTO
		mode_auto();
		break;
	case '6': // MANUAL
		mode_manual();
		break;
	case '7': // FILTER
		filter_on_off();
		break;
	case '8': // O3
		o3_on_off();
		break;
	case '9': // BUBBLE ON
		bubble_on();
		break;
	case 'B': // BUBBLE OFF
		bubble_off();
		break;
	case 'J': // JET
		jet();
		break;
	case 'S': // START
		start_sequence();
		break;
	}

}

void handleNotFound() {
	String message = "File Not Found\n\n";
	server.send(404, "text/plain", message);
}

void WiFiReset() {
	WiFi.persistent(false);
	WiFi.disconnect();
	WiFi.mode(WIFI_OFF);
	WiFi.mode(WIFI_STA);
}

void mqttconnect() {
	/* Loop until reconnected */
	while (!mqtt_client.connected()) {
		Serial.print("MQTT connecting ...");
		/* client ID */
		String mqtt_clientId = "ESP32Client";
		/* connect now */
		if (mqtt_client.connect(mqtt_clientId.c_str())) {
			Serial.println("connected");
			/* subscribe topic with default QoS 0*/
			mqtt_client.subscribe(DOMOTICZ_OUT);
		}
		else {
			Serial.print("failed, status code =");
			Serial.print(mqtt_client.state());
			Serial.println("try again in 5 seconds");
			/* Wait 5 seconds before retrying */
			delay(10000);
		}
	}
}

void setup() {
	Serial.begin(115200);
	Ctrl_debug.begin(9708, SERIAL_8N1, 16, 17); //Initialize Ctrl debug
	Main_debug.begin(9708, SERIAL_8N1, 4, 2); //Initialize Main debug

    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
    }

///* initialize EEPROM */
//if (!EEPROM.begin(EEPROM_SIZE))
//{
//	Serial.println("failed to initialise EEPROM"); delay(1000);
//}
//EEPROM.write(temp_addr, byte(temperature));
//temperature = EEPROM.read(temp_addr);
//Serial.println("Lagrad Temperatur");
//Serial.println(temperature);

  Serial.printf("\tWiFi Setup -- \n" ); 
  wifiInit();      // get WiFi connected
  IP_info();
  MAC = getMacAddress();

  delay(2000);  // let thing settle
	Serial.println("");
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
	if (MDNS.begin("esp32")) {
		Serial.println("MDNS responder started");
	}

	server.on("/", handleRoot);
	server.on("/status.json", updateStatus);
	server.on("/setTemp", handleTemp);
	server.on("/setPower", handlePower);
	server.on("/setHeater", handleHeater);

	server.onNotFound(handleNotFound);

	server.begin();
	Serial.println("HTTP server started");

	/* configure the MQTT server with IPaddress and port */
	mqtt_client.setServer(mqtt_server, 1883);
	/* this receivedCallback function will be invoked when client received subscribed topic */
	mqtt_client.setCallback(receivedCallback);

	/*Connect to MQTT*/
	if (!mqtt_client.connected()) {
		mqttconnect();
	}
}

String createJsonResponse() {
	StaticJsonDocument<500> JSONbuffer;

	JsonObject root = JSONbuffer.to<JsonObject>();
	JsonArray type = root.createNestedArray("type");
	type.add("success");
	JsonArray code = root.createNestedArray("code");
	code.add(200);
	JsonArray power = root.createNestedArray("power");
	power.add(power);
	JsonArray heater = root.createNestedArray("heater");
	heater.add(heater);
	JsonArray temp = root.createNestedArray("temp");
	temp.add(act_temp);

	String json;
	json = serializeJsonPretty(JSONbuffer, Serial);
	return json;
}

void handleRoot() {
	String s = MAIN_page; //Read HTML contents
	server.send(200, "text/html", s); //Send web page
}

void updateStatus() {
	String json = "{\"p\":\"" + String(power) + "\",";
	json += "\"h\":\"" + String(heater) + "\",";
	json += "\"at\":\"" + String(act_temp) + "\",";
	json += "\"st\":\"" + String(temperature) + "\"}";
	server.send(200, "application/json", json);
	Serial.println("Status Update Sent");
}

void handleTemp() {
	String temp = server.arg("Temperature");
	int new_temp = temp.toInt();
	Serial.print("New Temp Received ");
	Serial.println(new_temp);
	set_temp(new_temp);
	server.send(200, "text/html", "Temperature");
}

void handlePower() {
	String t_state = server.arg("PowerState");
	if (t_state == "1" && power == "off")
	{
		spa_on_off();
		power = "on";
	}
	else
	{
		spa_on_off();
		power = "off";
	}
	server.send(200, "text/plane", "PowerState");
}

void handleHeater() {
	String h_state = server.arg("HeaterState");
	if (h_state == "1" && heater == "off")
	{
		heater_on_off();
		heater = "on";
	}
	else
	{
		heater_on_off();
		heater = "off";
	}
	server.send(200, "text/plane", "HeaterState");
}

void mode_manual() {
	pinMode(DB0, INPUT);
	pinMode(DB1, INPUT);
	pinMode(DB2, INPUT);
	pinMode(DB3, INPUT);
	pinMode(DB4, INPUT);
	pinMode(DB5, INPUT);
	pinMode(DB6, INPUT);
	pinMode(DB7, INPUT);
}

void mode_auto() {
	pinMode(DB0, OUTPUT);
	pinMode(DB1, OUTPUT);
	pinMode(DB2, OUTPUT);
	pinMode(DB3, OUTPUT);
	pinMode(DB4, OUTPUT);
	pinMode(DB5, OUTPUT);
	pinMode(DB6, OUTPUT);
	pinMode(DB7, OUTPUT);
	digitalWrite(DB0, HIGH);
	digitalWrite(DB1, HIGH);
	digitalWrite(DB2, HIGH);
	digitalWrite(DB3, HIGH);
	digitalWrite(DB4, HIGH);
	digitalWrite(DB5, HIGH);
	digitalWrite(DB6, HIGH);
	digitalWrite(DB7, HIGH);
}

void spa_on_off() {
	digitalWrite(DB0, LOW);
	delay(500);
	digitalWrite(DB0, HIGH);
}

void heater_on_off() {
	digitalWrite(DB1, LOW);
	delay(500);
	digitalWrite(DB1, HIGH);
}

void jet() {
	digitalWrite(DB7, LOW);
	delay(500);
	digitalWrite(DB7, HIGH);
}

void filter_on_off() {
	digitalWrite(DB4, LOW);
	delay(500);
	digitalWrite(DB4, HIGH);
}

void o3_on_off() {
	digitalWrite(DB5, LOW);
	delay(500);
	digitalWrite(DB5, HIGH);
}

void bubble_on() {
	digitalWrite(DB6, LOW);
	delay(500);
	digitalWrite(DB6, HIGH);
}

void bubble_off() {
	digitalWrite(DB6, LOW);
	delay(2000);
	digitalWrite(DB6, HIGH);
}

void set_temp(int set_temp) {
	void temp_down(int);
	void temp_up(int);
	int difftemp = (set_temp - temperature); // Negativ difftemp -> minska temp, positiv difftemp -> öka temp
	Serial.println("Tempskillnad: ");
	Serial.println(abs(difftemp));
	if (difftemp<0) {
		temp_down(abs(difftemp));
	}
	else if (difftemp>0) {
		temp_up(abs(difftemp));
	}
	else {

	}
	temperature = set_temp;
	// EEPROM.write(temp_addr, byte(set_temp));
}

void temp_up(int difftemp) {
	Serial.println("Temp Upp");
	Serial.println(difftemp);
	for (int i = -1;i < difftemp;i++) {
		digitalWrite(DB2, LOW);
		delay(500);
		digitalWrite(DB2, HIGH);
		delay(500);
	}
}

void temp_down(int difftemp) {
	Serial.println("Temp Ner");
	Serial.println(difftemp);
	for (int i = -1;i < difftemp;i++) {
		digitalWrite(DB3, LOW);
		delay(500);
		digitalWrite(DB3, HIGH);
		delay(500);
	}
}

void wifiInit() //
{
  WiFi.mode(WIFI_AP_STA); // required to read NVR before WiFi.begin()

  // load credentials from NVR, a little RTOS code here
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);     // load wifi settings to struct comf
  rssiSSID = reinterpret_cast<const char*>(conf.sta.ssid);
  password = reinterpret_cast<const char*>(conf.sta.password);

  //  Serial.printf( "SSID = %s\n", rssiSSID );  // un-comment for debuging
  //  Serial.printf( "Pass = %s\n", password );  // un-comment for debuging
  // Open Preferences with wifi namespace. Namespace is limited to 15 chars
  preferences.begin("wifi", false);
    PrefSSID     = preferences.getString("ssid", "none"); //NVS key ssid
    PrefPassword = preferences.getString("password", "none"); //NVS key password
  preferences.end();

  // keep from rewriting flash if not needed
  if( !checkPrefsStore() )   // see is NV and Prefs are the same
  {                          // not the same, setup with SmartConfig
    if( PrefSSID == "none" ) // New...setup wifi
    {
      initSmartConfig();
      delay( 3000);
      ESP.restart(); // reboot with wifi configured
    }
  }

  // I flash LEDs while connecting here

  WiFi.begin( PrefSSID.c_str() , PrefPassword.c_str() );

  int WLcount = 0;
  while (WiFi.status() != WL_CONNECTED && WLcount < 200 ) // can take > 100 loops depending on router settings
  {
    delay( 100 );
    Serial.printf(".");
    ++WLcount;
  }
  delay( 3000 );

  // stop the led flasher here

} // END wifiInit()

// match WiFi IDs in NVS to Pref store, assumes WiFi.mode(WIFI_AP_STA); was executed
bool checkPrefsStore()
{
  bool val = false;
  String NVssid, NVpass, prefssid, prefpass;

  NVssid = getSsidPass( "ssid" );
  NVpass = getSsidPass( "pass" );

  // Open Preferences with my-app namespace. Namespace name is limited to 15 chars
  preferences.begin("wifi", false);
    prefssid  =  preferences.getString("ssid",     "none"); //NVS key ssid
    prefpass  =  preferences.getString("password", "none"); //NVS key password
  preferences.end();

  if( NVssid.equals(prefssid) && NVpass.equals(prefpass) )
  { 
    val = true; 
  }
  return val;
} // END checkPrefsStore()

void initSmartConfig()
{
  // start LED flasher here
  int loopCounter = 0;

  WiFi.mode( WIFI_AP_STA );     //Init WiFi, start SmartConfig
  Serial.printf( "Entering SmartConfig\n" );

  WiFi.beginSmartConfig();

  while (!WiFi.smartConfigDone())
  { // flash led to indicate not configured
    Serial.printf( "." );
    if( loopCounter >= 40 )
    {
      loopCounter = 0;
      Serial.printf( "\n" );
    }
    delay(600);
    ++loopCounter;
  }
  loopCounter = 0;

  // stopped flasher here

  Serial.printf("\nSmartConfig received.\n Waiting for WiFi\n\n");
  delay(2000 );

  while( WiFi.status() != WL_CONNECTED )
  { // check till connected
     delay(500);
  }
  IP_info();

  preferences.begin("wifi", false); // put it in storage
    preferences.putString( "ssid" ,    getSsid);
    preferences.putString( "password", getPass);
  preferences.end();

  delay(300);
} // END SmartConfig()

void IP_info()
{
  getSsid = WiFi.SSID();
  getPass = WiFi.psk();
  rssi = getRSSI( rssiSSID );
  WFstatus = getWifiStatus( WFstatus );
  MAC = getMacAddress();

  Serial.printf( "\n\n\tSSID\t%s, ", getSsid.c_str() );
  Serial.print( rssi);  Serial.printf(" dBm\n" );  // printf??
  Serial.printf( "\tPass:\t %s\n", getPass.c_str() ); 
  Serial.print( "\n\n\tIP address:\t" );  Serial.print(WiFi.localIP() );
  Serial.print( " / " );              Serial.println( WiFi.subnetMask() );
  Serial.print( "\tGateway IP:\t" );  Serial.println( WiFi.gatewayIP() );
  Serial.print( "\t1st DNS:\t" );     Serial.println( WiFi.dnsIP() );
  Serial.printf( "\tMAC:\t\t%s\n", MAC.c_str() );
}  // END IP_info()

int getWifiStatus( int WiFiStatus )
{
  WiFiStatus = WiFi.status();
  Serial.printf("\tStatus %d", WiFiStatus );
  switch( WiFiStatus )
  {
    case WL_IDLE_STATUS : // WL_IDLE_STATUS = 0,
        Serial.printf(", WiFi IDLE \n");
        break;
    case WL_NO_SSID_AVAIL: // WL_NO_SSID_AVAIL = 1,
        Serial.printf(", NO SSID AVAIL \n");
        break;
    case WL_SCAN_COMPLETED: // WL_SCAN_COMPLETED = 2,
        Serial.printf(", WiFi SCAN_COMPLETED \n");
        break;
    case WL_CONNECTED: // WL_CONNECTED = 3,
        Serial.printf(", WiFi CONNECTED \n");
        break;
    case WL_CONNECT_FAILED: // WL_CONNECT_FAILED = 4,
        Serial.printf(", WiFi WL_CONNECT FAILED\n");
        break;
    case WL_CONNECTION_LOST: // WL_CONNECTION_LOST = 5,
        Serial.printf(", WiFi CONNECTION LOST\n");
        WiFi.persistent(false); // don't write FLASH
        break;
    case WL_DISCONNECTED: // WL_DISCONNECTED = 6
        Serial.printf(", WiFi DISCONNECTED ==\n");
        WiFi.persistent(false); // don't write FLASH when reconnecting
        break;
  }
  return WiFiStatus;
}  // END getWifiStatus()

// Get the station interface MAC address.
// @return String MAC
String getMacAddress(void)
{
  WiFi.mode(WIFI_AP_STA); // required to read NVR before WiFi.begin()
  uint8_t baseMac[6];
  esp_read_mac( baseMac, ESP_MAC_WIFI_STA ); // Get MAC address for WiFi station
  char macStr[18] = { 0 };
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  return String(macStr);
}  // END getMacAddress()

// Return RSSI or 0 if target SSID not found
// const char* SSID = "YOUR_SSID"; // declare in GLOBAL space
// call: int32_t rssi = getRSSI(SSID);
int32_t getRSSI( const char* target_ssid )
{
  byte available_networks = WiFi.scanNetworks();

  for (int network = 0; network < available_networks; network++)
  {
    if ( strcmp( WiFi.SSID( network).c_str(), target_ssid ) == 0)
    {
      return WiFi.RSSI( network );
    }
  }
  return 0;
} // END getRSSI()

// Requires; #include <esp_wifi.h>
// Returns String NONE, ssid or pass arcording to request
// ie String var = getSsidPass( "pass" );
String getSsidPass( String s )
{
  String val = "NONE"; // return "NONE" if wrong key sent
  s.toUpperCase();
  if( s.compareTo("SSID") == 0 )
  {
    wifi_config_t conf;
    esp_wifi_get_config( WIFI_IF_STA, &conf );
    val = String( reinterpret_cast<const char*>(conf.sta.ssid) );
  }
  if( s.compareTo("PASS") == 0 )
  {
    wifi_config_t conf;
    esp_wifi_get_config( WIFI_IF_STA, &conf );
    val = String( reinterpret_cast<const char*>(conf.sta.password) );
  }
  return val;
}  // END getSsidPass()

void start_sequence() {
	startup_status = "on";
	Serial.println("Startar SPA");
	//update_log("SPA startar!");
	//mode_auto();
	//update_log("SPA Auto mode set");
	delay(500);
	//update_selector(idx_man_auto, 10); // Set mode auto
	delay(500);
	//update_switch(idx_ozone, 0); // Set O3 to OFF in Domoticz
	delay(500);
	//update_switch(idx_bubble, 0); // Set bubble to OFF in Domoticz
	delay(500);
	//update_selector(idx_set_temp, 20); // Set start temp 38 degrees in Domoticz
	delay(500);
	//heater_on_off(); //Switch heater ON
	//delay(500);
	//update_switch(idx_heater, 1); // Set heater to ON in Domoticz
}

void update_switch(int idx, int nvalue)
{

}

void update_temp(int idx, int nvalue, int svalue)
{

}

void update_log(char message[50])
{

}

void loop() {

if ( WiFi.status() == WL_CONNECTED )
  {   	// Main connected loop
  		// ANY MAIN LOOP CODE HERE

	mqtt_client.loop();     // internal household function for MQTT

	if (startup_status == "off")
		start_sequence();
	else {}
	if (power == "on" && last_power != "on") {
		update_switch(idx_on_off, 1); // Set SPA to On in Domoticz
		last_power = "on";
	}
	else if (power == "off" && last_power != "off") {
		update_switch(idx_on_off, 0); // Set SPA to Off in Domoticz
		last_power = "off";
	}
	if (heater == "on" && last_heater != "on") {
		update_switch(idx_heater, 1); // Set SPA to On in Domoticz
		last_heater = "on";
	}
	else if (heater == "off" && last_heater != "off") {
		update_switch(idx_heater, 0); // Set SPA to Off in Domoticz
		last_heater = "off";
	}

	server.handleClient();

	/* SERIAL RECEIVE FROM SPA KEYBOARD UNIT */
	if (spa_ctrl_serial == 1){
		if (Ctrl_debug.available()>0) { // there are bytes in the serial buffer to read
			ctrl_startByte = Ctrl_debug.read(); // read in the next byte
			if (ctrl_startByte == 0xA5) { //startOfMessage
				Serial.println("*** RECEIVED FROM CTRL ***");
				ctrl_statusSeq = Ctrl_debug.read();
				Serial.println(ctrl_statusSeq, HEX);
				ctrl_statusByte1 = Ctrl_debug.read();
				Serial.println(ctrl_statusByte1, HEX);

				switch (ctrl_statusSeq) {
				case 1: // SPA ON/OFF
					if (ctrl_statusByte1 == 0)
					{
						Serial.println("SPA Off");
						power = "off";
					}
					else if (ctrl_statusByte1 == 1)
					{
						Serial.println("SPA On");
						power = "on";
					}
					break;
				case 2: // SEKVENS 2
					Serial.print("Sekvens 2, Value: ");
					Serial.println(ctrl_statusByte1, HEX);
					break;
				case 3: // HEATER ON/OFF
					if (ctrl_statusByte1 == 0)
					{
						Serial.println("Heater Off");
						heater = "off";
					}
					else if (ctrl_statusByte1 == 1)
					{
						Serial.println("Heater On");
						heater = "on";
					}
					break;
				case 4: // SEKVENS 4
					Serial.print("Sekvens 4, Value: ");
					Serial.println(ctrl_statusByte1, HEX);
					break;
				case 5: // TEMP
					Serial.print("Ny Temp ");
					Serial.println(ctrl_statusByte1, DEC);
					break;
				case 10: // SEKVENS 10
					Serial.print("Sekvens 10, Value: ");
					Serial.println(ctrl_statusByte1, HEX);
					break;
				case 11: // SEKVENS 11
					Serial.print("Sekvens 11, Value: ");
					Serial.println(ctrl_statusByte1, HEX);
					break;
				}
			}
			else {}
		}
	}
	/* SERIAL RECEIVE FROM SPA MAIN UNIT */
	if (spa_main_serial == 1){
		if (Main_debug.available()>0) { // there are bytes in the serial buffer to read
			main_startByte = Main_debug.read(); // read in the next byte
			if (main_startByte == 0xA5) { //startOfMessage
				Serial.println("*** RECEIVED FROM MAIN ***");
				main_startByte = Main_debug.read();
				Serial.println(main_statusSeq, HEX);
				main_statusByte1 = Main_debug.read();
				Serial.println(main_statusByte1, HEX);

				switch (main_statusSeq) {
				case 6: // ACTUAL TEMP
					Serial.print("Actual Temp: ");
					Serial.println(main_statusByte1, DEC);
					act_temp = (int) main_statusByte1;
					update_temp(idx_act_temp, 0, act_temp);
					break;
				case 7: // SEKVENS 7
					Serial.print("Sekvens 7, Value: ");
					Serial.println(main_statusByte1, HEX);
					break;
				case 8: // SEKVENS 8
					Serial.print("Sekvens 8, Value: ");
					Serial.println(main_statusByte1, HEX);
					break;
				case 9: // SEKVENS 9
					Serial.print("Sekvens 9, Value: ");
					Serial.println(main_statusByte1, HEX);
					break;
				case 12: // SEKVENS 12
					Serial.print("Sekvens 12, Value: ");
					Serial.println(main_statusByte1, DEC);
					break;
				case 13: // SEKVENS 13
					Serial.print("Sekvens 13, Value: ");
					Serial.println(main_statusByte1, HEX);
					break;
				case 14: // SEKVENS 14
					Serial.print("Sekvens 14, Value: ");
					Serial.println(main_statusByte1, HEX);
					break;
				}
			}
			else {}
	}
}

  }   // END Main connected loop()
  else
  {      // WiFi DOWN

    //  wifi down start LED flasher here

    WFstatus = getWifiStatus( WFstatus );
    WiFi.begin(  PrefSSID.c_str() , PrefPassword.c_str() );
    int WLcount = 0;
    while (  WiFi.status() != WL_CONNECTED && WLcount < 200 )
    {
      delay( 100 );
      Serial.printf(".");

        if (UpCount >= 60)  // keep from scrolling sideways forever
        {
           UpCount = 0;
           Serial.printf("\n");
        }
        ++UpCount;
        ++WLcount;
    }

    if( getWifiStatus( WFstatus ) == 3 )   //wifi returns
    { 
      // stop LED flasher, wifi going up
    }
   delay( 1000 );
  } // END WiFi down

} // END Loop()

