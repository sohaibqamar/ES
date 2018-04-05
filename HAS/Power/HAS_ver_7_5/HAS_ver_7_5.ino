#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>

#define BAUD_RATE 9600 // Setting 9600 Baud Rate

// Defining Pins
#define RELAY_OUT_PIN A0
#define PWR_BTN_PIN A1
#define LED_PIN 3

// Defining Data protocol Constants
#define DEV_TYPE_7 0xD7
#define ID_BTN 0xF1
#define ID_CURRENT 0xF3
#define ID_CHILD_LOCK 0xF5
#define BTN_PWR 0xA0
#define BTN_WIFI 0xA1
#define SEQ_CHILD 0xB0

#define BTN_STATE_OFF 0x00
#define BTN_STATE_ON 0x01

// Defining Global variables
byte recvMsg[7];         // a string to hold incoming data
byte sendMsg[9];         // a string to hold incoming data
volatile byte deviceType;
volatile byte ID;
volatile byte SeqNumber;
volatile byte btnState;
volatile byte current_relay_state = 0;
volatile byte previous_relay_state = 0;
volatile byte wifi_btn_state = 0;
volatile byte wifi_btn_pressed = 0;
volatile byte wifi_btn_released = 1;

volatile byte pwrBTN_state = 0;
volatile byte child_lock_enabled = 0;
// Current Sensor Variables
const int sensorIn = A6;
int mVperAmp = 66; // use 185 for 5A, 100 for 20A Module and 66 for 30A Module
float Voltage = 0;
float VRMS = 0;
float AmpsRMS = 0;

long time_ap_mode = 0;
long time_ap_mode_running = 0;
unsigned long led_blink_counter = 0;
volatile boolean flag_ap_Mode_running = false;
boolean flag_ap_mode = false;
boolean actual_led_status = false;
boolean led_status = false;

//Interrupt Service Routine for PCINT1(PC1), Power Buton
ISR(PCINT1_vect)
{
	if(flag_ap_Mode_running == false)
	{
		// High-to-low transition?
		if ((PINC & _BV(PC1)) == 0)
		{
			if(current_relay_state == 1)
			{
				current_relay_state = 0;
			}
			else if(current_relay_state == 0)
			{
				current_relay_state = 1;
			}
		}
	}
}

void setup()
{
    Serial.begin(BAUD_RATE);	// initialize Serial
	pinMode(LED_PIN, OUTPUT);
    pinMode(RELAY_OUT_PIN, OUTPUT);
    Enable_Power_BTN();
	Enable_WiFi_BTN();
	//Resume_Last_Power_State();
}

