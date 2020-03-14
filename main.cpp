// Simple example where is demonstrated:

// How to declare the objects Esp32MAClientLog and Esp32MAClientSend
// How to configure: -log a variable and -connect to Machine advisors
// How to upate the two objects in the loop


#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>

#include "Esp32MAClientThread.hpp"
#include "secrets/secrets.h"

bool iniNTP();
static void InitWifi();
static bool isWifiOK();

// Please input the SSID and password of WiFi

const char* ssid     = SSID;
const char* password = SSIDPASS;

// NTP

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Machine Advisor

Esp32MAClientLog machineLog; // Log variables to a buffer
Esp32MAClientSend machineSend("ESP32", machineLog); // Send the buffer to Machine Advisor


// Aplication variables example

int temp=20;
int humid=10;
int volt=5;
unsigned long lastUpdate=0;


//////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

    Serial.begin(115200);
    Serial.println("Initializing...");

    InitWifi();
    iniNTP();

    // Machne Advisor connection credetials for "Machine"

    machineSend.setConnexionString(MACHINEBROKERURL, MACHINECLIENTID, MACHINEPASSWORD);
    while (!machineSend.connect()) Serial.print("C");

    // Register a variable with a sampling time of 10s

    machineLog.registerVar("humidity", &humid, 10000);
    
    // Aplication inits

    randomSeed(analogRead(0));
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////


void loop (){

    // Aplication example

    if ((millis() - lastUpdate) > 1000) {

        temp += random(-1,+1);
        humid += random(-1,+1);
        volt += random(-1, +1);

        lastUpdate = millis();
    }

    machineLog.update(timeClient.getEpochTime());
    machineSend.update(isWifiOK());
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////


bool iniNTP(){

    timeClient.begin();
    timeClient.setTimeOffset(3600);
    timeClient.setUpdateInterval(1000*60*100); // update time every 100 minutes

    bool actualitzada;
    while(true) {
        actualitzada = timeClient.update();
        if (!actualitzada) {
            timeClient.forceUpdate();
            Serial.print("t");
        } else break;
    }
    
    return(actualitzada);
}


static void InitWifi() {
    Serial.println(" > WiFi");
    Serial.println("Connecting...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}


static bool isWifiOK(){
    return (WiFi.status() == WL_CONNECTED);
}

