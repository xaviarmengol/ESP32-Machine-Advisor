#ifndef ESP32MALOG_HPP
#define ESP32MALOG_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h> // Needed for the MA APIs
#include <AzureIotHub.h> // Needed to send to MQTT to MA
//#include <ArduinoJson.h> // Needed to manage JSON
//TODO: convert API responses to JSON

#include "dataStructure.h"
#include "SDBuffer.hpp"
#include "Esp32MQTTClient.h"
#include "DebugMgr.hpp"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


class Esp32MAClientLog {

    public:

        // Constructor

        Esp32MAClientLog (bool enableSDLog=false);

        // Register variables

        int registerVar(String name, int *ptrValue, int minPeriod, int threshold=0, int maxPeriod=-1); //-1 means no maximum
        bool modifyRegisteredVar(String name, int *ptrValue, int minPeriod, int threshold=0, int maxPeriod=-1);

        // Update Method

        void update(unsigned long ts);

        // Error management

//        void resetError();
//        int getNumErrors();

        String getBufferInfo();

        // Internal methods

        QueueHandle_t* _getPtrBuffer();
        unsigned long* _getTsPtr();


    private:

        // Constructor

        bool _enableSDLog;

        unsigned long _nowMillis;
        bool _coldStart=true;

        bool _logInitialized=false; // If log has been initializedvvvv

        // Structure to register variables

        varRegisterList_t _varList;

        // Registering vars private methods

        bool _registerVarAtPosition(int pos, String name, int *ptrValue, int minPeriod, int threshold=0, int maxPeriod=-1);
        int _findVarIndex(int *ptrVar);
        bool _shouldVarBeUpdated(int varId);

        unsigned long _lastMinPeriodsMillis=0;

        // Error management

        DebugMgr _debug;

//        void _setError(String errorText = "> Error with no text");
//        void _setMsg(String msgText = "> Msg with no text");

//        String _lastErrorText="";
//        unsigned long _lastErrorMillis=0;

        
//        bool _globalError=false;
//        long int _numErrors=0;

        unsigned long _lastTs;

        // Buffer management

        QueueHandle_t _xBufferCom; // Intertask communication buffer. Initialitzacion in the constructor
        bool _pushVarToBuffer(int varId, unsigned long ts);
        bool _pushVarToBufferHardware(varStamp_t* ptrVarStamp);
        void _fillVarFromIdTs(varStamp_t *ptrVar, int varId, unsigned long ts);


        int _varsNotBufferedAndLost = 0;

        unsigned long _lastBufferErrorMillis=0;

        // SD Buffer management

        SDBuffer _sdBufferCom;

        void _updateSDBuffer();
 
};



#endif