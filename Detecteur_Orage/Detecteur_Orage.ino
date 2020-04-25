/*
  Station Meteo Pro - module detecteur d'orage
  avec : 
     - NodeMCU
     - Module AS3935 en I2C

 Cablage AS3935 :
 VCC - 3V
 GND - GND 
 SCL - D1
 SDA - D2
 IRQ - D8 (GPIO15)
 SI - 3V

  
  Source :     https://www.sla99.fr
  Site météo : https://www.meteo-four38.fr
  Date : 2019-09-05

  Changelog : 
  25/04/2020  v1    version initiale

  Inspiré de la librairie AS3935MI de Gregor Christandl

*/

#include <Arduino.h>
#include <Wire.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>


#include <AS3935I2C.h>

#define PIN_IRQ 15 // = D8
#define LED_TRANSMISSION 14 //D5 ; jaune
#define LED_STORM 12        //D6 ; bleu
#define LED_ERROR 13        //D7 ; rouge

//create an AS3935 object using the I2C interface, I2C address 0x01 and IRQ pin number 2
AS3935I2C as3935(AS3935I2C::AS3935I2C_A11, PIN_IRQ);
//0x01 = AS3935I2C_A01
//0x02 = AS3935I2C_A10
//0x03 = AS3935I2C_A11

//this value will be set to true by the AS3935 interrupt service routine.
volatile bool interrupt_ = false;

const char* ssid = "xxxxx";
const char* password = "xxxxx";
char server[] = "192.168.1.2";  
WiFiClient client;
String KEY_WS="45698753349560223"; 

int storm = 0;
int dist = 0;
int energy = 0;

unsigned long previousMillis=   0;
unsigned long delaiProgramme =  60000L;   //60 sec

void setup() {
  // put your setup code here, to run once:
	Serial.begin(9600);

  pinMode(LED_TRANSMISSION, OUTPUT); 
  pinMode(LED_STORM, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);

  digitalWrite(LED_TRANSMISSION, HIGH);
  digitalWrite(LED_STORM, HIGH);
  digitalWrite(LED_ERROR, HIGH);

  delay(1000);

  digitalWrite(LED_TRANSMISSION, LOW);
  digitalWrite(LED_STORM, LOW);
  digitalWrite(LED_ERROR, LOW);
  

  delay(10);
  
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());   
  Serial.println(WiFi.macAddress());

  //sequence led reseau OK
  digitalWrite(LED_TRANSMISSION, HIGH);
  delay(500);
  digitalWrite(LED_TRANSMISSION, LOW);
  delay(500);
  digitalWrite(LED_TRANSMISSION, HIGH);
  delay(500);
  digitalWrite(LED_TRANSMISSION, LOW);
  delay(500);

 
  Serial.println("Start module");

	//wait for serial connection to open (only necessary on some boards)
	while (!Serial);  

	//set the IRQ pin as an input pin. do not use INPUT_PULLUP - the AS3935 will pull the pin 
	//high if an event is registered.
	pinMode(PIN_IRQ, INPUT);

	Wire.begin();

	//begin() checks the Interface and I2C Address passed to the constructor and resets the AS3935 to 
	//default values.
	if (!as3935.begin())
	{
		Serial.println("begin() failed. Check the I2C address passed to the AS3935I2C constructor. ");
		while (1);
	}

	//check I2C connection.
	if (!as3935.checkConnection())
	{
		Serial.println("checkConnection() failed. check your I2C connection and I2C Address. ");
		while (1);
	}
	else
		Serial.println("I2C connection check passed. ");

	//check the IRQ pin connection.
	if (!as3935.checkIRQ())
	{
		Serial.println("checkIRQ() failed. check if the correct IRQ pin was passed to the AS3935I2C constructor. ");
		while (1);
	}
	else
		Serial.println("IRQ pin connection check passed. ");

	//calibrate the resonance frequency. failing the resonance frequency could indicate an issue 
	//of the sensor. resonance frequency calibration will take about 1.7 seconds to complete.
	int32_t frequency = 0;
	if (!as3935.calibrateResonanceFrequency(frequency))
	{
		Serial.println("Resonance Frequency Calibration failed. ");
		while (1);
	}
	else
		Serial.println("Resonance Frequency Calibration passed. ");

	Serial.print("Resonance Frequency is "); Serial.print(frequency); Serial.println("Hz");

	//calibrate the RCO.
	if (!as3935.calibrateRCO())
	{
		Serial.println("RCO Calibration failed. ");
		while (1);
	}
	else
		Serial.println("RCP Calibration passed. ");

	//set the analog front end to 'indoors'
	as3935.writeAFE(AS3935MI::AS3935_INDOORS);
  //outdoor = AS3935_OUTDOORS

	//set default value for noise floor threshold
	as3935.writeNoiseFloorThreshold(AS3935MI::AS3935_NFL_2);

	//set the default Watchdog Threshold
	as3935.writeWatchdogThreshold(AS3935MI::AS3935_WDTH_2);

	//set the default Spike Rejection 
	as3935.writeSpikeRejection(AS3935MI::AS3935_SREJ_2);

	//write default value for minimum lightnings (1)
	as3935.writeMinLightnings(AS3935MI::AS3935_MNL_5);
  //AS3935_MNL_1 or AS3935_MNL_5 or AS3935_MNL_9 or AS3935_MNL_16

	//do not mask disturbers
	as3935.writeMaskDisturbers(false);

	//the AS3935 will pull the interrupt pin HIGH when an event is registered and will keep it 
	//pulled high until the event register is read.
	attachInterrupt(digitalPinToInterrupt(PIN_IRQ), AS3935ISR, RISING);

	Serial.println("Initialization complete, waiting for events...");
   
   //sequence led AS3935 OK
  digitalWrite(LED_STORM, HIGH);
  delay(500);
  digitalWrite(LED_STORM, LOW);
  delay(500);
  digitalWrite(LED_STORM, HIGH);
  delay(500);
  digitalWrite(LED_STORM, LOW);
  delay(500);
}

