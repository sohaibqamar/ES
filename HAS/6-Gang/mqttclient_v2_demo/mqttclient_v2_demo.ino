#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#include <ESP8266WebServer.h>
#include <Ticker.h>
#include <WiFiUdp.h>
#include "helpers.h"
#include "global.h"
/*
Include the HTML, STYLE and Script "Pages"
*/
#include "Page_Root.h"
#include "Page_Admin.h"
#include "Page_Script.js.h"
#include "Page_Style.css.h"
#include "Page_Information.h"
#include "PAGE_NetworkConfiguration.h"

#define ACCESS_POINT_NAME  "ESP"				
#define ACCESS_POINT_PASSWORD  "12345678" 
#define AdminTimeOut 180 // Defines the Time in Seconds, when the Admin-Mode will be auto-disabled

const char* ssid = "newES";
const char* password = "intel123";
StaticJsonBuffer<200> jsonBuffer;

char* mqttMessage = "";

int button1 = 0;
int button2 = 0;
int fan = 0;
int button4 = 0;
int button5 = 0;
int dimmer = 1;
int current = 0;
float ampere = 0;

const char* mqtt_server = "34.208.3.118";
char localMacAddrBuff[18];

const int mqtt_port = 1883;

int b1 = 0;


const char *mqtt_client_name = "esp_client"; // Client connections cant have the same connection name

const char* pub_Topic_relay1 = "esp2866/relay1";
const char* pub_Topic_relay2 = "esp2866/relay2";
const char* pub_Topic_relay3 = "esp2866/relay3";
const char* pub_Topic_relay4 = "esp2866/relay4";
const char* pub_Topic_relay5 = "esp2866/relay5";
const char* pub_Topic_relay6 = "esp2866/relay6";
const char* pub_Topic_current = "esp2866/current";

const char* test_Pub = "$aws/things/339AJoharTown-98:98:98:98:98:98/shadow/update";
const char* test_sub = "$aws/things/339AJoharTown-98:98:98:98:98:98/shadow/update/accepted";
// Topic to  accept  (SUBSCRIBE) the state of  the shadow
const char* accept_device_state = "$aws/things/339AJoharTown-98:98:98:98:98:98/shadow/get/accepted";
// Topic to  request (PUBLISH) the state of  the shadow
const char* request_device_state = "$aws/things/339AJoharTown-98:98:98:98:98:98/shadow/get";

const char* sub_Topic_android1 = "android/button1";
const char* sub_Topic_android2 = "android/button2";
const char* sub_Topic_android3 = "android/button3";
const char* sub_Topic_android4 = "android/button4";
const char* sub_Topic_android5 = "android/button5";
const char* sub_Topic_android6 = "android/button6";

#define DEV_TYPE_1 0xD1
#define DEV_TYPE_7 0xD7
#define ID_BTN 0xF1
#define ID_FAN 0xF2
#define ID_CURRENT 0xF3
#define ID_WIFI 0xF4
#define ID_AP_MODE 0xF5
#define BTN_1 0xA0
#define BTN_2 0xA1
#define BTN_3 0xA2
#define BTN_4 0xA3
#define BTN_5 0xA4
#define BTN_6 0xA5
#define BTN_7 0xA6
#define BTN_8 0xA7

#define BTN_STATE_OFF 0x00
#define BTN_STATE_ON 0x01

#define FAN_STATE_OFF 0x00
#define FAN_STATE_ON 0x01
#define DIM_STATE_PLUS 0xB6
#define DIM_STATE_MINUS 0xB7

byte msg[13];         // a string to hold incoming data
volatile byte deviceType;
volatile byte ID;
volatile byte SeqNumber;
volatile byte btnState;

byte button_0_Off [] = {0x2A, 0x2A, 0x2A, 0xD1, 0xF1, 0xA0, 0x00};
byte button_0_On [] = {0x2A, 0x2A, 0x2A, 0xD1, 0xF1, 0xA0, 0x01};

byte button_1_Off [] = {0x2A, 0x2A, 0x2A, 0xD1, 0xF1, 0xA1, 0x00};
byte button_1_On [] = {0x2A, 0x2A, 0x2A, 0xD1, 0xF1, 0xA1, 0x01};

