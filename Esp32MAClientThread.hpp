#ifndef ESP32MACLIENTTHREAD_HPP
#define ESP32MACLIENTTHREAD_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h> // Needed for the MA APIs
#include <AzureIotHub.h> // Needed to send to MQTT to MA
//#include <ArduinoJson.h> // Needed to manage JSON
//TODO: convert API responses to JSON

// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>
#define ESP32MALOG_SD true

#include "Esp32MQTTClient.h"

// Library defines

#define MAXNUMVARS 32 // Max num of variables to log
#define MAXBUFFER 64 // Max size of the ram buffer
#define MILLISSENDPERIOD 1000 // Minimum period between messages to Machine Advisor
#define COMRECOVERYDELAY 1000 // Timeout after recovering Wifi/communications
#define MAXCHARVARNAME 15 // Maximum chars of the var name

#define ENDPOINTAPI "https://api.machine-advisor.schneider-electric.com/download/{{clientidnum}}/%5B%22{{device}}%3A{{varname}}%22%5D/{{tsini}}/{{tsend}}"

// SD Defines

#define FILENAMESD "/sdbuffer.csv"
#define SD_GPIO 5

// Type: List of registered variables

typedef struct varRegister_t {
    String name;
    int* ptrValue;  // ptr to variabl
    int minPeriod;  // updating minimum period
    int threshold;  // threshold in case of updating by change (optional)
    int maxPeriod;  // max time without updating (optional)

    int _lastValue; // last value sent
    unsigned long _lastUpdateTime; // millis when las value was sent

} varRegister_t;

typedef struct varRegisterList_t {
    varRegister_t var[MAXNUMVARS];
    int num = 0;    // num of registered variables
} varRegisterList_t;


// Type: Variable with time stamp. To push to the buffer

typedef struct varStamp_t {
    char varName[MAXCHARVARNAME];
    uint8_t  varId;
    int	value;
    unsigned long ts;
} varStamp_t;


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

// TODO: Build a class to manage SD Buffer.
// It is a FIFO buffer written in a file. 
// Pop is done positioning a pointer with seek, and reading 
// Push is done appending in the file.
// When de file gets too big, the older part is removed to make space.


class SDBuffer {

    public:

        SDBuffer();
        bool setFileName(String fileName);
        bool fileExist();
        bool createFile(String fileName);
        int bufferSize();

        bool empty();

        bool peek(varStamp_t* varStamp); // Use file.seek()
        bool pop(varStamp_t* varStamp, bool onlyPeek=false); // Use file.seek()
        bool push(varStamp_t* varStamp); // Use file.append() if there is space in disk. If not delete X first values and retry.
        void deleteFile(); // Delete file if seekPointer is in the end (no more data to pop);
        bool init();

    private:

        bool _SDinit=false;
        String _fileName;

        int _bufferSize=0; // Real buffer size (in objects)

        int _totalFileSize=0; // Total size of the file.
        size_t _currentPointer=0; // Position where to start reading new data

        bool _isFileCreated;

        //bool _deleteOldestValuesFromFile (int numValues);

        bool _writeAppendFile(fs::FS &fs, const char * path, const char * message, const char * option);

        uint64_t _size();

};




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

        void resetError();
        int getNumErrors();
        String getBufferInfo();

        // Internal methods

        QueueHandle_t* _getPtrBuffer();
        unsigned long* _getTsPtr();


    private:

        // Constructor

        bool _enableSDLog;

        unsigned long _nowMillis;
        bool _coldStart=true;

        // Structure to register variables

        varRegisterList_t _varList;

        // Registering vars private methods

        bool _registerVarAtPosition(int pos, String name, int *ptrValue, int minPeriod, int threshold=0, int maxPeriod=-1);
        int _findVarIndex(int *ptrVar);
        bool _shouldVarBeUpdated(int varId);

        unsigned long _lastMinPeriodsMillis=0;

        // Error management

        void _setError(String errorText = "> Error with no text");
        String _lastErrorText="";
        unsigned long _lastErrorMillis=0;
        unsigned long _lastTs;
        bool _globalError=false;
        long int _numErrors=0;

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



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


class Esp32MAClientSend {

    public:

        // Constructor

        Esp32MAClientSend(String assetName, Esp32MAClientLog &logClient); // Easy
        Esp32MAClientSend(String assetName, QueueHandle_t* ptrxBufferCom, unsigned long* ptrTs);

        // Conexion methods

        void setMABrokerUrl(String rawBroker);
        void setMAClientId(String rawClientId);
        void setMAPassword(String rawPassword);

        void setConnexionString(String rawConnexionStr);
        void setConnexionString(String rawBroker, String rawClientId, String rawPassword);

        void setMASessionCookie(String sessionCookie);

        bool connect();

        // Updating method. To be called as fast as posible
        // Optionally a connection status can be provided

        void update(bool isComOK = true);

        // Sending messages manually 

        bool sendMQTTMessage(String message, bool isComOK = true);
        bool sendMQTTMessage(String name, int value, unsigned long ts, bool isComOK = true);

        // API management to download data

        void downloadCsv(const String device, const String var, unsigned long tsIni, unsigned long tsEnd);
        void printCsv();

        String getCsv();
        String getMachineCode();

        // Error management

        void resetError();
        int getNumErrors();
        String getBufferInfo();

        // Auxiliar static methods

        static unsigned long makeTS(int year, byte month, byte day, byte hour, byte min, byte seg);

    private:

        String _assetName; // Asset name (constructor)

        unsigned long _nowMillis;

        String _brokerUrl=""; // Broker URL string from MA
        String _password=""; // Password string from MA
        String _clientId=""; // Client ID string from MA
        String _connexionString=""; // Conexion string
        String _sessionCookie=""; // Session Cookie (from chrome)

        int _messageOKCount = 0; // Messages sent OK
        int _messageErrorCount = 0; // Message with problems


        // Body of MA message

        const String _iniMessage = "{\"metrics\": {\"assetName\": \"{{assetname}}\"";
        const String _endMessage = "}}";
        const String _varMessage = ",\"{{varname}}\": {{varvalue}},\"{{varname}}_timestamp\": {{time}}";

        // Body of API end point

        const String _endPointApi = ENDPOINTAPI;

        // Auxiliar methods

        void _buildConnexionString();
        static void _SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result);

        String _createMQTTMessage();
        String _createMQTTMessageVar(String name, int value, unsigned long ts);

        // Error management

        void _setError(String errorText = "> Error with no text");
        String _lastErrorText="";
        unsigned long _lastErrorMillis=0;
        unsigned long _lastTs;
        bool _globalError=false;
        long int _numErrors=0;
        bool _isComOK;

        bool _lastIsWifiOK; // To manage the re-sending delay
        bool _lastIsComFullOK; 
        unsigned long _recoveringComMillis=0;

        // API management

        void _getFromApi(const String endPointRequest, const String XAuth="", const String cookiesValue="");
        String _receivedPayload;

        //Freertos buffer managment
        
        QueueHandle_t* _ptrxBufferCom;
        bool _sendBufferedMessages();
        unsigned long _lastBufferMillis=0;

        unsigned long* _ptrTs;

             


};







#endif