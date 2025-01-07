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
char ssid[] = "McDonalds Kiosk";
char pword[] = "00000000";

//US Sensor 1
#define trig_1 2
#define echo_1 4

//US Sensor 2
#define trig_2 27
#define echo_2 14

//US Sensor 3
#define trig_3 4
#define echo_3 5

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

TaskHandle_t task_1;
TaskHandle_t task_2;

void setup() 
{
    Serial.begin(115200);
    WiFi.begin(ssid, pword);

    pinMode(trig_1, OUTPUT);
    pinMode(echo_1, INPUT);
    pinMode(trig_2, OUTPUT);
    pinMode(echo_2, INPUT);
    pinMode(strain, INPUT);
    pinMode(in1, OUTPUT);
    pinMode(in2, OUTPUT);
    pinMode(pwm, OUTPUT);
    Blynk.begin(auth, ssid, pword, "blynk.cloud", 80);
    scale.begin(load_dout, load_sck);
    scale.set_scale(calibration_reading);
    timer.setInterval(1000L, sendData); 

    Serial.println(!pox.begin() ? "FAILED" : "SUCCESS");

    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);

    xTaskCreatePinnedToCore(getVitals, "task_1", 10000, NULL, 0, &task_1, 0);
    delay(500);
}

String vitals;
int value = 0;

void sendData()
{
    Blynk.virtualWrite(V0, getDistanceCm(trig_1, echo_1));
    Blynk.virtualWrite(V1, getDistanceCm(trig_2, echo_2));
    //Blynk.virtualWrite(V3, getStrainData());
    Blynk.virtualWrite(V4, getWeight());
    Blynk.virtualWrite(V5, vitals);
    Serial.println(getStrainData());
}

BLYNK_WRITE(V2)
{
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    value = map(param.asInt(), 0, 100, 0, 255);
    analogWrite(pwm, value);
}

float getDistanceCm(int trig, int echo)
{
    long duration = 0;
    float distance = 0;
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);

    duration = pulseIn(echo, HIGH);
    distance = duration * 0.017;
    return distance;
}

String getStrainData()    
{
    String strainData;
    float val = analogRead(strain);
    float percentage = map(value, 0, 1024, 0, 100);
    strainData = (String)val + " (" + (String)percentage + "%)";
    return strainData;
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
    }
}

void loop()
{
    Blynk.run();
    timer.run();
}
