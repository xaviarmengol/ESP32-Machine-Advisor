#include "Esp32MAClient.hpp"

// Constructor (Detailed)

Esp32MAClientSend::Esp32MAClientSend(String assetName, QueueHandle_t* ptrxBufferCom, unsigned long* ptrTs) {

    _assetName = assetName;
    _ptrxBufferCom = ptrxBufferCom;
    _ptrTs = ptrTs;
    debug.setLibName("Client");
}


// Easy constructor (Based directly on Esp32MAClientLog object)

Esp32MAClientSend::Esp32MAClientSend(String assetName, Esp32MAClientLog &logClient) {

    //Esp32MAClientSend(assetName, logClient._getPtrBuffer(), logClient._getTsPtr());
    
    _assetName = assetName;
    _ptrxBufferCom = logClient._getPtrBuffer();
    _ptrTs = logClient._getTsPtr();
    debug.setLibName("Client");
    
}



// Connexion Strigs methods

void Esp32MAClientSend::setMABrokerUrl(String rawBroker) {

    // Removing de protocol and port
    _brokerUrl = rawBroker.substring(8,rawBroker.lastIndexOf(":"));
    _buildConnexionString();

}

void Esp32MAClientSend::setMAClientId(String rawClientId){

    _clientId = rawClientId;
    _buildConnexionString();

}

void Esp32MAClientSend::setMAPassword(String rawPassword) {

    _password = rawPassword;
    _buildConnexionString();

}

void Esp32MAClientSend::_buildConnexionString(){
    _connexionString = "HostName=" + _brokerUrl + ";" + "DeviceId=" + _clientId + ";" + "SharedAccessSignature=" + _password;
}


void Esp32MAClientSend::setConnexionString(String rawConnexionStr){
    _connexionString = rawConnexionStr;
}

void Esp32MAClientSend::setConnexionString(String rawBroker, String rawClientId, String rawPassword){
    setMABrokerUrl(rawBroker);
    setMAClientId(rawClientId);
    setMAPassword(rawPassword);
    _buildConnexionString();
}

// Set cookie. The cookie can be obtained from a registered Web browser.

void Esp32MAClientSend::setMASessionCookie(String sessionCookie) {

    _sessionCookie = sessionCookie;

}


// Callback functions (optional)

void Esp32MAClientSend::_SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result) {

    if (result == IOTHUB_CLIENT_CONFIRMATION_OK) {
        //Serial.println("Send Confirmation Callback finished.");
    } else {
        Serial.println("Error: IOT Hub answered an error when message sent: " + String(result));
    }

}


// Connection method. To be called in the setup phase.

bool Esp32MAClientSend::connect() {

    bool allOK;

    debug.setMsg("Starting connexion to Machine Advisor");

    allOK = Esp32MQTTClient_Init((const uint8_t*)_connexionString.c_str());
    Esp32MQTTClient_SetSendConfirmationCallback(_SendConfirmationCallback);

    if (!allOK) debug.setError("Problem connecting to Machine Advisor. Check connection credentials");
    return(allOK);

}


// Update method upload the bufered messages to MA
// To be called as fast as posible

void Esp32MAClientSend::update(bool isComOK){

    _lastTs = *_ptrTs;
    _nowMillis = millis();
    _isComOK = isComOK;
    
    _sendBufferedMessages();

}

// Send the buffered to Machine Advisor

bool Esp32MAClientSend::_sendBufferedMessages(){

    // vTaskDelay is not used to be able to use the library in a mono-task system

    bool bufferWithValue;
    bool sendOK=true;
    
    varStamp_t varStamp;

    // TODO: Check what is the minimum posible period to update messages to Machine Advisor

    if ((_nowMillis - _lastBufferMillis) >= MILLISSENDPERIOD) {

        // Peek the value of the buffer (but do not remove it). 
        // Do NOT block the task to be able to use the library in a mono-task system

        bufferWithValue = (xQueuePeek(*_ptrxBufferCom, &varStamp, 0) == pdPASS);            

        if (bufferWithValue) {

            String mqttMessage = _createMQTTMessageVar(varStamp.varName, varStamp.value, varStamp.ts);

            // TODO: Manage to send multiples updates in the same message.

            sendOK = sendMQTTMessage(mqttMessage, _isComOK);

            if (sendOK) {

                // If send is OK, remove the message from the buffer
                xQueueReceive(*_ptrxBufferCom, &varStamp, 0);

                _messageOKCount = (_messageOKCount + 1) % INTMAX_MAX;

                if (bufferWithValue && uxQueueMessagesWaiting(*_ptrxBufferCom) !=0) {
                    debug.setMsg("Last message was buffered=" + getBufferInfo(), _lastTs);
                }

            } else {
                debug.setError("Sending the message to MA. Check connection status. Buffer=" + getBufferInfo(), _lastTs);
                _messageErrorCount = (_messageErrorCount +1) % INTMAX_MAX;
            }
        }

        _lastBufferMillis = _nowMillis;
    }

    // To avoid Watch dog problems, if the task is running in core 0 delay it 1ms
    if (xPortGetCoreID() == 0) vTaskDelay(1);

    return(sendOK);
}


// Create the message for a single variable

String Esp32MAClientSend::_createMQTTMessageVar(String name, int value, unsigned long ts){

    String returnMessage;
    String varTemp;
    String iniTemp;

    iniTemp = _iniMessage;
    iniTemp.replace("{{assetname}}", _assetName);
    returnMessage = iniTemp;

    varTemp = _varMessage;
    varTemp.replace("{{varname}}", name);
    varTemp.replace("{{varvalue}}", String(value));
    varTemp.replace("{{time}}", String(ts)+String("000"));

    returnMessage += varTemp;

    returnMessage +=_endMessage;
    return(returnMessage);
}