void loop() {
   unsigned long currentMillis = millis(); 

     
   if (currentMillis - previousMillis > delaiProgramme){
     previousMillis = millis();     

      String url = "/stationmeteo/storm.php?key="+KEY_WS+"&storm="+storm+"&distance="+dist+"&energy="+energy;
    
      HTTPClient http;  
      String Link = "http://192.168.1.2:81"+url;
      
      http.begin(Link); 
      
      int httpCode = http.GET();          
      String payload = http.getString();  
     
      Serial.println(httpCode);   
      Serial.println(payload);  

      if(httpCode != 200){
        digitalWrite(LED_TRANSMISSION, LOW);
        digitalWrite(LED_ERROR, HIGH);
        
      }
      if(httpCode == 200){
        digitalWrite(LED_TRANSMISSION, HIGH);
        delay (250);
        digitalWrite(LED_TRANSMISSION, LOW);
        delay (250);
        digitalWrite(LED_TRANSMISSION, HIGH);
        digitalWrite(LED_ERROR, LOW);
      }     
      http.end(); 

    //RAZ valeurs
    storm = 0;
    dist = 0;
    energy = 0;
    digitalWrite(LED_STORM, LOW);
   }
 
	if (interrupt_)
	{
		//the Arduino should wait at least 2ms after the IRQ pin has been pulled high
		delay(2);

		//reset the interrupt variable
		interrupt_ = false;

		//query the interrupt source from the AS3935
		uint8_t event = as3935.readInterruptSource();

		//send a report if the noise floor is too high. 
		if (event == AS3935MI::AS3935_INT_NH)
		{
			Serial.println("Noise floor too high. attempting to increase noise floor threshold. ");

			//if the noise floor threshold setting is not yet maxed out, increase the setting.
			//note that noise floor threshold events can also be triggered by an incorrect
			//analog front end setting.
			if (as3935.increaseNoiseFloorThreshold())
				Serial.println("increased noise floor threshold");
			else
				Serial.println("noise floor threshold already at maximum");
		}

		//send a report if a disturber was detected. if disturbers are masked with as3935.writeMaskDisturbers(true);
		//this event will never be reported.
		else if (event == AS3935MI::AS3935_INT_D)
		{

			Serial.println("Disturber detected. attempting to increase noise floor threshold. ");

			//increasing the Watchdog Threshold and / or Spike Rejection setting improves the AS3935s resistance 
			//against disturbers but also decrease the lightning detection efficiency (see AS3935 datasheet)
			uint8_t wdth = as3935.readWatchdogThreshold();
			uint8_t srej = as3935.readSpikeRejection();

			if ((wdth < AS3935MI::AS3935_WDTH_10) || (srej < AS3935MI::AS3935_SREJ_10))
			{
				//alternatively increase spike rejection and watchdog threshold 
				if (srej < wdth)
				{
					if (as3935.increaseSpikeRejection())
						Serial.println("increased spike rejection ratio");
					else
						Serial.println("spike rejection ratio already at maximum");
				}
				else
				{
					if (as3935.increaseWatchdogThreshold())
						Serial.println("increased watchdog threshold");
					else
						Serial.println("watchdog threshold already at maximum");
				}
			}
			else
			{
				Serial.println("error: Watchdog Threshold and Spike Rejection settings are already maxed out.");
			}
		}

		else if (event == AS3935MI::AS3935_INT_L)
		{     
			Serial.print("Lightning detected! Storm Front is ");
			Serial.print(as3935.readStormDistance());
			Serial.println("km away.");

      dist = as3935.readStormDistance();
      energy = as3935.readEnergy();
      storm += 1;
      digitalWrite(LED_STORM, HIGH);
      delay (250);
      digitalWrite(LED_STORM, LOW);
      delay (250);
      digitalWrite(LED_STORM, HIGH);
     
		}
	}
}

//interrupt service routine. this function is called each time the AS3935 reports an event by pulling 
//the IRQ pin high.
void AS3935ISR()
{
	interrupt_ = true;
}
