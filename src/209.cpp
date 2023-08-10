/*
12 -- Light PWM
32 -- Light2 PWM
33 -- Pixel 1 pin
25 -- Pixel 2 pin
26 -- RST
27 -- DAT
14 -- CLK
*/

const char version[6] = "2.0.9";

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <DS1302.h>
#include <PubSubClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <EEPROM.h>

DS1302 rtc(26, 27, 14);
Time t;

const byte PIXEL1_PER_SEGMENT = 2;  	// Number of LEDs in each Segment
const byte PIXEL1_DIGITS = 4;       	// Number of connected Digits
const byte PIXEL1_DOTS = 2;
const byte PIXEL1_PIN = 33;         	// Pixel 1 Pin
const byte PIXEL2_PER_SEGMENT = 1;  	// Number of LEDs in each Segment
const byte PIXEL2_DIGITS = 4;       	// Number of connected Digits
const byte PIXEL2_DOTS = 2;
const byte PIXEL2_PIN = 25;         	// Pixel 2 Pin
const byte LIGHT_PIN = 12;				// Light PWM pin
const byte LIGHT2_PIN = 32;				// Light2 PWM pin

Adafruit_NeoPixel strip1 = Adafruit_NeoPixel((PIXEL1_PER_SEGMENT * 7 * PIXEL1_DIGITS) + PIXEL1_DOTS, PIXEL1_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2 = Adafruit_NeoPixel((PIXEL2_PER_SEGMENT * 7 * PIXEL2_DIGITS) + PIXEL2_DOTS, PIXEL2_PIN, NEO_GRB + NEO_KHZ800);

const char *ssid     = "BZ_IOT";
const char *password = "Password";

IPAddress local_IP(192, 168, 1, 205);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 1, 1);
IPAddress secondaryDNS(192, 168, 1, 3);

#define MQTT_HOST IPAddress(192, 168, 1, 3)

WiFiClient espClient;
PubSubClient client(espClient);
AsyncWebServer server(80);

byte sunriseMin = 0;
byte sunsetMin = 0;
bool nightMode = 1;
byte dsHr = 0;
byte dsMin = 0;
u16_t temperature = 0;
byte lightInten = 0;
byte light2Inten = 0;
byte newLightInten = 0;
byte oldLightinten = 0;
byte dayBgtn = 0;
byte nightBgtn = 0;
byte day2Bgtn = 0;
byte night2Bgtn = 0;
// EEPROM Address
byte lightIntenAddr = 0;
byte light2IntenAddr = 1;
byte dayBgtnAddr = 2;
byte nightBgtnAddr = 3;
byte day2BgtnAddr = 4;
byte night2BgtnAddr = 5;

bool blinkState = 0;
bool updated = 0;
byte page = 1;
bool freeze = 0;
bool clockUpdated = 0;
uint32_t hue = 32768;
unsigned long mqttESPinfo1;
unsigned long mqttESPinfo2;
unsigned long blinkDelay;
unsigned long pageHoldingTime;
unsigned long time_page;
unsigned long checkTime;
unsigned long reconnTime;
unsigned long time_now = 0;

//Digits array
byte digits1[12] = {
	//abcdefg
	0b1111011,      // 0
	0b0001001,      // 1
	0b1011110,      // 2
	0b0011111,      // 3
	0b0101101,      // 4
	0b0110111,      // 5
	0b1110111,      // 6
	0b0011001,      // 7
	0b1111111,      // 8
	0b0111111,      // 9
	0b1000110,      // c	(10)
	0b0000100,      // -	(11)
};
byte digits2[12] = {
	//abcdefg
	0b1111011,      // 0
	0b0001010,      // 1
	0b1011101,      // 2
	0b0011111,      // 3
	0b0101110,      // 4
	0b0110111,      // 5
	0b1110111,      // 6
	0b0011010,      // 7
	0b1111111,      // 8
	0b0111111,      // 9
	0b1000101,      // c	(10)
	0b0000100,      // -	(11)
};

