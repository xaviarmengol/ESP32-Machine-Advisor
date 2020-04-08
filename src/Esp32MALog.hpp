#ifndef ESP32MALOG_HPP
#define ESP32MALOG_HPP

#include <Arduino.h>
#include "dataStructure.h" // Structure to share information between log and client

#include "SDBuffer.hpp" // Fash memory buffer class (optional use)
#include "DebugMgr.hpp" // Debug class


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

        // Information about RAM buffer

        String getBufferInfo();

        // Internal methods that can be accessed from other classes

        QueueHandle_t* _getPtrBuffer();
        unsigned long* _getTsPtr();

        // Error management

        DebugMgr debug;

    private:

        // Constructor

        bool _enableSDLog;

        unsigned long _nowMillis;
        bool _coldStart=true;

        bool _logInitialized=false; // If log class has been initialized

        // Structure to register variables

        varRegisterList_t _varList;

        // Registering vars private methods

        bool _registerVarAtPosition(int pos, String name, int *ptrValue, int minPeriod, int threshold=0, int maxPeriod=-1);
        int _findVarIndex(int *ptrVar);
        bool _shouldVarBeUpdated(int varId);

        unsigned long _lastMinPeriodsMillis=0;



        unsigned long _lastTs;

        // RAM Buffer management (thread safe)

        QueueHandle_t _xBufferCom; // Intertask communication buffer

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