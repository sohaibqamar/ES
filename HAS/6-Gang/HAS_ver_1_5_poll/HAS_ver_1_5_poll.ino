#include <avr/interrupt.h>
#include <avr/io.h>
#include <Wire.h>
#include "Adafruit_MCP23017.h"
#include "Adafruit_TLC5947.h"
#include <EEPROM.h>
#include "RTClib.h"

Adafruit_MCP23017 mcp;
RTC_DS1307 rtc;

#define RELAY_BTN1 0
#define RELAY_BTN2 2
#define RELAY_FAN 1
#define RELAY_BTN4 3
#define RELAY_BTN5 4
#define RELAY_BTN6 5
#define BUZZER_OUT 6

#define PAD1   15
#define PAD2   13
#define PAD3   14
#define PAD4   12
#define PAD5   8
#define PAD6   9
#define PAD7   11
#define PAD8   10

// How many boards do you have chained?
#define NUM_TLC5974 1
 
#define data   14
#define clock   15
#define latch   10
#define oe  16//-1  // set to -1 to not use the enable pin (its optional)

#define LED0 0
#define LED1 1
#define LED2 2
#define LED3 3
#define LED4 4
#define LED5 5
#define LED6 6
#define LED7 7
#define LED8 8

#define DEV_TYPE_1 0xD1
#define ID_BTN 0xF1
#define ID_FAN 0xF2
#define ID_CURRENT 0xF3
#define BTN_1 0xA0
#define BTN_2 0xA1
#define BTN_3 0xA2
#define BTN_4 0xA3
#define BTN_5 0xA4
#define BTN_6 0xA5
#define BTN_7 0xA6
#define BTN_8 0xA7

#define FAN_OFF 0x00
#define FAN_ON 0x01
#define FAN_UP 0xB6
#define FAN_DOWN 0xB7

#define BTN_STATE_OFF 0x00
#define BTN_STATE_ON 0x01

#define totalTicks 3456
#define speedTicks 576
#define AC_TIM2_OVF 1        //used to keep how many interrupts were fired

long time1 = 0;         // the last time the output pin was toggled
long time_ap_mode = 0;
long time_ap_mode_running = 0;
unsigned long led_blink_counter = 0;
long debounce = 20;   // the debounce time, increase if the output flickers

Adafruit_TLC5947 tlc = Adafruit_TLC5947(NUM_TLC5974, clock, data, latch);

boolean PAD1_isReleased = false;
boolean PAD1_pressed = false;
boolean PAD2_isReleased = false;
boolean PAD2_pressed = false;
boolean PAD3_isReleased = false;
boolean PAD3_pressed = false;
boolean PAD4_isReleased = false;
boolean PAD4_pressed = false;
boolean PAD5_isReleased = false;
boolean PAD5_pressed = false;
boolean PAD6_isReleased = false;
boolean PAD6_pressed = false;
boolean PAD7_isReleased = false;
boolean PAD7_pressed = false;
boolean PAD8_isReleased = false;
boolean PAD8_pressed = false;
boolean flag_ap_mode = true;
boolean flag_ap_Mode_running = false;
boolean wifi_led_status = false;
byte recvMsg[7];         // a string to hold incoming data
byte sendMsg[13];         // a string to hold incoming data

volatile byte deviceType;
volatile byte ID;
volatile byte SeqNumber;
volatile byte btnState;
volatile byte fan_speed = 1;
volatile byte fan_status = 0;

volatile unsigned int timerCounter = 0;
// Current Sensor Variables
const int sensorIn = A6;
int mVperAmp = 66; // use 185 for 5A, 100 for 20A Module and 66 for 30A Module
float Voltage = 0;
float VRMS = 0;
float AmpsRMS = 0;

// EEPROM variables
volatile byte mem_btn1_status;
volatile byte mem_btn2_status;
volatile byte mem_fan_status;
volatile byte mem_fan_speed;
volatile byte mem_btn4_status;
volatile byte mem_btn5_status;
volatile byte mem_btn6_status;

// RTC variables
uint32_t currentTime;
byte timeByte3;
byte timeByte2;
byte timeByte1;
byte timeByte0;

// Timer ISR for controlling the Pitch Motor
ISR(TIMER2_OVF_vect)
{
    TCNT2 = AC_TIM2_OVF;
	timerCounter++;
}

void setup()
{
	Serial.begin(9600);
	rtc.begin();
	DateTime now = rtc.now();
	currentTime = now.unixtime();
	timeByte3 = lowByte(currentTime >> 24);
    timeByte2 = lowByte(currentTime >> 16);
    timeByte1 = lowByte(currentTime >> 8);
    timeByte0 = lowByte(currentTime);
	
	Enable_Triac();
	Turn_OFF_Triac();
	
	cli();
	Enable_Timer2();
	sei();
	Disable_Timer2();
	
	tlc.begin();
    if (oe >= 0)
    {
        pinMode(oe, OUTPUT);
        digitalWrite(oe, LOW);
    }
    LEDs_OFF_ALL();
	
	mcp.begin();      // use default address 0
	Enable_Relays();
	Relay_OFF_ALL();
	/*Enable_Buttons();
	/*delay(300);*/
	Turn_ON_Fan();
	//delay(1500);
	
	Buzzer_Enable();
	Buzzer_ON();
	delay(10);
    Buzzer_OFF();
    delay(10);
    Buzzer_ON();
    delay(10);
    Buzzer_OFF();
	
	Resume_Last_State();
}