// Clear all the Pixels
void clearDisplay() {
	for (int i = 0; i < strip1.numPixels(); i++) {
		strip1.setPixelColor(i, strip1.Color(0, 0, 0));
	}
	for (int i = 0; i < strip2.numPixels(); i++) {
		strip2.setPixelColor(i, strip2.Color(0, 0, 0));
	}
}

void writeDigit1(int index, int val, int r, int g, int b) {
	byte digit = digits1[val];
	int margin;
	if (index == 0 || index == 1 ) margin = 0;
	if (index == 2 || index == 3 ) margin = 1;
	if (index == 4 || index == 5 ) margin = 2;
	for (int i = 6; i >= 0; i--) {
	    int offset = index * (PIXEL1_PER_SEGMENT * 7) + i * PIXEL1_PER_SEGMENT;
	    uint32_t color;
	    if (digit & 0x01 != 0) {
	        if (index == 0 || index == 1 ) color = strip1.Color(r, g, b);
	        if (index == 2 || index == 3 ) color = strip1.Color(r, g, b);
	        if (index == 4 || index == 5 ) color = strip1.Color(r, g, b);
	    }
	    else
	        color = strip1.Color(0, 0, 0);

	    for (int j = offset; j < offset + PIXEL1_PER_SEGMENT; j++) {
	        strip1.setPixelColor(j, color);
	    }
	    digit = digit >> 1;
    }
}

void writeDigit1HSV(int index, int val, uint32_t hue) {
	byte digit = digits1[val];
	int margin;
	if (index == 0 || index == 1 ) margin = 0;
	if (index == 2 || index == 3 ) margin = 1;
	if (index == 4 || index == 5 ) margin = 2;
	for (int i = 6; i >= 0; i--) {
	    int offset = index * (PIXEL1_PER_SEGMENT * 7) + i * PIXEL1_PER_SEGMENT;
	    uint32_t color;
	    if (digit & 0x01 != 0) {
	        if (index == 0 || index == 1 ) color = strip1.ColorHSV(hue);
	        if (index == 2 || index == 3 ) color = strip1.ColorHSV(hue);
	        if (index == 4 || index == 5 ) color = strip1.ColorHSV(hue);
	    }
	    else
	        color = strip1.Color(0, 0, 0);

	    for (int j = offset; j < offset + PIXEL1_PER_SEGMENT; j++) {
	        strip1.setPixelColor(j, color);
	    }
	    digit = digit >> 1;
    }
}

void writeDigit2(int index, int val, int r, int g, int b) {
	byte digit = digits2[val];
	int margin;
	if (index == 0 || index == 1 ) margin = 0;
	if (index == 2 || index == 3 ) margin = 1;
	if (index == 4 || index == 5 ) margin = 2;
	for (int i = 6; i >= 0; i--) {
	    int offset = index * (PIXEL2_PER_SEGMENT * 7) + i * PIXEL2_PER_SEGMENT;
	    uint32_t color;
	    if (digit & 0x01 != 0) {
	        if (index == 0 || index == 1 ) color = strip2.Color(r, g, b);
	        if (index == 2 || index == 3 ) color = strip2.Color(r, g, b);
	        if (index == 4 || index == 5 ) color = strip2.Color(r, g, b);
	    }
	    else
	        color = strip2.Color(0, 0, 0);

	    for (int j = offset; j < offset + PIXEL2_PER_SEGMENT; j++) {
	        strip2.setPixelColor(j, color);
	    }
	    digit = digit >> 1;
    }
}

void writeDigit2HSV(int index, int val, uint32_t hue) {
	byte digit = digits2[val];
	int margin;
	if (index == 0 || index == 1 ) margin = 0;
	if (index == 2 || index == 3 ) margin = 1;
	if (index == 4 || index == 5 ) margin = 2;
	for (int i = 6; i >= 0; i--) {
	    int offset = index * (PIXEL2_PER_SEGMENT * 7) + i * PIXEL2_PER_SEGMENT;
	    uint32_t color;
	    if (digit & 0x01 != 0) {
	        if (index == 0 || index == 1 ) color = strip2.ColorHSV(hue);
	        if (index == 2 || index == 3 ) color = strip2.ColorHSV(hue);
	        if (index == 4 || index == 5 ) color = strip2.ColorHSV(hue);
	    }
	    else
	        color = strip2.Color(0, 0, 0);

	    for (int j = offset; j < offset + PIXEL2_PER_SEGMENT; j++) {
	        strip2.setPixelColor(j, color);
	    }
	    digit = digit >> 1;
    }
}

