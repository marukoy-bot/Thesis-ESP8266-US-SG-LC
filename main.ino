#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6yNIZeYyD"
#define BLYNK_TEMPLATE_NAME "Thesis"
#define BLYNK_AUTH_TOKEN "OVtAkBjO2ES1bBrmj56ooWVQkmlRTfwN"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <HX711.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"

char auth[] = BLYNK_AUTH_TOKEN;
// char ssid[] = "Pa Load Bala";
// char pword[] = "1234567890";
// char ssid[] = "IPhone 15 Pro Max 1tb Fully Paid";
// char pword[] = "uslt4527";
// char ssid[] = "McDonalds Kiosk";
// char pword[] = "00000000";
char ssid[] = "ESP32_Hotspot";
char pword[] = "12345678";

//Laser Sensor
#define laserSensor 35

//pump
#define in1 33
#define in2 32
#define pwm 26

//Strain Gauge Sensor
#define strain 15

//Load Cell Sensor
#define load_sck 18
#define load_dout 19

//Load Cell Sensor
#define calibration_reading -4279.9
#define REPORTING_PERIOD_MS 1000

BlynkTimer timer;
HX711 scale;
PulseOximeter pox;

bool connected = false;
uint32_t tsLastReport = 0;
unsigned long lastMillis = 0;
const int interval = 60000;
int LDRval = 0;
const int LDRThreshold = 3600;
int dropletCount = 0;
unsigned long startTime = 0;
float dropletsPerMinute = 0;

TaskHandle_t task_1;
TaskHandle_t task_2;

void setup() 
{
    Serial.begin(115200);
    WiFi.softAP(ssid, pword);
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    pinMode(LDR, INPUT);
    pinMode(in1, OUTPUT);
    pinMode(in2, OUTPUT);
    pinMode(pwm, OUTPUT);

    Blynk.config(auth);
    Blynk.begin(auth);

    //Blynk.begin(auth, ssid, pword, "blynk.cloud", 80);
    scale.begin(load_dout, load_sck);
    scale.set_scale(calibration_reading);
    timer.setInterval(1000L, sendData); 

    Serial.println(!pox.begin() ? "FAILED" : "SUCCESS");

    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);

    xTaskCreatePinnedToCore(getVitals, "task_1", 10000, NULL, 0, &task_1, 0);
    delay(500);
    startTime = millis();
}

String vitals;
int value = 0;

void sendData()
{
    Blynk.virtualWrite(V0, dropletsPerMinute);
    Blynk.virtualWrite(V4, getWeight());
    Blynk.virtualWrite(V5, vitals);
}

BLYNK_WRITE(V2)
{
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    value = map(param.asInt(), 0, 100, 0, 255);
    analogWrite(pwm, value);
}

float getWeight()
{
    return scale.get_units(20);
}

void getVitals(void * params)
{
    Serial.print("getVitals(): core ");
    Serial.println(xPortGetCoreID());
    for(;;)
    {
        pox.update();
        if (millis() - tsLastReport > REPORTING_PERIOD_MS)
        {
            vitals = (String)pox.getHeartRate() + " bpm | " + (String)pox.getSpO2() + "%";
            tsLastReport = millis();
        }


        // unsigned long elapsedTime = millis() - startTime;
        // dropletsPerMinute = (dropletCount/ (elapsedTime / 60000.0));

        static unsigned long lastPrintTime = 0;

        if(millis() - lastPrintTime >= 60000)
        {
            dropletsPerMinute = dropletCount;
            dropletCount = 0;
            //Serial.println(dropletCount);
            lastPrintTime = millis();
        }
    }
}

void loop()
{
    Blynk.run();
    timer.run();
}