byte button_3_Off [] = {0x2A, 0x2A, 0x2A, 0xD1, 0xF1, 0xA4, 0x00};
byte button_3_On [] = {0x2A, 0x2A, 0x2A, 0xD1, 0xF1, 0xA4, 0x01};

byte button_4_Off [] = {0x2A, 0x2A, 0x2A, 0xD1, 0xF1, 0xA5, 0x00};
byte button_4_On [] = {0x2A, 0x2A, 0x2A, 0xD1, 0xF1, 0xA5, 0x01};

byte fan_Off [] = {0x2A, 0x2A, 0x2A, 0xD1, 0xF2, 0x00, 0x00};
byte fan_On [] = {0x2A, 0x2A, 0x2A, 0xD1, 0xF2, 0x01, 0x01};
byte dimmer_up [] = {0x2A, 0x2A, 0x2A, 0xD1, 0xF2, 0x01, 0xB6};
byte dimmer_down [] = {0x2A, 0x2A, 0x2A, 0xD1, 0xF2, 0x01, 0xB7};

byte demand_crrent [] = {0x2A, 0x2A, 0x2A, 0xD1, 0xF3, 0x01, 0x01};

boolean wifi_status = false;

WiFiClient espClient;
PubSubClient client(espClient);

// Time variables
uint32_t receivedTime;
uint32_t shadowTime;
byte timeByte3;
byte timeByte2;
byte timeByte1;
byte timeByte0;


void setup()
{
	Serial.begin(9600);
	EEPROM.begin(512);
	//Serial.println("Testing ESP");
	//WiFi.begin(ssid, password);

	wifi_status = true;
	WiFi.mode(WIFI_STA);
	ConfigureWifi();
	
	server.on ( "/favicon.ico",   []() { server.send ( 200, "text/html", "" );   }  );
	server.on ( "/admin.html", []() { server.send ( 200, "text/html", PAGE_AdminMainPage );   }  );
	server.on ( "/config.html", send_network_configuration_html );
	server.on ( "/info.html", []() { server.send ( 200, "text/html", PAGE_Information );   }  );
	server.on ( "/style.css", []() { server.send ( 200, "text/plain", PAGE_Style_css );  } );
	server.on ( "/microajax.js", []() { server.send ( 200, "text/plain", PAGE_microajax_js );  } );
	server.on ( "/admin/values", send_network_configuration_values_html );
	server.on ( "/admin/connectionstate", send_connection_state_values_html );
	server.on ( "/admin/infovalues", send_information_values_html );
 
	server.onNotFound ( []() { server.send ( 400, "text/html", "Page not Found" );   }  );
	server.begin();
	tkSecond.attach(1,Second_Tick);
	AdminEnabled = false;
	
	while(WiFi.status() != WL_CONNECTED)
	{
		if(Serial.available() > 0)
		{
			if(Serial.readBytes(msg, sizeof(msg)) == sizeof(msg))
			{
				if(msg[0] == 0x2A && msg[1] == 0x2A && msg[2] == 0x2A)
				{
					deviceType = msg[3];
					ID = msg[4];
					switch(deviceType)
					{
						case DEV_TYPE_1:
						{
							switch(ID)
							{
								case ID_AP_MODE:
								{
									WiFi.mode(WIFI_AP_STA);
									WiFi.softAP( ACCESS_POINT_NAME , ACCESS_POINT_PASSWORD);
									AdminEnabled= true;
									AdminTimeOutCounter=0;
									break;
								}
								default:
									break;
							}
							break;
						}
						default:
							break;
					}
				}
			}
		}
		server.handleClient();
	}
	WiFi.mode(WIFI_STA);
	AdminEnabled = false;
	
	client.setServer(mqtt_server, mqtt_port);
	client.setCallback(callback);
	(WiFi.macAddress()).toCharArray(localMacAddrBuff, sizeof(localMacAddrBuff));
}
 