void getTime() {
	t = rtc.getTime();
	dsHr = t.hour;
	dsMin = t.min;
}

void disp_Time() {
	clearDisplay();
	if (dsHr / 10 > 0) {
		writeDigit1(0, dsHr / 10, 255, 128, 0);
		writeDigit2(0, dsHr / 10, 255, 128, 0);
	}
	writeDigit1(1, dsHr % 10, 255, 128, 0);
	writeDigit1(2, dsMin / 10, 255, 128, 0);
	writeDigit1(3, dsMin % 10, 255, 128, 0);
	writeDigit2(1, dsHr % 10, 255, 128, 0);
	writeDigit2(2, dsMin / 10, 255, 128, 0);
	writeDigit2(3, dsMin % 10, 255, 128, 0);
}

void setBlink(bool isTemp, int r, int g, int b) {
	if (isTemp) {
		strip1.setPixelColor((PIXEL1_PER_SEGMENT * 7 * PIXEL1_DIGITS), strip1.Color(r, g, b));
		strip2.setPixelColor((PIXEL2_PER_SEGMENT * 7 * PIXEL2_DIGITS), strip2.Color(r, g, b));
	} else {
		strip1.setPixelColor((PIXEL1_PER_SEGMENT * 7 * PIXEL1_DIGITS), strip1.Color(r, g, b));
		strip1.setPixelColor((PIXEL1_PER_SEGMENT * 7 * PIXEL1_DIGITS) + 1, strip1.Color(r, g, b));
		strip2.setPixelColor((PIXEL2_PER_SEGMENT * 7 * PIXEL2_DIGITS), strip2.Color(r, g, b));
		strip2.setPixelColor((PIXEL2_PER_SEGMENT * 7 * PIXEL2_DIGITS) + 1, strip2.Color(r, g, b));
	}
}

void setBlinkHSV(bool isTemp, uint32_t hue) {
	if (isTemp) {
		strip1.setPixelColor((PIXEL1_PER_SEGMENT * 7 * PIXEL1_DIGITS), strip1.ColorHSV(hue));
		strip2.setPixelColor((PIXEL2_PER_SEGMENT * 7 * PIXEL2_DIGITS), strip2.ColorHSV(hue));
	} else {
		strip1.setPixelColor((PIXEL1_PER_SEGMENT * 7 * PIXEL1_DIGITS), strip1.ColorHSV(hue));
		strip1.setPixelColor((PIXEL1_PER_SEGMENT * 7 * PIXEL1_DIGITS) + 1, strip1.ColorHSV(hue));
		strip2.setPixelColor((PIXEL2_PER_SEGMENT * 7 * PIXEL2_DIGITS), strip2.ColorHSV(hue));
		strip2.setPixelColor((PIXEL2_PER_SEGMENT * 7 * PIXEL2_DIGITS) + 1, strip2.ColorHSV(hue));
	}
}

void dispTemp() {
	clearDisplay();
	if (temperature > 999 || temperature < 1) {
		// Strip 1
		writeDigit1(0, 11, 0, 255, 0);	// -
		writeDigit1(1, 11, 0, 255, 0);	// -
		writeDigit1(2, 11, 0, 255, 0);	// -
		writeDigit1(3, 10, 0, 255, 0);	// c
		// Strip 2
		writeDigit2(0, 11, 0, 255, 0);	// -
		writeDigit2(1, 11, 0, 255, 0);	// -
		writeDigit2(2, 11, 0, 255, 0);	// -
		writeDigit2(3, 10, 0, 255, 0);	// c
	}
	else {
		// Strip 1
		writeDigit1HSV(0, temperature / 100, hue);
		writeDigit1HSV(1, (temperature / 10) % 10, hue);
		writeDigit1HSV(2, temperature % 10, hue);
		writeDigit1HSV(3, 10, hue);		// c
		// Strip 2
		writeDigit2HSV(0, temperature / 100, hue);
		writeDigit2HSV(1, (temperature / 10) % 10, hue);
		writeDigit2HSV(2, temperature % 10, hue);
		writeDigit2HSV(3, 10, hue);		// c
	}
}