void loop() 
{
    if(Serial.available() > 0)
    {
        if(Serial.readBytes(recvMsg, sizeof(recvMsg)) == sizeof(recvMsg))
        {
            if(recvMsg[0] == 0x2A && recvMsg[1] == 0x2A && recvMsg[2] == 0x2A) // Start valid Data Check
            {
                deviceType = recvMsg[3];
                ID = recvMsg[4];
                SeqNumber = recvMsg[5];
                btnState = recvMsg[6];
                
                switch(deviceType) // Start deviceType Switch
                {
                    case DEV_TYPE_1:
                    {
                        switch(ID) // Start ID Switch --> BTNS or FAN
                        {
                            case ID_BTN:
                            {
                                switch(SeqNumber) // Start BTNs switch
                                {
                                    case BTN_1:
                                    {
                                        switch(btnState)// Start BTNs ON/OFF state switch
                                        {
                                            case BTN_STATE_OFF:
                                            {
                                                PAD1_pressed = false;
                                                BTN1_OFF();
                                                break;
                                            }
                                            case BTN_STATE_ON:
                                            {
                                                PAD1_pressed = true;
                                                BTN1_ON();
                                                break;
                                            }
                                        }// END BTNs ON/OFF state switch
                                        break;
                                    }
                                    case BTN_2:
                                    {
                                        switch(btnState)// Start BTNs ON/OFF state switch
                                        {
                                            case BTN_STATE_OFF:
                                            {
                                                PAD2_pressed = false;
                                                BTN2_OFF();
                                                break;
                                            }
                                            case BTN_STATE_ON:
                                            {
                                                PAD2_pressed = true;
                                                BTN2_ON();
                                                break;
                                            }
                                        }// END BTNs ON/OFF state switch
                                        break;
                                    }
                                    case BTN_5:
                                    {
                                        switch(btnState)// Start BTNs ON/OFF state switch
                                        {
                                            case BTN_STATE_OFF:
                                            {
                                                PAD5_pressed = false;
                                                BTN5_OFF();
                                                break;
                                            }
                                            case BTN_STATE_ON:
                                            {
                                                PAD5_pressed = true;
                                                BTN5_ON();
                                                break;
                                            }
                                        }// END BTNs ON/OFF state switch
                                        break;
                                    }
                                    case BTN_6:
                                    {
                                        switch(btnState)// Start BTNs ON/OFF state switch
                                        {
                                            case BTN_STATE_OFF:
                                            {
                                                PAD6_pressed = false;
                                                BTN6_OFF();
                                                break;
                                            }
                                            case BTN_STATE_ON:
                                            {
                                                PAD6_pressed = true;
                                                BTN6_ON();
                                                break;
                                            }
                                        }// END BTNs ON/OFF state switch
                                        break;
                                    }
                                    default:
                                        break;
                                } // END BTNs switch
                                break;
                            }
                            case ID_FAN:
                            {
                                switch(SeqNumber)
                                {
                                    case FAN_OFF:
                                    {
                                        PAD3_pressed = false;
                                        FAN_OFF_FUNC();
                                        break;
                                    }
                                    case FAN_ON:
                                    {
                                        PAD3_pressed = true;
                                        FAN_ON_FUNC();
                                        switch(btnState)
                                        {
                                            case 0x01:
                                            {
                                                fan_speed = 1;
												
                                                break;
                                            }
                                            case 0x02:
                                            {
                                                fan_speed = 2;
												
                                                break;
                                            }
                                            case 0x03:
                                            {
                                                fan_speed = 3;
												
                                                break;
                                            }
                                            case 0x04:
                                            {
                                                fan_speed = 4;
												
                                                break;
                                            }
                                            case 0x05:
                                            {
                                                fan_speed = 5;
												
                                                break;
                                            }
                                            case 0x06:
                                            {
                                                fan_speed = 6;
												
                                                break;
                                            }
                                            case FAN_UP:
                                            {
                                                fan_speed++;
                                                if(fan_speed > 6)
                                                {
                                                    fan_speed = 6;
                                                }
                                                break;
                                            }
                                            case FAN_DOWN:
                                            {
                                                PAD7_pressed = true;
                                                fan_speed--;
                                                if(fan_speed < 1)
                                                {
                                                    fan_speed = 1;
                                                }
                                                break;
                                            }
                                            default:
                                                break;
                                        }
                                        dimmerLEDs(fan_speed);
                                        break;
                                    }
                                    default:
                                    break;
                                }
                                break;
                            }
							case ID_CURRENT:
							{
								Voltage = getVPP();
								VRMS = (Voltage/2.0) *0.707; 
								AmpsRMS = (VRMS * 1000)/mVperAmp;
								Send_Current(AmpsRMS);
								break;
							}
                            default:
                                break;
                        }// END ID Switch
                        break;
                    }
                    default:
                        break;
                }// END deviceType Switch
            }// END valid Data Check
        } // END Serial Receive
    }
    /////////////////////////PAD1 
    if((mcp.digitalRead(PAD1) == HIGH) && (millis() - time1 > debounce) && PAD1_isReleased == true && flag_ap_Mode_running == false)
    {
        PAD1_isReleased = false;
        if(PAD1_pressed == true)
        {
            PAD1_pressed = false;
            BTN1_OFF();
			Send_Relay_Status(0, 0);
        }
        else
        {
            PAD1_pressed = true;
            BTN1_ON();
			Send_Relay_Status(0, 1);
        }
        time1 = millis();
    }
    else if((mcp.digitalRead(PAD1) == LOW) && PAD1_isReleased == false)
    {
        PAD1_isReleased = true;
    }
/////////////////////////PAD2
    if((mcp.digitalRead(PAD2) == HIGH) && (millis() - time1 > debounce) && PAD2_isReleased == true && flag_ap_Mode_running == false)
    {
		flag_ap_mode = true;
		time_ap_mode = millis();
        PAD2_isReleased = false;
        if(PAD2_pressed == true)
        {
            PAD2_pressed = false;
            BTN2_OFF();
			Send_Relay_Status(1, 0);
        }
        else
        {
            PAD2_pressed = true;
            BTN2_ON();
			Send_Relay_Status(1, 1);
        }
        time1 = millis();
    }
	else if((mcp.digitalRead(PAD2) == LOW) && PAD2_isReleased == false)
    {
		flag_ap_mode = false;
		time_ap_mode = 0;
        PAD2_isReleased = true;
    }
	if(flag_ap_mode == true && (millis()-time_ap_mode > 5000))
	{
		if(PAD2_pressed == true)
        {
            PAD2_pressed = false;
            BTN2_OFF();
			Send_Relay_Status(1, 0);
        }
        else
        {
            PAD2_pressed = true;
            BTN2_ON();
			Send_Relay_Status(1, 1);
        }
		
		flag_ap_mode = false;
		time_ap_mode = 0;
		Buzzer_ON();
		delay(50);
		Buzzer_OFF();
		delay(50);
		Buzzer_ON();
		delay(50);
		Buzzer_OFF();
		delay(50);
		Buzzer_ON();
		delay(50);
		Buzzer_OFF();
		Send_AP_Mode();
		// Farmaishi Program
		flag_ap_Mode_running = true;
		time_ap_mode_running = millis();
		wifi_led_status = true;
		led_blink_counter = 0;
	}
	
	if(flag_ap_Mode_running == true)
	{
		if(millis()-time_ap_mode_running > 300)
		{
			led_blink_counter++;
			time_ap_mode_running = millis();
			if(wifi_led_status == true)
			{
				BTN4_OFF();
				wifi_led_status = false;
			}
			else
			{
				BTN4_ON();
				wifi_led_status = true;
			}
		}
		if(led_blink_counter > 600)
		{
			BTN4_ON();
			Buzzer_ON();
			delay(1000);
			Buzzer_OFF();
			flag_ap_Mode_running = false;
		}
	}
	
/////////////////////////PAD 3 ---> FAN Button
    if((mcp.digitalRead(PAD3) == HIGH) && (millis() - time1 > debounce) && PAD3_isReleased == true && flag_ap_Mode_running == false)
    {
        PAD3_isReleased = false;
        if(PAD3_pressed == true)
        {
            PAD3_pressed = false;
            FAN_OFF_FUNC();
			Send_FAN_Status(0, fan_speed);
        }
        else
        {
            PAD3_pressed = true;
			FAN_ON_FUNC();
			Send_FAN_Status(1, fan_speed);
        }
        time1 = millis();
    }
    else if((mcp.digitalRead(PAD3) == LOW) && PAD3_isReleased == false)
    {
        PAD3_isReleased = true;
    }
/////////////////////////PAD 4
    if((mcp.digitalRead(PAD4) == HIGH) && (millis() - time1 > debounce) && PAD4_isReleased == true && flag_ap_Mode_running == false)
    {
        PAD4_isReleased = false;
        if(PAD4_pressed == true)
        {
            PAD4_pressed = false;
            BTN4_OFF();
			Send_WiFi_status(0);
        }
        else
        {
            PAD4_pressed = true;
            BTN4_ON();
			Send_WiFi_status(1);
        }
        time1 = millis();
    }
    else if((mcp.digitalRead(PAD4) == LOW) && PAD4_isReleased == false)
    {
        PAD4_isReleased = true;
    }

/////////////////////////PAD5
    if((mcp.digitalRead(PAD5) == HIGH) && (millis() - time1 > debounce) && PAD5_isReleased == true && flag_ap_Mode_running == false)
    {
        PAD5_isReleased = false;
        if(PAD5_pressed == true)
        {
            PAD5_pressed = false;
            BTN5_OFF();
			Send_Relay_Status(4, 0);
        }
        else
        {
            PAD5_pressed = true;
            BTN5_ON();
			Send_Relay_Status(4, 1);
        }
        time1 = millis();
    }
    else if((mcp.digitalRead(PAD5) == LOW) && PAD5_isReleased == false)
    {
        PAD5_isReleased = true;
    }
/////////////////////////PAD6
    if((mcp.digitalRead(PAD6) == HIGH) && (millis() - time1 > debounce) && PAD6_isReleased == true && flag_ap_Mode_running == false)
    {
        PAD6_isReleased = false;
        if(PAD6_pressed == true)
        {
            PAD6_pressed = false;
            BTN6_OFF();
			Send_Relay_Status(5, 0);
        }
        else
        {
            PAD6_pressed = true;
            BTN6_ON();
			Send_Relay_Status(5, 1);
        }
        time1 = millis();
    }
    else if((mcp.digitalRead(PAD6) == LOW) && PAD6_isReleased == false)
    {
        PAD6_isReleased = true;
    }
/////////////////////////PAD7
    if((mcp.digitalRead(PAD7) == HIGH) && (millis() - time1 > debounce) && PAD7_isReleased == true && flag_ap_Mode_running == false)
    {
        PAD7_isReleased = false;
		if(fan_status == 1)
		{
			fan_speed--;
			if(fan_speed < 1)
			{
				fan_speed = 1;
			}
			switch(fan_speed)
			{
				case 1:
				{
					dimmerLEDs(fan_speed);
					break;
				}
				case 2:
				{
					dimmerLEDs(fan_speed);
					break;
				}
				case 3:
				{
					dimmerLEDs(fan_speed);
					break;
				}
				case 4:
				{
					dimmerLEDs(fan_speed);
					break;
				}
				case 5:
				{
					dimmerLEDs(fan_speed);
					break;
				}
				case 6:
				{
					dimmerLEDs(fan_speed);
					break;
				}
				default:
				{
					dimmerLEDs(fan_speed);
					break;
				}
			}
			
			Send_FAN_Status(3, fan_speed);
		}
        time1 = millis();
    }
    else if((mcp.digitalRead(PAD7) == LOW) && PAD7_isReleased == false)
    {
        PAD7_isReleased = true;
    }    
 /////////////////////////PAD8
    if((mcp.digitalRead(PAD8) == HIGH) && (millis() - time1 > debounce) && PAD8_isReleased == true && flag_ap_Mode_running == false)
    {
        PAD8_isReleased = false;
		
		if(fan_status == 1)
		{
			fan_speed++;
			if(fan_speed > 6)
			{
				fan_speed = 6;
			}
			switch(fan_speed)
			{
				case 1:
				{
					dimmerLEDs(fan_speed);
					break;
				}
				case 2:
				{
					dimmerLEDs(fan_speed);
					break;
				}
				case 3:
				{
					dimmerLEDs(fan_speed);
					break;
				}
				case 4:
				{
					dimmerLEDs(fan_speed);
					break;
				}
				case 5:
				{
					dimmerLEDs(fan_speed);
					break;
				}
				case 6:
				{
					dimmerLEDs(fan_speed);
					break;
				}
				default:
				{
					dimmerLEDs(fan_speed);
					break;
				}
			}
			
			Send_FAN_Status(2, fan_speed);
		}
        time1 = millis();
    }
    else if((mcp.digitalRead(PAD8) == LOW) && PAD8_isReleased == false)
    {
        PAD8_isReleased = true;
    }

	if(fan_status == 1)
    {
        if(timerCounter > 0 && timerCounter < (speedTicks*fan_speed))
		{
			Turn_ON_Triac();
		}
		else if(timerCounter > (speedTicks*fan_speed) && timerCounter < totalTicks)
		{
			Turn_OFF_Triac();
		}
		else if(timerCounter >= totalTicks)
		{
			timerCounter = 0;
		}
    }
}

