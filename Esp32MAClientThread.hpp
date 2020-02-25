#ifndef ESP32MACLIENTTHREAD_HPP
#define ESP32MACLIENTTHREAD_HPP

#include <Arduino.h>
#include <WiFi.h>
#include "AzureIotHub.h"
#include "Esp32MQTTClient.h"
#include <HTTPClient.h> // Needed for the MA APIs


#define MAXNUMVARS 32 // Max num of variables to log
#define MAXBUFFER 64 // Max size of the buffer
#define MILLISSENDPERIOD 1000 // Minimum period between messages to Machine Advisor
#define COMRECOVERYDELAY 0 // Timeout after recovering Wifi/communications
#define MAXCHARVARNAME 15 // Maximum chars of the var name

#define ENDPOINTAPI "https://api.machine-advisor.schneider-electric.com/download/{{clientidnum}}/%5B%22{{device}}%3A{{varname}}%22%5D/{{tsini}}/{{tsend}}"


typedef struct varStamp_t {
char varName[MAXCHARVARNAME];
uint8_t  varId;
int	value;
unsigned long ts;
} varStamp_t;



class Esp32MAClientLog {

    private:

        unsigned long _nowMillis;

        bool _coldStart=true;


        // Structure to register variables

        String _varName[MAXNUMVARS]; // names
        int *_ptrVarValue[MAXNUMVARS]; // ptr to variable
        int _varPeriod[MAXNUMVARS]; // updating minimum period
        int _varThreshold[MAXNUMVARS]; // threshold in case of updating by change (optional)
        int _varMaxPeriod[MAXNUMVARS]; // max time without updating (optional)

        int _varLastValue[MAXNUMVARS]; // last value sent
        unsigned long _varLastUpdateTime[MAXNUMVARS]; // millis when las value was sent

        int _numRegisteredVars = 0; // num of registered variables
        int _minPeriodAllVars = 100000; // minimum update period

        // Registering vars private methods

        bool _registerVarAtPosition(int pos, String name, int *ptrValue, int minPeriod, int threshold=0, int maxPeriod=-1);
        int _findVarIndex(int *ptrVar);
        bool _shouldVarBeUpdated(int varId);

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

        unsigned long _lastBufferErrorMillis=0;


    public:

        Esp32MAClientLog();
        QueueHandle_t* buffer();
        String getBufferInfo();

        void update(unsigned long ts);

        // Variables registering methods

        int registerVar(String name, int *ptrValue, int minPeriod, int threshold=0, int maxPeriod=-1); //-1 means no maximum
        bool modifyRegisteredVar(String name, int *ptrValue, int minPeriod, int threshold=0, int maxPeriod=-1);

        // Error management

        void resetError();
        int getNumErrors();

        // Return the time stamp pointer

        unsigned long* getTsPtr();
     
};



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////



class Esp32MAClientSend {

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
        unsigned long _recoveringComMillis=0;

        // API management

        void _getFromApi(const String endPointRequest, const String XAuth="", const String cookiesValue="");
        String _receivedPayload;

        // New Buffer management (using Queue library)
        
        //Queue* _ptrBufferCom;
        QueueHandle_t* _ptrxBufferCom;
        bool _sendBufferedMessages();
        unsigned long _lastBufferMillis=0;

        unsigned long* _ptrTs;

    public:

        Esp32MAClientSend(String assetName, Esp32MAClientLog &logClient);
        Esp32MAClientSend(String assetName, QueueHandle_t* ptrxBufferCom, unsigned long* ptrTs);

        // Error management

        void resetError();
        int getNumErrors();
        String getBufferInfo();

        // Conexion methods

        void setMABrokerUrl(String rawBroker);
        void setMAClientId(String rawClientId);
        void setMAPassword(String rawPassword);

        void setConnexionString(String rawConnexionStr);
        void setConnexionString(String rawBroker, String rawClientId, String rawPassword);

        void setMASessionCookie(String sessionCookie);

        bool connect();


        // Updating method. To be called as fast as posible
        // It needs the time stamp to log the variable
        // Optionally a connection status can be provided

        void update(bool isComOK = true);

        // Sending messages manually 

        bool sendMQTTMessage(String message, bool isComOK = true);
        bool sendMQTTMessage(String name, int value, unsigned long ts, bool isComOK = true);

        // API management

        void downloadCsv(const String device, const String var, unsigned long tsIni, unsigned long tsEnd);
        void printCsv();

        String getCsv();
        String getMachineCode();

        // Auxiliar static methods

        static unsigned long makeTS(int year, byte month, byte day, byte hour, byte min, byte seg);
};


#endif