void connectToWifi() {
	Serial.println("Connecting to Wi-Fi...");
	if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
	{
		Serial.println("STA Failed to configure");
	}
	WiFi.begin(ssid, password);
	WiFi.setAutoReconnect(true);
	WiFi.persistent(true);
	Serial.print("Connected to ");
	Serial.println(WiFi.SSID());
}

void callback(char* topic, byte* message, unsigned int length) {
    String messageTemp;

    for (int i = 0; i < length; i++) {
        Serial.print((char)message[i]);
        messageTemp += (char)message[i];
    }

    DynamicJsonDocument doc(512);
    deserializeJson(doc, messageTemp);
    
    if (String(topic) == "esp/clock/get") {
        
        bool flagUpdateTemp = doc["temp"]["flag"];
        if (flagUpdateTemp == 1) {
        	temperature = doc["temp"]["value"];
			hue			= doc["temp"]["hue"];
        }

        bool flagLightInten = doc["lightInten"]["flag"];
		if (flagLightInten == 1) {
			byte intLightInten = doc["lightInten"]["value"];
			newLightInten = intLightInten;
			oldLightinten = lightInten;
			EEPROM.put(lightIntenAddr, intLightInten);
			EEPROM.commit();
		}

        bool flagLight2Inten = doc["light2Inten"]["flag"];
        if (flagLight2Inten == 1) {
        	byte intLight2inten = doc["light2Inten"]["value"];
            light2Inten = intLight2inten;
			EEPROM.put(light2IntenAddr, light2Inten);
			EEPROM.commit();
            analogWrite(LIGHT2_PIN, intLight2inten);
        }

        bool flagUpdateTime = doc["time"]["flag"];
        if (flagUpdateTime == 1) {
			byte Second	= doc["time"]["Second"];
			byte Minute	= doc["time"]["Minute"];
			byte Hour	= doc["time"]["Hour"];
			byte Day	= doc["time"]["Day"];
			byte Month	= doc["time"]["Month"];
			int Year	= doc["time"]["Year"];
			if (Hour != dsHr || Minute != dsMin) {
				rtc.setTime(Hour, Minute, Second);
				rtc.setDate(Day, Month, Year);
			}
			DynamicJsonDocument docSend(64);
            String MQTT_STR;
			docSend["time"]["status"]	= "OK";
			docSend["time"]["msg"]		= "updated";
			serializeJson(docSend, MQTT_STR);
            client.publish("esp/clock/savestatus", MQTT_STR.c_str());
        }

		bool flagNightBrightness = doc["bgtn"]["night"]["flag"];
		if (flagNightBrightness == 1) {
			nightBgtn = doc["bgtn"]["night"]["value"];
			EEPROM.put(nightBgtnAddr, nightBgtn);
			EEPROM.commit();
			DynamicJsonDocument docSend(64);
			String MQTT_STR;
			docSend["bgtn"]["night"]["status"]	= "OK";
			docSend["bgtn"]["night"]["msg"]		= nightBgtn;
			serializeJson(docSend, MQTT_STR);
			client.publish("esp/clock/savestatus", MQTT_STR.c_str());
		}

		bool flagDayBrightness = doc["bgtn"]["day"]["flag"];
		if (flagDayBrightness == 1) {
			dayBgtn = doc["bgtn"]["day"]["value"];
			EEPROM.put(dayBgtnAddr, dayBgtn);
			EEPROM.commit();
			DynamicJsonDocument docSend(64);
			String MQTT_STR;
			docSend["bgtn"]["day"]["status"]	= "OK";
			docSend["bgtn"]["day"]["msg"]		= dayBgtn;
			serializeJson(docSend, MQTT_STR);
			client.publish("esp/clock/savestatus", MQTT_STR.c_str());
		}

		bool flagNight2Brightness = doc["bgtn"]["night2"]["flag"];
		if (flagNight2Brightness == 1) {
			night2Bgtn = doc["bgtn"]["night2"]["value"];
			EEPROM.put(night2BgtnAddr, night2Bgtn);
			EEPROM.commit();
			DynamicJsonDocument docSend(64);
			String MQTT_STR;
			docSend["bgtn"]["night2"]["status"]	= "OK";
			docSend["bgtn"]["night2"]["msg"]	= night2Bgtn;
			serializeJson(docSend, MQTT_STR);
			client.publish("esp/clock/savestatus", MQTT_STR.c_str());
		}

		bool flagDay2Brightness = doc["bgtn"]["day2"]["flag"];
		if (flagDay2Brightness == 1) {
			day2Bgtn = doc["bgtn"]["day2"]["value"];
			EEPROM.put(day2BgtnAddr, day2Bgtn);
			EEPROM.commit();
			DynamicJsonDocument docSend(64);
			String MQTT_STR;
			docSend["bgtn"]["day2"]["status"]	= "OK";
			docSend["bgtn"]["day2"]["msg"]		= day2Bgtn;
			serializeJson(docSend, MQTT_STR);
			client.publish("esp/clock/savestatus", MQTT_STR.c_str());
		}
    }
}