void Resume_Last_State(void)
{
	/*mem_btn1_status = EEPROM.read(0);
	mem_btn2_status = EEPROM.read(1);
	mem_fan_status = EEPROM.read(2);
	mem_fan_speed = EEPROM.read(3);
	mem_btn4_status = EEPROM.read(4);
	mem_btn5_status = EEPROM.read(5);
	mem_btn6_status = EEPROM.read(6);
	/*Serial.print(mem_btn1_status);
	Serial.print(mem_btn2_status);
	Serial.print(mem_fan_status);
	Serial.print(mem_fan_speed);
	Serial.print(mem_btn4_status);
	Serial.print(mem_btn5_status);
	Serial.println(mem_btn6_status);*/
	// By default, Wi-Fi is enabled.
	BTN4_ON();
	PAD4_pressed = true;
	/*switch(mem_btn1_status)// Button 1 State Resume
	{
		case 0:
		{
			PAD1_pressed = false;
			mcp.digitalWrite(RELAY_BTN1, LOW);
			tlc.setLED(LED0, 0, 0, 0);
			tlc.write();
			break;
		}
		case 1:
		{
			PAD1_pressed = true;
			mcp.digitalWrite(RELAY_BTN1, HIGH);
			tlc.setLED(LED0, 1000, 1000, 1000);
			tlc.write();
			break;
		}
		default:
		{
			PAD1_pressed = false;
			mcp.digitalWrite(RELAY_BTN1, LOW);
			tlc.setLED(LED0, 0, 0, 0);
			tlc.write();
			break;
		}
	}
	
	switch(mem_btn2_status)// Button 2 State Resume
	{
		case 0:
		{
			PAD2_pressed = false;
			mcp.digitalWrite(RELAY_BTN2, LOW);
			tlc.setLED(LED1, 0, 0, 0);
			tlc.write();
			break;
		}
		case 1:
		{
			PAD2_pressed = true;
			mcp.digitalWrite(RELAY_BTN2, HIGH);
			tlc.setLED(LED1, 1000, 1000, 1000);
			tlc.write();
			break;
		}
		default:
		{
			PAD2_pressed = false;
			mcp.digitalWrite(RELAY_BTN2, LOW);
			tlc.setLED(LED1, 0, 0, 0);
			tlc.write();
			break;
		}
	}
	
	switch(mem_btn5_status)// Button 5 State Resume
	{
		case 0:
		{
			PAD5_pressed = false;
			mcp.digitalWrite(RELAY_BTN5, LOW);
			tlc.setLED(LED4, 0, 0, 0);
			tlc.write();
			break;
		}
		case 1:
		{
			PAD5_pressed = true;
			mcp.digitalWrite(RELAY_BTN5, HIGH);
			tlc.setLED(LED4, 1000, 1000, 1000);
			tlc.write();
			break;
		}
		default:
		{
			PAD5_pressed = false;
			mcp.digitalWrite(RELAY_BTN5, LOW);
			tlc.setLED(LED4, 0, 0, 0);
			tlc.write();
			break;
		}
	}
	
	switch(mem_btn6_status)// Button 6 State Resume
	{
		case 0:
		{
			PAD6_pressed = false;
			mcp.digitalWrite(RELAY_BTN6, LOW);
			tlc.setLED(LED5, 0, 0, 0);
			tlc.write();
			break;
		}
		case 1:
		{
			PAD6_pressed = true;
			mcp.digitalWrite(RELAY_BTN6, HIGH);
			tlc.setLED(LED5, 1000, 1000, 1000);
			tlc.write();
			break;
		}
		default:
		{
			PAD6_pressed = false;
			mcp.digitalWrite(RELAY_BTN6, LOW);
			tlc.setLED(LED5, 0, 0, 0);
			tlc.write();
			break;
		}
	}
	
	switch(mem_fan_status)
	{
		case 0:
		{
			PAD3_pressed = false;
			FAN_OFF_FUNC();
			fan_speed = mem_fan_speed;
			break;
		}
		case 1:
		{
			PAD3_pressed = true;
			FAN_ON_FUNC();
			fan_speed = mem_fan_speed;
			dimmerLEDs(fan_speed);
			break;
		}
		default:
		{
			PAD3_pressed = false;
			FAN_OFF_FUNC();
			fan_speed = mem_fan_speed;
			break;
		}
	}*/
}