void callback(char* topic, byte* payload, unsigned int length)
{
	String mqttmesg = String((char *)payload);
	
	StaticJsonBuffer<1500> jsonBuffer;
	JsonObject& root = jsonBuffer.parseObject(mqttmesg);
	
	if(String(topic) == accept_device_state)
	{
		client.unsubscribe(accept_device_state);
		shadowTime = root["timestamp"];
		receivedTime = readEEPROM_timeStamp();
		
		if(shadowTime > receivedTime)
		{
			button1 = root["state"]["desired"]["b1"];
			button2 = root["state"]["desired"]["b2"];
			fan = root["state"]["desired"]["b3"];
			button4 = root["state"]["desired"]["b4"];
			button5 = root["state"]["desired"]["b5"];

			dimmer = root["state"]["desired"]["dim"];
			
			if(button1 == 1)
			{
				Serial.write(button_0_On, sizeof(button_0_On));
				button1 = 1;
			}
			else if(button1 == 0)
			{
				Serial.write(button_0_Off, sizeof(button_0_Off));
				button1 = 0;
			}
			if(button2 == 1)
			{
				Serial.write(button_1_On, sizeof(button_1_On));
				button2 = 1;
			}
			else if(button2 == 0)
			{
				Serial.write(button_1_Off, sizeof(button_1_Off));
				button2 = 0;
			}
			if(fan == 1)
			{
				fan_On[6] = byte(dimmer);
				Serial.write(fan_On, sizeof(fan_On));
				fan = 1;
			}
			else if(fan == 0)
			{
				Serial.write(fan_Off, sizeof(fan_Off));
				fan = 0;
			}
			if(button4 == 1)
			{
				Serial.write(button_3_On, sizeof(button_3_On));
				button4 = 1;
			}
			else if(button4 == 0)
			{
				Serial.write(button_3_Off, sizeof(button_3_Off));
				button4 = 0;
			}
			if(button5 == 1)
			{
				Serial.write(button_4_On, sizeof(button_4_On));
				button5 = 1;
			}
			else if(button5 == 0)
			{
				Serial.write(button_4_Off, sizeof(button_4_Off));
				button5 = 0;
			}
		}
		else if(shadowTime < receivedTime)
		{
			read_EEPROM_status();
			client.publish(test_Pub, makeJson());
		}
	}

	else if(String(topic) == test_sub)
	{
		button1 = root["state"]["desired"]["b1"];
		button2 = root["state"]["desired"]["b2"];
		fan = root["state"]["desired"]["b3"];
		button4 = root["state"]["desired"]["b4"];
		button5 = root["state"]["desired"]["b5"];

		dimmer = root["state"]["desired"]["dim"];
		current = root["state"]["desired"]["amp"];
		const String foreignMacAddr = root["state"]["desired"]["mac"];

		if((foreignMacAddr) != WiFi.macAddress())
		{
			if(button1 == 1)
			{
				Serial.write(button_0_On, sizeof(button_0_On));
				button1 = 1;
			}
			else if(button1 == 0)
			{
				Serial.write(button_0_Off, sizeof(button_0_Off));
				button1 = 0;
			}
			if(button2 == 1)
			{
				Serial.write(button_1_On, sizeof(button_1_On));
				button2 = 1;
			}
			else if(button2 == 0)
			{
				Serial.write(button_1_Off, sizeof(button_1_Off));
				button2 = 0;
			}
			if(fan == 1)
			{
				fan_On[6] = byte(dimmer);
				Serial.write(fan_On, sizeof(fan_On));
				fan = 1;
			}
			else if(fan == 0)
			{
				Serial.write(fan_Off, sizeof(fan_Off));
				fan = 0;
			}
			if(button4 == 1)
			{
				Serial.write(button_3_On, sizeof(button_3_On));
				button4 = 1;
			}
			else if(button4 == 0)
			{
				Serial.write(button_3_Off, sizeof(button_3_Off));
				button4 = 0;
			}
			if(button5 == 1)
			{
				Serial.write(button_4_On, sizeof(button_4_On));
				button5 = 1;
			}
			else if(button5 == 0)
			{
				Serial.write(button_4_Off, sizeof(button_4_Off));
				button5 = 0;
			}
			if(current == 1)
			{
				Serial.write(demand_crrent, sizeof(demand_crrent));
				current = 0;
			}
		}
	}
}

void reconnect()
{
	while (!client.connected())
	{
		if(client.connect(mqtt_client_name))
		{
			client.subscribe(test_sub);
			client.subscribe(accept_device_state);
			client.publish(request_device_state , "");
		}
		else
		{
			delay(2000);
		}
	}
}

