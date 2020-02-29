// Same of main_full, but with Autoconnect feature to connect to any wifi.

#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include "Esp32MAClientThread.hpp"

// Autoconnect and WEB Server

#include <WebServer.h>
#include <AutoConnect.h>

// Application

#include "secrets/secrets.h"
bool iniNTP();
static bool isWifiOK();

// Autoconnect

WebServer Server;
AutoConnect Portal(Server);
AutoConnectConfig Config;

// NTP

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Machine Advisor

Esp32MAClientLog machineLog; // Log variables to a buffer
Esp32MAClientSend machineSend("ESP32", machineLog); // Send the buffer to Machine Advisor

// Tasks definition

TaskHandle_t handTskUploading;
TaskHandle_t handTskWebPortal;
void tskUploading (void *pvParameters);
void tskWebPortal (void *pvParameters);

// Aplication variables example

int temp=20;
int humid=1000;
int volt=5;
unsigned long lastUpdate=0;


//////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

    Serial.begin(115200);
    Serial.println("Initializing...");

    Config.title = "Menú WiFi - Rellotge";
    Config.apid = AUTOCONNECTSSID;
    Config.psk  = AUTOCONNECTPASS;
    Config.autoReconnect = true; 
    Portal.config(Config);
    
    if (Portal.begin()) {
        Serial.println("WiFi connected: " + WiFi.localIP().toString());
    };

    iniNTP();

    // Machne Advisor connection credetials for "Machine"

    machineSend.setConnexionString(MACHINEBROKERURL, MACHINECLIENTID, MACHINEPASSWORD);
    while (!machineSend.connect()) Serial.print("C");
    
    // ClientSend / Web Portal will be declared in a independent task pinned to the second core (0). 
    // (Loop is always pinned to core (1))

    xTaskCreateUniversal(tskUploading, "TaskUpload", 10000, NULL, 2, &handTskUploading, 0);
    xTaskCreateUniversal(tskWebPortal, "TaskWebPortal", 10000, NULL, 1, &handTskWebPortal, 0);

    delay(500); 

    // Example 1:
    // Register a variable with a sampling time of 20s

    machineLog.registerVar("humidity", &humid, 20000);

    // Example 2:
    // Register a variable with a minimum sampling time of 5s,
    // but sample it only if the variable change is more than 5 units

    machineLog.registerVar("temperature", &temp, 5000, 5);

    // Example 3:
    // Register a variable with a minimum sample time of 5s
    // but sample it only if the variable change is more than 2 units
    // If the variable hasn't changed enough for 30s, sample it anyway

    machineLog.registerVar("voltage", &volt, 5000, 20, 30000);

    // Example 4:
    // Print some machine data stored in MA

    machineSend.setMASessionCookie(COOKIEKEY);
    
    // Display the last 5 minutes of variable humitity. Use makeTS method to set a concrete date/time

    unsigned long tsIni = timeClient.getEpochTime()-60*5;
    unsigned long tsEnd = timeClient.getEpochTime();

    machineSend.downloadCsv("ESP32", "humidity", tsIni, tsEnd);
    machineSend.printCsv();

    // Aplication inits

    randomSeed(analogRead(0));
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////

// Loop is pinned to core 1 by default

void loop (){

    // Aplication example

    if ((millis() - lastUpdate) > 1000) {

        temp += random(-1,+1);
        humid += random(-1,+1);
        volt += random(-1, +1);

        lastUpdate = millis();
    }

    machineLog.update(timeClient.getEpochTime());
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////


void tskUploading (void *pvParameters){

    Serial.println("Task: Uploading to MA running on core " + String(xPortGetCoreID()));

    while (true) {
        machineSend.update(isWifiOK());
    }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

void tskWebPortal (void *pvParameters){

    Serial.println("Task: Web Portal running on core " + String(xPortGetCoreID()));

    while (true) {
        Portal.handleClient();
        vTaskDelay(10);
    }

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



static bool isWifiOK(){
    return (WiFi.status() == WL_CONNECTED);
}