void Enable_Relays(void)
{
	mcp.pinMode(RELAY_BTN1, OUTPUT);
    mcp.pinMode(RELAY_BTN2, OUTPUT);
    mcp.pinMode(RELAY_FAN, OUTPUT);
    mcp.pinMode(RELAY_BTN4, OUTPUT);
    mcp.pinMode(RELAY_BTN5, OUTPUT);
    mcp.pinMode(RELAY_BTN6, OUTPUT);
}

void Enable_Buttons(void)
{
	mcp.pinMode(PAD1, INPUT);//Key1, PAD1, Switch 8, Ribbon 9, GPB7 on MCP23017
    mcp.pullUp(PAD1, HIGH);  // turn on a 100K pullup internally
    mcp.pinMode(PAD2, INPUT);//Key1, PAD1, Switch 8, Ribbon 9, GPB7 on MCP23017
    mcp.pullUp(PAD2, HIGH);  // turn on a 100K pullup internally
    mcp.pinMode(PAD3, INPUT);//Key1, PAD1, Switch 8, Ribbon 9, GPB7 on MCP23017
    mcp.pullUp(PAD3, HIGH);  // turn on a 100K pullup internally
    mcp.pinMode(PAD4, INPUT);//Key1, PAD1, Switch 8, Ribbon 9, GPB7 on MCP23017
    mcp.pullUp(PAD4, HIGH);  // turn on a 100K pullup internally
    mcp.pinMode(PAD5, INPUT);//Key1, PAD1, Switch 8, Ribbon 9, GPB7 on MCP23017
    mcp.pullUp(PAD5, HIGH);  // turn on a 100K pullup internally
    mcp.pinMode(PAD6, INPUT);//Key1, PAD1, Switch 8, Ribbon 9, GPB7 on MCP23017
    mcp.pullUp(PAD6, HIGH);  // turn on a 100K pullup internally
    mcp.pinMode(PAD7, INPUT);//Key1, PAD1, Switch 8, Ribbon 9, GPB7 on MCP23017
    mcp.pullUp(PAD7, HIGH);  // turn on a 100K pullup internally
    mcp.pinMode(PAD8, INPUT);//Key1, PAD1, Switch 8, Ribbon 9, GPB7 on MCP23017
    mcp.pullUp(PAD8, HIGH);  // turn on a 100K pullup internally
}