char* makeJson()
{
	StaticJsonBuffer<200> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();

	JsonObject& state = root.createNestedObject("state");
	JsonObject& desired = state.createNestedObject("desired");
	desired["b1"] = button1;
	desired["b2"] = button2;
	desired["b3"] = fan;
	desired["b4"] = button4;
	desired["b5"] = button5;
	desired["dim"] = dimmer;
	desired["amp"] = ampere;
	desired["mac"] = localMacAddrBuff;

	String pubMessage;

	root.printTo(pubMessage);

	char msg[pubMessage.length() + 1];
	pubMessage.toCharArray(msg, pubMessage.length() + 1 );

	return msg;
}

void loop()
{
	if (AdminEnabled)
	{	
		server.handleClient();
		if (AdminTimeOutCounter > AdminTimeOut)
		{
			//Serial.println("AP mode expired.");
			AdminEnabled = false;
			WiFi.mode(WIFI_STA);
			ConfigureWifi();
		}
	}
	
	if(wifi_status == true && AdminEnabled == false)
	{
		if(!client.connected())
		{
			reconnect();
		}
		client.loop();
	}
	
	if(Serial.available() > 0)
    {
		if(Serial.readBytes(msg, sizeof(msg)) == sizeof(msg))
		{
			if(msg[0] == 0x2A && msg[1] == 0x2A && msg[2] == 0x2A)
			{
				deviceType = msg[3];
				ID = msg[4];
				SeqNumber = msg[5];
				btnState = msg[6];
				timeByte3 = msg[9];
				timeByte2 = msg[10];
				timeByte1 = msg[11];
				timeByte0 = msg[12];
				write_EEPROM_timeStamp();
				
				// Start deviceType Switch
				switch(deviceType)
				{
					case DEV_TYPE_1:
					{
						switch(ID)
						{
							case ID_BTN:
							{
								switch(SeqNumber)
								{
									case BTN_1:
									{
										switch(btnState)	// Start BTNs ON/OFF state switch
										{
											case BTN_STATE_OFF:
											{
												if(client.connected())
												{
													button1 = 0;
													client.publish(test_Pub, makeJson());
													EEPROM.write(3, 0x00);
													EEPROM.commit();
												}
												break;
											}
											case BTN_STATE_ON:
											{
												if(client.connected())
												{
													button1 = 1;
													client.publish(test_Pub, makeJson());
													EEPROM.write(3, 0x01);
													EEPROM.commit();
												}
												break;
											}
										}
										break;
									}
									
									case BTN_2:
									{
										switch(btnState)	// Start BTNs ON/OFF state switch
										{   
											case BTN_STATE_OFF:
											{
												if(client.connected())
												{
													button2 = 0;
													client.publish(test_Pub, makeJson());
													EEPROM.write(4, 0x00);
													EEPROM.commit();
												}
												break;
											}
											case BTN_STATE_ON:
											{
												if(client.connected())
												{
													button2 = 1;
													client.publish(test_Pub, makeJson());
													EEPROM.write(4, 0x01);
													EEPROM.commit();
												}
												break;
											}
										}
										break;
									}
									
									case BTN_5:
									{
										switch(btnState)// Start BTNs ON/OFF state switch
										{
											case BTN_STATE_OFF:
											{
												if(client.connected())
												{
													button4 = 0;
													client.publish(test_Pub, makeJson());
													EEPROM.write(5, 0x00);
													EEPROM.commit();
												}
												break;
											}
											case BTN_STATE_ON:
											{
												if(client.connected())
												{
													button4 = 1;
													client.publish(test_Pub, makeJson());
													EEPROM.write(5, 0x01);
													EEPROM.commit();
												}
												break;
											}
										}
										break;
									}
										
									case BTN_6:
									{
										switch(btnState)   // Start BTNs ON/OFF state switch
										{
											case BTN_STATE_OFF:
											{
												if(client.connected())
												{
													button5 = 0;
													client.publish(test_Pub, makeJson());
													EEPROM.write(6, 0x00);
													EEPROM.commit();
												}
												break;
											}
											case BTN_STATE_ON:
											{
												if(client.connected())
												{
													button5 = 1;
													client.publish(test_Pub, makeJson());
													EEPROM.write(6, 0x01);
													EEPROM.commit();
												}
												break;
											}
										}
										break;
									}
								}// End Sequence number
								break;
							}
			
							case ID_FAN:
							{
								switch(btnState)
								{
									case FAN_STATE_OFF:
									{
										if(client.connected())
										{
											fan = 0;
											client.publish(test_Pub, makeJson());
											EEPROM.write(7, 0x00);
											EEPROM.commit();
										}
										break;
									}
									case FAN_STATE_ON:
									{
										if(client.connected())
										{
											fan = 1;
											client.publish(test_Pub, makeJson());
											EEPROM.write(7, 0x01);
											EEPROM.commit();
										}
										break;
									}
									case DIM_STATE_PLUS:
									{
										if(client.connected())
										{
											if(dimmer < 6)
											{
												dimmer += 1;
											}
											client.publish(test_Pub, makeJson());
											write_EEPROM_dimmer();
										}
										break;
									}
									case DIM_STATE_MINUS:
									{
										if(client.connected())
										{
											if(dimmer > 1)
											{
												dimmer -= 1;
											}
											client.publish(test_Pub, makeJson());
											write_EEPROM_dimmer();
										}
										break;
									}
								}
								break;
							}
								
							case ID_CURRENT:
							{
								byte tempCurrent[4];
								tempCurrent[0] = msg[5];
								tempCurrent[1] = msg[6];
								tempCurrent[2] = msg[7];
								tempCurrent[3] = msg[8];
								ampere = *((float*)(tempCurrent));

								if(client.connected())
								{
									client.publish(test_Pub, makeJson());
									ampere = 0.00;
								}
								break;
							}
							case ID_WIFI:
							{
								switch(SeqNumber)
								{
									case BTN_1:
									{
										switch(btnState)
										{   // Start Wifi ON/OFF state switch
											case BTN_STATE_OFF:
											{
												WiFi.disconnect();
												wifi_status = false;
												break;
											}
											case BTN_STATE_ON:
											{
												WiFi.begin(ssid, password);
												wifi_status = true;
												break;
											}
										}
									}
									break;

								}// End Sequence number
								break;
							}
							case ID_AP_MODE:
							{
								if (AdminEnabled == false)
								{	
									WiFi.mode(WIFI_AP_STA);
									WiFi.softAP( ACCESS_POINT_NAME , ACCESS_POINT_PASSWORD);
									AdminEnabled= true;
									AdminTimeOutCounter=0;
									//Serial.println("AP mode enabled.");
								}
								break;
							}
						}// END Switch ID
						break;
					}
					default:
						break;
				}// END deviceType Switch
			}// END if data correction
		}// END if Serial bytes
	}
}

