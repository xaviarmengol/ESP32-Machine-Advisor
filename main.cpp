#include <Arduino.h>

#include <WiFi.h>
#include <NTPClient.h>

#include "Esp32MAClientThread.hpp"
#include "secrets/secrets.h"

bool iniNTP();
static void InitWifi();
static bool isWifiOK();

// Tasks definition

TaskHandle_t handTskUploading;
void tskUploading (void *pvParameters);

// Please input the SSID and password of WiFi

const char* ssid     = SSID;
const char* password = SSIDPASS;
static bool hasWifi = false;

// NTP

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Esp32MAClientLog : Log the application variables and send them to a buffer

Esp32MAClientLog dataLogger;

// Esp32MAClienteSend : Read the buffer and send it to Machine Advisor 

Esp32MAClientSend machine("ESP32", dataLogger);

// Aplication variables example

int temp=20;
int humid=1000;
int volt=5;
unsigned long lastUpdate=0;


//////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

    Serial.begin(115200);
    Serial.println("Initializing...");

    // Wifi

    Serial.println(" > WiFi");
    hasWifi = false;
    InitWifi();
    if (!hasWifi) {
        return;
    }

    // NTP

    timeClient.begin();
    timeClient.setTimeOffset(3600);
    timeClient.setUpdateInterval(1000*60*100); // update time every 100 minutes
    iniNTP();

    // Machne Advisor connection credetials for "Machine"

    machine.setMABrokerUrl(MACHINEBROKERURL);
    machine.setMAClientId(MACHINECLIENTID);
    machine.setMAPassword(MACHINEPASSWORD);

    while (!machine.connect()) {
        Serial.print("C");
    }

    // Sending task is declared in a independent task declaration pinned to a dedicated core (0)
    // Standard loop task is runnign in core 1.

    xTaskCreateUniversal(tskUploading, "TaskUpload", 10000, NULL, 1, &handTskUploading, 0);
    delay(500); 


    // Example 1:
    // Register a variable with a sampling time of 20s

    dataLogger.registerVar("humidity", &humid, 20000);

    // Example 2:
    // Register a variable with a minimum sampling time of 5s,
    // but sample it only if the variable change is more than 5 units

    dataLogger.registerVar("temperature", &temp, 5000, 5);

    // Example 3:
    // Register a variable with a minimum sample time of 5s
    // but sample it only if the variable change is more than 2 units
    // If the variable hasn't changed enough for 30s, sample it anyway

    dataLogger.registerVar("voltage", &volt, 5000, 20, 30000);

    // Example 4:
    // Print some machine data stored in MA

    machine.setMASessionCookie(COOKIEKEY);
    
    // Example to calculate some dates
    //unsigned long tsIni = machine.makeTS(2020, 2, 15, 17, 50, 16);
    //unsigned long tsEnd = machine.makeTS(2020, 2, 15, 17, 53, 36);

    // Display the last 5 minutes of variable humitity

    unsigned long tsIni = timeClient.getEpochTime()-60*5;
    unsigned long tsEnd = timeClient.getEpochTime();

    machine.downloadCsv("ESP32", "humidity", tsIni, tsEnd);
    machine.printCsv();


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

    dataLogger.update(timeClient.getEpochTime());
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////


void tskUploading (void *pvParameters){

    Serial.println("Task: Uploading to MA running on core " + String(xPortGetCoreID()));

    while (true) machine.update(isWifiOK());

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////


bool iniNTP(){
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
    Serial.println("Connecting...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    hasWifi = true;
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

static bool isWifiOK(){
    return (WiFi.status() == WL_CONNECTED);
}