void Buzzer_Enable(void)
{
	mcp.pinMode(BUZZER_OUT, OUTPUT);
}

void Buzzer_OFF(void)
{
	mcp.digitalWrite(BUZZER_OUT, LOW);
}

void Buzzer_ON(void)
{
	mcp.digitalWrite(BUZZER_OUT, HIGH);
}

void BTN1_OFF(void)
{
	mcp.digitalWrite(RELAY_BTN1, LOW);
	tlc.setLED(LED0, 0, 0, 0);
	tlc.write();
	mem_btn1_status = 0;
	write_EEPROM_status();
}

void BTN1_ON(void)
{
	mcp.digitalWrite(RELAY_BTN1, HIGH);
	tlc.setLED(LED0, 1000, 1000, 1000);
	tlc.write();
	mem_btn1_status = 1;
	write_EEPROM_status();
}

void BTN2_OFF(void)
{
	mcp.digitalWrite(RELAY_BTN2, LOW);
	tlc.setLED(LED1, 0, 0, 0);
	tlc.write();
	mem_btn2_status = 0;
	write_EEPROM_status();
}

void BTN2_ON(void)
{
	mcp.digitalWrite(RELAY_BTN2, HIGH);
	tlc.setLED(LED1, 1000, 1000, 1000);
	tlc.write();
	mem_btn2_status = 1;
	write_EEPROM_status();
}