void write_EEPROM_dimmer(void)
{
	switch(dimmer)
	{
		case 1:
		{
			EEPROM.write(8, 0x01);			break;
		}
		case 2:
		{
			EEPROM.write(8, 0x02);			break;
		}
		case 3:
		{
			EEPROM.write(8, 0x03);			break;
		}
		case 4:
		{
			EEPROM.write(8, 0x04);			break;
		}
		case 5:
		{
			EEPROM.write(8, 0x05);			break;
		}
		default:
			break;
	}
	EEPROM.commit();
}

void write_EEPROM_timeStamp(void)
{
	EEPROM.write(9, timeByte3);
	EEPROM.write(10, timeByte2);
	EEPROM.write(11, timeByte1);
	EEPROM.write(12, timeByte0);
	EEPROM.commit();
}

uint32_t readEEPROM_timeStamp(void)
{
	timeByte3 = EEPROM.read(9);
	timeByte2 = EEPROM.read(10);
	timeByte1 = EEPROM.read(11);
	timeByte0 = EEPROM.read(12);
	
	uint32_t tempTime = 0;
    tempTime += (uint32_t)timeByte3 << 24;
    tempTime += (uint32_t)timeByte2 << 16;
    tempTime += (uint32_t)timeByte1 << 8;
    tempTime += (uint32_t)timeByte0;
	
	return tempTime;	
}

void read_EEPROM_status(void)
{
	button1 = int(EEPROM.read(3));
	button2 = int(EEPROM.read(4));
	button4 = int(EEPROM.read(5));
	button5 = int(EEPROM.read(6));
	fan = int(EEPROM.read(7));
	dimmer = int(EEPROM.read(8));
}