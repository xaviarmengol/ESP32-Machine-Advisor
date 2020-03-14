// Complete example where is demonstrated:

// How to declare the objects Esp32MAClientLog and Esp32MAClientSend
// How to use the option of a SD card, to extend the buffer up to the SD capacity
// How to create a specific task for Esp32MAClientSent and execute it in a separated core (0)
// How to register diferent variables with diferent options
// How to start loging even if there is no conection
// How to connect to Machine Advisor
// How to dowload data from Machine Advisor
// How to update the Log task in the main loop
// How to update de Sending task, and all wifi conection, in a specific task


#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>

#include "Esp32MAClient.hpp"
#include "secrets/secrets.h" // Replace with your credentials

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

Esp32MAClientLog machineLog(ESP32MALOG_SD); // Log variables to a buffer
Esp32MAClientSend machineSend("ESP32", machineLog); // Send the buffer to Machine Advisor

// Tasks definition

TaskHandle_t handTskUploading;
void tskUploading (void *pvParameters);

// Aplication variables example

int temp=20;
int humid=1000;
int volt=5;
unsigned long lastUpdate=0;


//////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

    Serial.begin(115200);
    Serial.println("Initializing...");
    
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


    // All code related to the conection to Machine Advisor is executed in core 0.
    // (Standard loop is always pinned to core 1)

    xTaskCreateUniversal(tskUploading, "TaskUpload", 10000, NULL, 1, &handTskUploading, 0);
    delay(500); 

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

    // Time Stamp: Use NTP if ready. If not, use seconds from startup

    unsigned long ts = timeClient.update() ? timeClient.getEpochTime() : (millis() / 1000);

    machineLog.update(ts);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////


void tskUploading (void *pvParameters){

    // SETUP or the task

    Serial.println("Task: Uploading to MA running on core " + String(xPortGetCoreID()));

    // Conection to Network

    InitWifi();
    iniNTP();

    // Machne Advisor connection credetials for "Machine"

    machineSend.setConnexionString(MACHINEBROKERURL, MACHINECLIENTID, MACHINEPASSWORD);
    while (!machineSend.connect()) Serial.print("C");


    // Example Getting data:
    // Print some machine data stored in MA

    machineSend.setMASessionCookie(COOKIEKEY);
    
    // Display the last 5 minutes of variable humitity. Use makeTS method to set a concrete date/time

    unsigned long tsIni = timeClient.getEpochTime()-60*5;
    unsigned long tsEnd = timeClient.getEpochTime();

    machineSend.downloadCsv("ESP32", "humidity", tsIni, tsEnd);
    machineSend.printCsv();


    // LOOP of the task

    while (true) {
        machineSend.update(isWifiOK());
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


static void InitWifi() {
    Serial.println(" > WiFi");
    Serial.println("Connecting...");

    WiFi.begin(ssid, password);
    int retryNum=0;

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (retryNum==10) {
            WiFi.begin(ssid, password);
            retryNum=0;
        }
        retryNum++;
    }

    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}


static bool isWifiOK(){
    return (WiFi.status() == WL_CONNECTED);
}