void FAN_OFF_FUNC(void)
{
	//mcp.digitalWrite(RELAY_FAN, LOW);
	tlc.setLED(LED2, 0, 0, 0);
	tlc.write();
	dimmerLEDs(0);
	fan_status = 0;
	Turn_OFF_Triac();
	timerCounter = 0;
	Disable_Timer2();
	mem_fan_status = fan_status;
	mem_fan_speed = fan_speed;
	write_EEPROM_status();
}

void FAN_ON_FUNC(void)
{
	//mcp.digitalWrite(RELAY_FAN, HIGH);
	tlc.setLED(LED2, 1000, 1000, 1000);
	tlc.write();
	dimmerLEDs(fan_speed);
	fan_status = 1;
	timerCounter = 0;
	Enable_Timer2();
	mem_fan_status = fan_status;
	mem_fan_speed = fan_speed;
	write_EEPROM_status();
}

void BTN4_OFF(void)
{
	//mcp.digitalWrite(RELAY_BTN4, LOW);
	tlc.setLED(LED3, 0, 0, 0);
	tlc.write();
	mem_btn4_status = 0;
	write_EEPROM_status();
}

void BTN4_ON(void)
{
	//mcp.digitalWrite(RELAY_BTN4, HIGH);
	tlc.setLED(LED3, 1000, 1000, 1000);
	tlc.write();
	mem_btn4_status = 1;
	write_EEPROM_status();
}

void BTN5_OFF(void)
{
	mcp.digitalWrite(RELAY_BTN5, LOW);
	tlc.setLED(LED4, 0, 0, 0);
	tlc.write();
	mem_btn5_status = 0;
	write_EEPROM_status();
}

void BTN5_ON(void)
{
	mcp.digitalWrite(RELAY_BTN5, HIGH);
	tlc.setLED(LED4, 1000, 1000, 1000);
	tlc.write();
	mem_btn5_status = 1;
	write_EEPROM_status();
}

void BTN6_OFF(void)
{
	mcp.digitalWrite(RELAY_BTN6, LOW);
	tlc.setLED(LED5, 0, 0, 0);
	tlc.write();
	mem_btn6_status = 0;
	write_EEPROM_status();
}

void BTN6_ON(void)
{
	mcp.digitalWrite(RELAY_BTN6, HIGH);
	tlc.setLED(LED5, 1000, 1000, 1000);
	tlc.write();
	mem_btn6_status = 1;
	write_EEPROM_status();
}

void Send_Relay_Status(byte relay_number, byte relay_status)
{
    sendMsg[0] = 0x2A;
    sendMsg[1] = 0x2A;
    sendMsg[2] = 0x2A;
    sendMsg[3] = 0xD1;
    sendMsg[4] = 0xF1;
    
    switch(relay_number)
    {
        case 0:
        {
            sendMsg[5] = 0xA0;          break;
        }
        case 1:
        {
            sendMsg[5] = 0xA1;          break;
        }
        case 2:
        {
            sendMsg[5] = 0xA2;          break;
        }
        case 3:
        {
            sendMsg[5] = 0xA3;          break;
        }
        case 4:
        {
            sendMsg[5] = 0xA4;          break;
        }
        case 5:
        {
            sendMsg[5] = 0xA5;          break;
        }
        case 6:
        {
            sendMsg[5] = 0xA6;          break;
        }
        case 7:
        {
            sendMsg[5] = 0xA7;          break;
        }
        default:
            break;
    }
    if(relay_status == 0)
    {
        sendMsg[6] = 0x00;
    }
    else if(relay_status == 1)
    {
        sendMsg[6] = 0x01;
    }
	sendMsg[7] = 0x00;
	sendMsg[8] = 0x00;
	
	DateTime now = rtc.now();
	currentTime = now.unixtime();
	timeByte3 = lowByte(currentTime >> 24);
    timeByte2 = lowByte(currentTime >> 16);
    timeByte1 = lowByte(currentTime >> 8);
    timeByte0 = lowByte(currentTime);
	
	sendMsg[9] = timeByte3;
    sendMsg[10] = timeByte2;
    sendMsg[11] = timeByte1;
    sendMsg[12] = timeByte0;
	
    Serial.write(sendMsg, sizeof(sendMsg));
}

