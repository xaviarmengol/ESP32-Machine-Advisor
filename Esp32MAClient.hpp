#ifndef ESP32MACLIENT_HPP
#define ESP32MACLIENT_HPP

#include "Esp32MALog.hpp"
#include "dataStructure.h"
#include "DebugMgr.hpp"

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

        DebugMgr _debug;

        unsigned long _lastTs;

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