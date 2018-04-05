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

#define ACCESS_POINT_NAME  "ESP-P"				
#define ACCESS_POINT_PASSWORD  "12345678" 
#define AdminTimeOut 180 // Defines the Time in Seconds, when the Admin-Mode will be auto-disabled

const char* ssid = "newES";
const char* password = "intel123";
StaticJsonBuffer<200> jsonBuffer;

char* mqttMessage = "";

int button1 = 0;
int current = 0;
float ampere = 0;

const char* mqtt_server = "34.208.3.118";
char localMacAddrBuff[18];

const int mqtt_port = 1883;

int b1 = 0;


const char *mqtt_client_name = "power_socket"; // Client connections cant have the same connection name

const char* test_Pub = "$aws/things/339AJoharTown-AB:98:98:97:97:97/shadow/update";
const char* test_sub = "$aws/things/339AJoharTown-AB:98:98:97:97:97/shadow/update/accepted";
const char* accept_device_state = "$aws/things/339AJoharTown-AB:98:98:97:97:97/shadow/get/accepted";
const char* request_device_state = "$aws/things/339AJoharTown-AB:98:98:97:97:97/shadow/get";

#define DEV_TYPE_1 0xD1
#define DEV_TYPE_7 0xD7
#define ID_BTN 0xF1
#define ID_CURRENT 0xF3
#define ID_WIFI 0xF4
#define ID_AP_MODE 0xF5
#define BTN_1 0xA0

#define BTN_STATE_OFF 0x00
#define BTN_STATE_ON 0x01

byte msg[9];         // a string to hold incoming data
volatile byte deviceType;
volatile byte ID;
volatile byte SeqNumber;
volatile byte btnState;

byte button_0_Off [] = {0x2A, 0x2A, 0x2A, 0xD7, 0xF1, 0xA0, 0x00};
byte button_0_On [] = {0x2A, 0x2A, 0x2A, 0xD7, 0xF1, 0xA0, 0x01};

byte demand_crrent [] = {0x2A, 0x2A, 0x2A, 0xD7, 0xF3, 0x01, 0x01};

boolean wifi_status = false;

WiFiClient espClient;
PubSubClient client(espClient);
 
void setup()
{
	Serial.begin(9600);
	EEPROM.begin(512);

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
						case DEV_TYPE_7:
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
		button1 = root["state"]["desired"]["b1"];
		if (button1 == 1)
		{
			Serial.write(button_0_On, sizeof(button_0_On));
			button1 = 1;
		}
		else if (button1 == 0)
		{
			Serial.write(button_0_Off, sizeof(button_0_Off));
			button1 = 0;
		}
	}
	
	else if(String(topic) == test_sub)
	{
		button1 = root["state"]["desired"]["b1"];
		current = root["state"]["desired"]["amp"];
		const String foreignMacAddr = root["state"]["desired"]["mac"];

		if((foreignMacAddr) != WiFi.macAddress())
		{
			if (button1 == 1)
			{
				Serial.write(button_0_On, sizeof(button_0_On));
				button1 = 1;
			}
			else if (button1 == 0)
			{
				Serial.write(button_0_Off, sizeof(button_0_Off));
				button1 = 0;
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
		if(client.connect(mqtt_client_name )) 
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

				// Start deviceType Switch
				switch(deviceType)
				{
					case DEV_TYPE_7:
					{
						switch(ID)
						{
							case ID_BTN:
							{
								switch(SeqNumber)
								{
									case BTN_1:
									{
										switch(btnState)
										{   // Start BTNs ON/OFF state switch
											case BTN_STATE_OFF:
											{
												if(client.connected())
												{
													button1 = 0;
													client.publish(test_Pub, makeJson());
												}
												break;
											}
											case BTN_STATE_ON:
											{
												if(client.connected())
												{
													button1 = 1;
													client.publish(test_Pub, makeJson());
												}
												break;
											}
										}
									}
									break;

								}// End Sequence number
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
						}
					}
				}
			}
		}
	}
}