void Send_FAN_Status(byte fan_status, byte fan_speed_temp)
{
    sendMsg[0] = 0x2A;
    sendMsg[1] = 0x2A;
    sendMsg[2] = 0x2A;
    sendMsg[3] = 0xD1;
    sendMsg[4] = 0xF2;
    
    if(fan_status == 0)
    {
        sendMsg[5] = 0x00;
        sendMsg[6] = 0x00;
    }
    else if(fan_status == 1)
    {
        sendMsg[5] = 0x01;
        sendMsg[6] = 0x01;
        /*switch(fan_speed_temp)
        {
            case 1:
            {
                sendMsg[6] = 0x01;          break;
            }
            case 2:
            {
                sendMsg[6] = 0x02;          break;
            }
            case 3:
            {
                sendMsg[6] = 0x03;          break;
            }
            case 4:
            {
                sendMsg[6] = 0x04;          break;
            }
            case 5:
            {
                sendMsg[6] = 0x05;          break;
            }
            case 6:
            {
                sendMsg[6] = 0x06;          break;
            }
            default:
                break;
        }*/
    }
    else if(fan_status == 2) // Speed Up
    {
        sendMsg[5] = 0x01;
        sendMsg[6] = 0xB6;
    }
    
    else if(fan_status == 3) // Speed Down
    {
        sendMsg[5] = 0x01;
        sendMsg[6] = 0xB7;
    }
	sendMsg[7] = 0x00;
	sendMsg[8] = 0x00;
	
	DateTime now = rtc.now();
	currentTime = now.unixtime();
	timeByte3 = lowByte(currentTime >> 24);
    timeByte2 = lowByte(currentTime >> 16);
    timeByte1 = lowByte(currentTime >> 8);
    timeByte0 = lowByte(currentTime);
	
	sendMsg[9] = timeByte3;
    sendMsg[10] = timeByte2;
    sendMsg[11] = timeByte1;
    sendMsg[12] = timeByte0;
	
    Serial.write(sendMsg, sizeof(sendMsg));
}

void Send_Current(float tempAMPS)
{
	sendMsg[0] = 0x2A;
    sendMsg[1] = 0x2A;
    sendMsg[2] = 0x2A;
    sendMsg[3] = 0xD1;
    sendMsg[4] = 0xF3;
	byte tempByte[4];
	*((float *)tempByte) = tempAMPS;
	sendMsg[5] = tempByte[0];
	sendMsg[6] = tempByte[1];
	sendMsg[7] = tempByte[2];
	sendMsg[8] = tempByte[3];
	
	DateTime now = rtc.now();
	currentTime = now.unixtime();
	timeByte3 = lowByte(currentTime >> 24);
    timeByte2 = lowByte(currentTime >> 16);
    timeByte1 = lowByte(currentTime >> 8);
    timeByte0 = lowByte(currentTime);
	
	sendMsg[9] = timeByte3;
    sendMsg[10] = timeByte2;
    sendMsg[11] = timeByte1;
    sendMsg[12] = timeByte0;
	
	Serial.write(sendMsg, sizeof(sendMsg));
}

void Send_WiFi_status(byte wifi_status)
{
	sendMsg[0] = 0x2A;
    sendMsg[1] = 0x2A;
    sendMsg[2] = 0x2A;
    sendMsg[3] = 0xD1;
    sendMsg[4] = 0xF4;
	sendMsg[5] = 0xA0;
	if(wifi_status == 0)
	{
		sendMsg[6] = 0x00;
	}
	else if(wifi_status == 1)
	{
		sendMsg[6] = 0x01;
	}
	sendMsg[7] = 0x00;
	sendMsg[8] = 0x00;
	
	DateTime now = rtc.now();
	currentTime = now.unixtime();
	timeByte3 = lowByte(currentTime >> 24);
    timeByte2 = lowByte(currentTime >> 16);
    timeByte1 = lowByte(currentTime >> 8);
    timeByte0 = lowByte(currentTime);
	
	sendMsg[9] = timeByte3;
    sendMsg[10] = timeByte2;
    sendMsg[11] = timeByte1;
    sendMsg[12] = timeByte0;
	
	Serial.write(sendMsg, sizeof(sendMsg));
}

void Send_AP_Mode()
{
	sendMsg[0] = 0x2A;
    sendMsg[1] = 0x2A;
    sendMsg[2] = 0x2A;
    sendMsg[3] = 0xD1;
    sendMsg[4] = 0xF5;
	sendMsg[5] = 0x00;
	sendMsg[6] = 0x00;
	sendMsg[7] = 0x00;
	sendMsg[8] = 0x00;
	
	DateTime now = rtc.now();
	currentTime = now.unixtime();
	timeByte3 = lowByte(currentTime >> 24);
    timeByte2 = lowByte(currentTime >> 16);
    timeByte1 = lowByte(currentTime >> 8);
    timeByte0 = lowByte(currentTime);
	
	sendMsg[9] = timeByte3;
    sendMsg[10] = timeByte2;
    sendMsg[11] = timeByte1;
    sendMsg[12] = timeByte0;
	
	Serial.write(sendMsg, sizeof(sendMsg));
}