// Create and Send a simple MQTTMessage to Machine Advisor

bool Esp32MAClientSend::sendMQTTMessage(String name, int value, unsigned long ts, bool isComOK){

    return(sendMQTTMessage(_createMQTTMessageVar(name, value, ts), isComOK));

}

// Send a RAW MQTTMessage to Machine Advisor.

bool Esp32MAClientSend::sendMQTTMessage(String mqttMessage, bool isComOK) {

    bool isComFullOK = false;
    bool isMessageSent = false;

    // If the connection is recovered, do not resend immediatelly

    if (isComOK && !_lastIsWifiOK) _recoveringComMillis = millis();

    else if (isComOK && (millis()-_recoveringComMillis)>= COMRECOVERYDELAY) {
        isComFullOK=true;
    }
    else isComFullOK=false;

    // Reset when some delay happened until re-conection

    if (isComFullOK && !_lastIsComFullOK) Esp32MQTTClient_Reset();

    // If we can send the message

    if (isComFullOK) {

        EVENT_INSTANCE* message = Esp32MQTTClient_Event_Generate(mqttMessage.c_str(), MESSAGE);

        isMessageSent = Esp32MQTTClient_SendEventInstance(message);


        if (isMessageSent) debug.setMsg("Message sent =" + mqttMessage);

    } 

    _lastIsWifiOK = isComOK;
    _lastIsComFullOK = isComFullOK;

    return(isMessageSent);
}



// Managing the APIs calls to retreive information

// Get machine code

String Esp32MAClientSend::getMachineCode(){
    return(_clientId.substring(_clientId.lastIndexOf("-")+1, _clientId.length()));
}

// Get CSV from machine

void Esp32MAClientSend::downloadCsv(const String device, const String var, unsigned long tsIni, unsigned long tsEnd) {

    String endPoint = _endPointApi;

    String clientIdNum = getMachineCode();

    endPoint.replace("{{clientidnum}}", clientIdNum);
    endPoint.replace("{{device}}", device);
    endPoint.replace("{{varname}}", var);
    endPoint.replace("{{tsini}}", String(tsIni));
    endPoint.replace("{{tsend}}", String(tsEnd));

    _getFromApi(endPoint, "", _sessionCookie);

}


// GET request to an end point. Use a class variable to store the payload (to avoid memory duplication)
// TODO: Optimize memory use

void Esp32MAClientSend::_getFromApi(const String endPointRequest, const String XAuth, const String cookiesValue){

    HTTPClient http;
    String payload;
     
    http.begin(endPointRequest);

    if (XAuth !="") {
        http.addHeader("X-Auth-Token", XAuth);
    }

    if (cookiesValue != ""){
        http.addHeader("Cookie", cookiesValue);
    }

    // Make Request

    debug.setMsg("Getting values from Machine Advisor API", _lastTs);

    int httpCode = http.GET();  

    if (httpCode >= 200 && httpCode<=299) { 

        debug.setMsg(String(http.getSize()), _lastTs);
        _receivedPayload = http.getString(); 
        // TODO: move to ---> http.getStream or pointer

    } else {
        debug.setError("HTTP request NOT successful. Code = " + String(httpCode), _lastTs);
        _receivedPayload="";
    }

    http.end(); // Free up connection
}


// Parsing CSV and Print

void Esp32MAClientSend::printCsv() {

    if (_receivedPayload != "") {

        int indexEndCol = _receivedPayload.indexOf("\n");
        int iniSub=0;
        
        while (1){
            String lineStr = _receivedPayload.substring(iniSub, indexEndCol-1);

            int coma1 = lineStr.indexOf(",");
            int coma2 = lineStr.indexOf(",", coma1+1);
            int size = lineStr.length();

            String msg = lineStr.substring(0,coma1);
            msg += " | " + lineStr.substring(coma1+1,coma2);
            msg += " | " + lineStr.substring(coma2+1,size) + String("\n");
            debug.setMsg(msg, _lastTs);

            iniSub = indexEndCol+1;
            indexEndCol = _receivedPayload.indexOf("\n", iniSub);

            if (iniSub == -1 || indexEndCol == -1) break;
        }
    } else debug.setError("No payload to parse and print. Check if it has been received from Machine Advisor", _lastTs);

}

// Get a raw csv

String Esp32MAClientSend::getCsv(){
    return(_receivedPayload);
}


// Aux Static methods

unsigned long Esp32MAClientSend::makeTS(int year, byte month, byte day, byte hour, byte min, byte seg){

    struct tm t;
    time_t t_of_day;

    t.tm_year = year-1900;  // Year - 1900
    t.tm_mon = month-1;           // Month, where 0 = jan
    t.tm_mday = day;          // Day of the month
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = seg;
    t.tm_isdst = 0;        // Is DST on? 1 = yes, 0 = no, -1 = unknown
    t_of_day = mktime(&t);

    return(t_of_day);
}


// Get the bufferend info

String Esp32MAClientSend::getBufferInfo(){

    UBaseType_t buffMsgWaiting = uxQueueMessagesWaiting(*_ptrxBufferCom);
    UBaseType_t buffSpaceAvailable = uxQueueSpacesAvailable(*_ptrxBufferCom);
    int msgTotal = (int)buffMsgWaiting + (int)buffSpaceAvailable;
    
    String status = "[" + String((int)buffMsgWaiting) + "/" + String(msgTotal) + "]";

    return(status);

}