void LightUpdate(int state) {
	if (newLightInten > oldLightinten) lightInten++;
	else if (newLightInten < oldLightinten) lightInten--;
	if (state == 0) {
		analogWrite(LIGHT_PIN, 0);
	}
	else if (state == 1) {
		analogWrite(LIGHT_PIN, lightInten);
	}
}

void reconnect() {
    if (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("ESP32Client")) {
            Serial.println("connected");
            client.subscribe("esp/clock/get");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
        }
    }
}

void setup() {
	Serial.begin(115200);
	Serial.print("Setting PIN");
	EEPROM.begin(256);
	EEPROM.get(lightIntenAddr, lightInten);
	EEPROM.get(light2IntenAddr, light2Inten);
	EEPROM.get(dayBgtnAddr, dayBgtn);
	EEPROM.get(nightBgtnAddr,nightBgtn);
	EEPROM.get(day2BgtnAddr, day2Bgtn);
	EEPROM.get(night2BgtnAddr,night2Bgtn);
	pinMode(LIGHT_PIN, OUTPUT);
    pinMode(LIGHT2_PIN, OUTPUT);
	analogWrite(LIGHT2_PIN, light2Inten);
	for (int i=lightInten; i>0; i--) {
		analogWrite(LIGHT_PIN, i);
		delay(10);
	}
	for (int i=0; i<lightInten; i++) {
		analogWrite(LIGHT_PIN, i);
		delay(10);
	}
	strip1.setBrightness(nightBgtn);
	strip2.setBrightness(night2Bgtn);
	strip1.begin();
	strip2.begin();
	connectToWifi();
	client.setServer(MQTT_HOST, 1883);
	client.setCallback(callback);
	rtc.halt(false);
	rtc.writeProtect(false);
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    	request->send(200, "text/plain", "Hi! I am ESP32. ESP32-Clock\nVersion: " + String(version));
	});

	AsyncElegantOTA.begin(&server);    // Start ElegantOTA
	server.begin();
	Serial.println("HTTP server started");
	Serial.print("OK");
	for (int i = 0; i < 12; i++) {
		writeDigit1(0, i, 0, 255, 0);
		writeDigit1(1, i, 0, 255, 0);
		writeDigit1(2, i, 0, 255, 0);
		writeDigit1(3, i, 0, 255, 0);
		writeDigit2(0, i, 0, 255, 0);
		writeDigit2(1, i, 0, 255, 0);
		writeDigit2(2, i, 0, 255, 0);
		writeDigit2(3, i, 0, 255, 0);
		delay(100);
	}
	delay(1000);
}