void Relay_OFF_ALL(void)
{
    mcp.digitalWrite(RELAY_BTN1, LOW);
    mcp.digitalWrite(RELAY_BTN2, LOW);
    mcp.digitalWrite(RELAY_FAN, LOW);
    mcp.digitalWrite(RELAY_BTN4, LOW);
    mcp.digitalWrite(RELAY_BTN5, LOW);
    mcp.digitalWrite(RELAY_BTN6, LOW);
}

void LEDs_OFF_ALL(void)
{
    tlc.setLED(LED0, 0, 0, 0);
    tlc.write();
    tlc.setLED(LED1, 0, 0, 0);
    tlc.write();
    tlc.setLED(LED2, 0, 0, 0);
    tlc.write();
    tlc.setLED(LED3, 0, 0, 0);
    tlc.write();
    tlc.setLED(LED5, 0, 0, 0);
    tlc.write();
    tlc.setLED(LED6, 0, 0, 0);
    tlc.write();
    tlc.setLED(LED7, 0, 0, 0);
    tlc.write();
    tlc.setLED(LED8, 0, 0, 0);
    tlc.write();
}

float getVPP()
{
  float result;
  
  int readValue;             //value read from the sensor
  int maxValue = 0;          // store max value here
  int minValue = 1024;          // store min value here
  
   uint32_t start_time = millis();
   while((millis()-start_time) < 1000) //sample for 1 Sec
   {
       readValue = analogRead(sensorIn);
       // see if you have a new maxValue
       if (readValue > maxValue) 
       {
           /*record the maximum sensor value*/
           maxValue = readValue;
       }
       if (readValue < minValue) 
       {
           /*record the maximum sensor value*/
           minValue = readValue;
       }
   }
   
   // Subtract min from max
   result = ((maxValue - minValue) * 5.0)/1024.0;
      
   return result;
 }

 // Absolute for long variable
long myABS(long myVar)
{
    if(myVar < 0)
        return (myVar*(-1));
    else
        return myVar;        
}

// Setting Timer2 for AC Sampling
void Enable_Timer2(void)
{
    TCCR2B = 0x00;              //Disbale Timer2 while we set it up
    TCNT2  = AC_TIM2_OVF;    //Reset Timer Count to 30 out of 255
    TIFR2  = 0x00;              //Timer2 INT Flag Reg: Clear Timer Overflow Flag
    TIMSK2 = 0x01;              //Timer2 INT Reg: Timer2 Overflow Interrupt Enable
    TCCR2A = 0x00;              //Timer2 Control Reg A: Wave Gen Mode normal
    TCCR2B = 0x01;              // CLK Frequency, no divider
}

void Disable_Timer2(void)
{
    TCCR2B = 0x00;              //Disbale Timer2 while we set it up
}

void Turn_ON_Fan(void)
{
	mcp.digitalWrite(RELAY_FAN, HIGH);
}

void Turn_OFF_Fan(void)
{
	mcp.digitalWrite(RELAY_FAN, LOW);
}

void Enable_Triac(void)
{
	DDRD |= (1 << DDD5);    // Setting PD5(AC_pin_Triac) as Output for FAN Control
	PORTD &= ~(1 << PD5);   // turn off TRIAC
}

void Turn_ON_Triac(void)
{
  PORTD |= (1 << PD5);  // turn on TRIAC
}

void Turn_OFF_Triac(void)
{
  PORTD &= ~(1 << PD5);   // turn off TRIAC
}

void dimmerLEDs(byte f_speed)
{
	switch(f_speed)
	{
		case 0:
		{
			tlc.setLED(LED6, 0, 0, 0);
			tlc.write();
			tlc.setLED(LED7, 0, 0, 0);
			tlc.write();
			break;
		}
		case 1:
		{
			tlc.setLED(LED6, 1000, 0, 0);
			tlc.write();
			tlc.setLED(LED7, 0, 0, 0);
			tlc.write();
			break;
		}
		case 2:
		{
			tlc.setLED(LED6, 1000, 1000, 0);
			tlc.write();
			tlc.setLED(LED7, 0, 0, 0);
			tlc.write();
			break;
		}
		case 3:
		{
			tlc.setLED(LED6, 1000, 1000, 1000);
			tlc.write();
			tlc.setLED(LED7, 0, 0, 0);
			tlc.write();
			break;
		}
		case 4:
		{
			tlc.setLED(LED6, 1000, 1000, 1000);
			tlc.write();
			tlc.setLED(LED7, 1000, 0, 0);
			tlc.write();
			break;
		}
		case 5:
		{
			tlc.setLED(LED6, 1000, 1000, 1000);
			tlc.write();
			tlc.setLED(LED7, 1000, 1000, 0);
			tlc.write();
			break;
		}
		case 6:
		{
			tlc.setLED(LED6, 1000, 1000, 1000);
			tlc.write();
			tlc.setLED(LED7, 1000, 1000, 1000);
			tlc.write();
			break;
		}
		default:
			break;
	}
}

void write_EEPROM_status(void)
{
	/*delay(200);
    EEPROM.write(0, mem_btn1_status);
    EEPROM.write(1, mem_btn2_status);
    EEPROM.write(2, mem_fan_status);
    EEPROM.write(3, mem_fan_speed);
    EEPROM.write(4, mem_btn4_status);
    EEPROM.write(5, mem_btn5_status);
    EEPROM.write(6, mem_btn6_status);
	EEPROM.write(7, millis());*/
}