void loop()
{
    if((previous_relay_state != current_relay_state)  && flag_ap_Mode_running == false)
    {
		if(current_relay_state == 0)
		{
			digitalWrite(RELAY_OUT_PIN, LOW);
			digitalWrite(LED_PIN, LOW);
			actual_led_status = false;
			//write_EEPROM_status();
			Send_Relay_Status(1, current_relay_state);
		}
        else if(current_relay_state == 1)
		{
			digitalWrite(RELAY_OUT_PIN, HIGH);
			digitalWrite(LED_PIN, HIGH);
			actual_led_status = true;
			//write_EEPROM_status();
			Send_Relay_Status(1, current_relay_state);
		}
        previous_relay_state = current_relay_state;
		flag_ap_mode = true;
		time_ap_mode = millis();
    }
	
	if((PINC & _BV(PC1)) == 2)
	{
		flag_ap_mode = false;
		time_ap_mode = 0;
	}
	
	if(flag_ap_mode == true && (millis()-time_ap_mode > 5000))
	{
		if(current_relay_state == 0)
		{
			current_relay_state = 1;
			digitalWrite(RELAY_OUT_PIN, HIGH);
			digitalWrite(LED_PIN, HIGH);
			actual_led_status = true;
			//write_EEPROM_status();
			Send_Relay_Status(1, current_relay_state);
		}
        else if(current_relay_state == 1)
		{
			current_relay_state = 0;
			digitalWrite(RELAY_OUT_PIN, LOW);
			digitalWrite(LED_PIN, LOW);
			actual_led_status = false;
			//write_EEPROM_status();
			Send_Relay_Status(1, current_relay_state);
		}
        previous_relay_state = current_relay_state;
		
		flag_ap_mode = false;
		time_ap_mode = 0;
		Send_AP_Mode();
		flag_ap_Mode_running = true;
		time_ap_mode_running = millis();
		led_blink_counter = 0;
	}
    
	if(flag_ap_Mode_running == true)
	{
		if(millis()-time_ap_mode_running > 300)
		{
			led_blink_counter++;
			time_ap_mode_running = millis();
			if(led_status == true)
			{
				digitalWrite(LED_PIN, LOW);
				led_status = false;
			}
			else
			{
				digitalWrite(LED_PIN, HIGH);
				led_status = true;
			}
		}
		if(led_blink_counter > 600)
		{
			digitalWrite(LED_PIN, LOW);
			flag_ap_Mode_running = false;
			if(actual_led_status == true)
			{
				digitalWrite(LED_PIN, HIGH);
			}
			else
			{
				digitalWrite(LED_PIN, LOW);
			}
		}
	}
	
	if(((PIND & _BV(PD2)) == 4) && wifi_btn_pressed == 0 && wifi_btn_released == 1  && flag_ap_Mode_running == false) // Turning ON Wi-Fi Button
	{
		wifi_btn_pressed = 1;
		wifi_btn_released = 0;
		wifi_btn_state = 1;
		Send_WiFi_status(wifi_btn_state);
	}
	else if(((PIND & _BV(PD2)) == 0) && wifi_btn_pressed == 1 && wifi_btn_released == 0  && flag_ap_Mode_running == false) // Turning OFF Wi-Fi Button
	{
		wifi_btn_pressed = 0;
		wifi_btn_released = 1;
		wifi_btn_state = 0;
		Send_WiFi_status(wifi_btn_state);
	}
	
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
					case DEV_TYPE_7:
					{
						switch(ID) // Start ID Switch --> BTNS or FAN
						{
							case ID_BTN:
							{
								switch(SeqNumber) // Start BTNs switch
								{
									case BTN_PWR:
									{
										if(child_lock_enabled == 0)
										{
											switch(btnState)// Start BTNs ON/OFF state switch
											{
												case BTN_STATE_OFF:
												{
													digitalWrite(RELAY_OUT_PIN, LOW);
													digitalWrite(LED_PIN, LOW);
													actual_led_status = false;
													current_relay_state = 0;
													previous_relay_state = current_relay_state;
													//write_EEPROM_status();
													break;
												}
												case BTN_STATE_ON:
												{
													digitalWrite(RELAY_OUT_PIN, HIGH);
													digitalWrite(LED_PIN, HIGH);
													actual_led_status = true;
													current_relay_state = 1;
													previous_relay_state = current_relay_state;
													//write_EEPROM_status();
													break;
												}
											}// END BTNs ON/OFF state switch
										}
										break;
									}
									default:
										break;
								} // END BTNs switch
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
							case ID_CHILD_LOCK:
							{
								switch(SeqNumber)
								{
									case SEQ_CHILD:
									{
										switch(btnState)
										{
											case BTN_STATE_OFF:
											{
												Disable_Child_Lock();
												child_lock_enabled = 0;
												//write_EEPROM_status();
												break;
											}
											case BTN_STATE_ON:
											{
												Enable_Child_Lock();
												child_lock_enabled = 1;
												//write_EEPROM_status();
												break;
											}
										}
										break;
									}
								}
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
		}
	}
}

void Enable_Power_BTN(void)
{
	DDRC   &= ~(1 << DDC1);       // Set PC1 as input (Using for interupt PCINT1)
    PORTC  |= (1 << PORTC1);     // Enable PC1 pull-up resistor
    cli();
    // Setting Pin Change ISR for Power Button
    PCMSK1 |= (1 << PCINT9);     // want pin PC1, PCINT9
    PCIFR  |= (1 << PCIF1);      // clear any outstanding interrupts
    PCICR  |= (1 << PCIE1);      // enable pin change interrupts for PCINT9
    sei(); // Enable the Global Interrupt Enable flag so that interrupts can be processed
}

void Enable_Child_Lock(void)
{
    // Disabling Pin Change ISR for Power Button
    PCMSK1 &= ~(1 << PCINT9);     // Clear pin PC1, PCINT9
    PCIFR  &= ~(1 << PCIF1);      // clear any outstanding interrupts
    PCICR  &= ~(1 << PCIE1);      // Disable pin change interrupts for PCINT9
}

void Disable_Child_Lock(void)
{
	// Enabling Pin Change ISR for Power Button
    PCMSK1 |= (1 << PCINT9);     // want pin PC1, PCINT9
    PCIFR  |= (1 << PCIF1);      // clear any outstanding interrupts
    PCICR  |= (1 << PCIE1);      // enable pin change interrupts for PCINT9
    sei(); // Enable the Global Interrupt Enable flag so that interrupts can be processed
}

void Enable_WiFi_BTN(void)
{
	DDRD   &= ~(1 << DDD2);       // Set PD2 as input (Using for Wi-Fi)
    //PORTD  |= (1 << PORTD2);     // Enable PC1 pull-up resistor
}

void Send_Relay_Status(byte relay_number, byte relay_status)
{
    sendMsg[0] = 0x2A;
    sendMsg[1] = 0x2A;
    sendMsg[2] = 0x2A;
    sendMsg[3] = 0xD7;
    sendMsg[4] = 0xF1;
    
    switch(relay_number)
    {
        case 1:
        {
            sendMsg[5] = 0xA0;          break;
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
    Serial.write(sendMsg, sizeof(sendMsg));
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
 
void Send_Current(float tempAMPS)
{
	sendMsg[0] = 0x2A;
    sendMsg[1] = 0x2A;
    sendMsg[2] = 0x2A;
    sendMsg[3] = 0xD7;
    sendMsg[4] = 0xF3;
	byte tempByte[4];
	*((float *)tempByte) = tempAMPS;
	sendMsg[5] = tempByte[0];
	sendMsg[6] = tempByte[1];
	sendMsg[7] = tempByte[2];
	sendMsg[8] = tempByte[3];
	
	Serial.write(sendMsg, sizeof(sendMsg));
}

void Send_WiFi_status(byte wifi_status)
{
	sendMsg[0] = 0x2A;
    sendMsg[1] = 0x2A;
    sendMsg[2] = 0x2A;
    sendMsg[3] = 0xD7;
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
	
	Serial.write(sendMsg, sizeof(sendMsg));
}

void Send_AP_Mode(void)
{
	sendMsg[0] = 0x2A;
    sendMsg[1] = 0x2A;
    sendMsg[2] = 0x2A;
    sendMsg[3] = 0xD7;
    sendMsg[4] = 0xF5;
	sendMsg[5] = 0x00;
	sendMsg[6] = 0x00;
	sendMsg[7] = 0x00;
	sendMsg[8] = 0x00;
	
	Serial.write(sendMsg, sizeof(sendMsg));
}

void Resume_Last_Power_State(void)
{
	current_relay_state = EEPROM.read(0);
	child_lock_enabled = EEPROM.read(1);
	
	if(child_lock_enabled == 1)
	{
		Enable_Child_Lock();
	}
	
	if(current_relay_state == 0)
	{
		digitalWrite(RELAY_OUT_PIN, LOW);
		digitalWrite(LED_PIN, LOW);
	}
	else if(current_relay_state == 1)
	{
		digitalWrite(RELAY_OUT_PIN, HIGH);
		digitalWrite(LED_PIN, HIGH);
	}
	//write_EEPROM_status();
	Send_Relay_Status(1, current_relay_state);
	previous_relay_state = current_relay_state;
}

void write_EEPROM_status(void)
{
    EEPROM.write(0, current_relay_state);
	EEPROM.write(1, child_lock_enabled);
}