void loop() {
    client.loop();

	if (!client.connected()) {
		if (millis() - reconnTime > 1000) {
			reconnect();
			reconnTime = millis();
		}
	} 

	if (millis() - checkTime > 5000) {
		if ((t.mon > 2) && (t.mon < 9)) {
			sunriseMin = 20;
			sunsetMin = 15;
		}
		else {
			sunriseMin = 40;
			sunsetMin = 5;
		}
		if ((t.hour > 5) && (t.hour < 19)) {
			if (t.hour == 6) {
				if (t.min > sunriseMin) {
					LightUpdate(0);
					nightMode = 0;
				}
				else {
					LightUpdate(1);
					nightMode = 1;
				}
			}
			else if (t.hour == 18) {
				if (t.min < sunsetMin) {
					LightUpdate(0);
					nightMode = 0;
				}
				else {
					LightUpdate(1);
					nightMode = 1;
				}
			}
			else {
				LightUpdate(0);
				nightMode = 0;
			}
		}
		else {
			LightUpdate(1);
			nightMode = 1;
		}
		checkTime = millis();
	}
	// Store time when page 1 or page 2 is active
	if (page == 1 && updated == 0) {
		time_page == millis();
		updated = 1;
	}
	else if (page == 2 && updated == 1) {
		time_page = millis();
		updated = 0;
	}

	// Calculate stored time
	pageHoldingTime = millis() - time_page;

	if (pageHoldingTime > 10000) {
		page = 2;
		pageHoldingTime = 0;
	}
	else if (pageHoldingTime > 3000) {
		page = 1;
		pageHoldingTime = 0;
	}
	
	// Nightmode check
	if (nightMode == 1) {
		strip1.setBrightness(nightBgtn);
		strip2.setBrightness(night2Bgtn);
	}
	else {
		strip1.setBrightness(dayBgtn);
		strip2.setBrightness(day2Bgtn);
	}

	// Page 1 is active
	if (page == 1) {
		if ((millis() - time_now > 2000) && clockUpdated == 0) {
			time_now = millis();
			getTime();
			disp_Time();
			strip1.show();
			strip2.show();
			clockUpdated = 1;	// freeze clock update
		}
		if (millis() - blinkDelay > 1000) {
			if (blinkState == 0) {
				blinkState = 1;
				setBlink(false, 100, 20, 0);
			}
			else {
				blinkState = 0;
				setBlink(false, 0, 0, 0);
			}
			strip1.show();
			strip2.show();
			blinkDelay = millis();
		}
		freeze = 0;	// unfreeze page 2
	}
	// Page 2 is active
	else if (page == 2 && freeze == 0) {
		dispTemp();
		setBlinkHSV(true, hue);
		strip1.show();
		strip2.show();
		freeze = 1;			// freeze page 2
		clockUpdated = 0;	// unfreeze clock update
	}

	if (millis() - mqttESPinfo1 > 1000) {
		DynamicJsonDocument docInfo(192);
		String MQTT_STR;
		docInfo["rssi"] = String(WiFi.RSSI());
		docInfo["uptime"] = String(millis()/1000);
		docInfo["datetime"] = String(dsHr/10) + String(dsHr%10) + ":" + String(dsMin/10) + String(dsMin%10);
		docInfo["lightInten"] = String(lightInten);
		docInfo["light2Inten"] = String(light2Inten);
		serializeJson(docInfo, MQTT_STR);
		client.publish("esp/clock/info", MQTT_STR.c_str());
		mqttESPinfo1 = millis();
	}
	if (millis() - mqttESPinfo2 > 300000) {		// 5 min
		DynamicJsonDocument docInfo(64);
		String MQTT_STR;
		docInfo["lightInten"] = String(lightInten);
		docInfo["light2Inten"] = String(light2Inten);
		serializeJson(docInfo, MQTT_STR);
		client.publish("esp/clock/chart", MQTT_STR.c_str());
		mqttESPinfo2 = millis();
	}
}
