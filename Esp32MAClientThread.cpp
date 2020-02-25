
// Reference: https://techtutorialsx.com/2017/09/13/esp32-arduino-communication-between-tasks-using-freertos-queues/

#include <Arduino.h>
#include "Esp32MAClientThread.hpp"
#include "secrets/secrets.h"


// Constructor

Esp32MAClientLog::Esp32MAClientLog(){

    _xBufferCom = xQueueCreate( MAXBUFFER, sizeof(varStamp_t));

    if(_xBufferCom == NULL){
    _setError("Problem creating buffer to interact between tasks. Check memory allocation.");
  }

}


// Registering variables to be sent to Machine Advisor IOT Hub
// minPeriod = period in milliseconds
// thershold (optinal) = if filled, the variable will be sent if the change>threshold and minPeriod has ocurred
// maxPeriod (optional) = if filled, maximum period without sending the variable

int Esp32MAClientLog::registerVar(String name, int *ptrValue, int minPeriod, int threshold, int maxPeriod){

    int varId=-1;
    bool allOk=false;

    if (_numRegisteredVars<MAXNUMVARS) {

        allOk = _registerVarAtPosition(_numRegisteredVars, name, ptrValue, minPeriod, threshold, maxPeriod);

        if (allOk) {
            varId = _numRegisteredVars;
            _numRegisteredVars ++;
        }

    } else {

        varId = -1;
        _setError("No more space for new variables. Check the constructor");
    }

    return (varId);
}


// Registering method in a vector position.

bool Esp32MAClientLog::_registerVarAtPosition(int varID, String name, int *ptrValue, int minPeriod, int threshold, int maxPeriod){

    // TODO: Verify that values are correct

    // Only can be updated a position already written, or the next one.
    if (varID <= _numRegisteredVars) {
        _varName[varID] = name;
        _ptrVarValue[varID] = ptrValue;
        _varPeriod[varID] = minPeriod;
        _varThreshold[varID] = threshold;
        _varMaxPeriod[varID] = maxPeriod;

        _varLastUpdateTime[varID] = 0;
        _varLastValue[varID] = 0;

        _minPeriodAllVars = min(_minPeriodAllVars, minPeriod);
        return(true);

    } else {
        _setError("No more space for new variables. Check the constructor");
        return(false);
    }  
}


// Modify an already registered variable

bool Esp32MAClientLog::modifyRegisteredVar(String name, int *ptrValue, int minPeriod, int threshold, int maxPeriod){

    int varID = _findVarIndex(ptrValue);

    if (varID>=0) return(_registerVarAtPosition(varID, name, ptrValue, minPeriod, threshold, maxPeriod));
    else {
        return(false);  
    }
}

// Find the index where a variable is registered

int Esp32MAClientLog::_findVarIndex (int *ptrValue){

    for (int i = 0; i < MAXNUMVARS; i++) {
        
        if (_ptrVarValue[i] == ptrValue){
            return(i);
        }
    }
    _setError("Variable not found");
    return (-1);
}


// Update: To be called as fast as posible
// Check if a variable should be updated. If so, push it to the communicaitons buffer

void Esp32MAClientLog::update(unsigned long ts){

    _lastTs = ts;
    _nowMillis = millis();
    
    int numUpd=0;

    for (int varId=0; varId<_numRegisteredVars; varId++){

        if(_shouldVarBeUpdated(varId)) _pushVarToBuffer(varId, ts);
        numUpd++;

    }

    if (_coldStart) _coldStart = false;
}



/// Calculate if the variable should be updated or not

bool Esp32MAClientLog::_shouldVarBeUpdated(int varId){

    unsigned long elapsedTimeVar = _nowMillis - _varLastUpdateTime[varId];

    bool varToBeUpdated =  ( (elapsedTimeVar >= _varPeriod[varId]) && 
                        (abs(*(_ptrVarValue[varId]) - _varLastValue[varId]) > _varThreshold[varId]) ) ||
                        (elapsedTimeVar > _varMaxPeriod[varId] && _varMaxPeriod[varId] != -1) ||
                        _coldStart;
                        
    return(varToBeUpdated);
}


// Push variable to the communication buffer

bool Esp32MAClientLog::_pushVarToBuffer(int varId, unsigned long ts) {

    varStamp_t varStamp;
    bool allOK=false;

    strcpy(varStamp.varName, _varName[varId].substring(0,MAXCHARVARNAME).c_str());
    varStamp.varId = varId;
    varStamp.value = *(_ptrVarValue[varId]);
    varStamp.ts = ts;


    // Send structure to buffer, and do NOT block if the buffer is full to avoid stopping the application.

    allOK = (xQueueSend(_xBufferCom, &varStamp, 0) == pdPASS);
    
    if (allOK) {

        _varLastUpdateTime[varId] = _nowMillis;
        _varLastValue[varId] = varStamp.value;

    } else {

        if ((millis()-_lastBufferErrorMillis) >= MILLISSENDPERIOD) {
            _setError("Problem pushing a var to the buffer. Buffer=" + getBufferInfo());
            _lastBufferErrorMillis = millis(); // Do not log an error immediatelly
        }
        
    } 

    return(allOK);
}

// Return buffer pointer

QueueHandle_t* Esp32MAClientLog::buffer(){
    return(&_xBufferCom);
}


// Return buffer Information

String Esp32MAClientLog::getBufferInfo(){

    UBaseType_t buffMsgWaiting = uxQueueMessagesWaiting(_xBufferCom);
    UBaseType_t buffSpaceAvailable = uxQueueSpacesAvailable(_xBufferCom);
    int msgTotal = buffMsgWaiting + buffSpaceAvailable;
    
    String status = "[" + String(buffMsgWaiting) + "/" + String(msgTotal) + "]";

    return(status);
}


// Error logging methods

void Esp32MAClientLog::_setError(String textError){
    _globalError = true;
    _lastErrorText = textError;
    _lastErrorMillis = _lastTs;
    _numErrors = (_numErrors+1) % LONG_MAX;
    Serial.println("Error: >>>" + textError + " TS=" + String(_lastTs) + " Total Errors=" + String(getNumErrors()));
}


void Esp32MAClientLog::resetError(){
    _globalError = false;
    _lastErrorText = "";
    _lastErrorMillis = 0;
    _numErrors=0;
}

int Esp32MAClientLog::getNumErrors(){
    return (_numErrors);
}

// Get the current time stamp pointer

unsigned long* Esp32MAClientLog::getTsPtr() {

    return(&_lastTs);

}



///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////


// Constructor (Dettailed)

Esp32MAClientSend::Esp32MAClientSend(String assetName, QueueHandle_t* ptrxBufferCom, unsigned long* ptrTs) {

    _assetName = assetName;
    _ptrxBufferCom = ptrxBufferCom;
    _ptrTs = ptrTs;
}

// Easy constructor (Based directly on Esp32MAClientLog object)

Esp32MAClientSend::Esp32MAClientSend(String assetName, Esp32MAClientLog &logClient) {

    _assetName = assetName;
    _ptrxBufferCom = logClient.buffer();
    _ptrTs = logClient.getTsPtr();
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

// Set cookie method
// The cookie can be obtained from a registerd Web browser.

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

    Serial.println("");
    Serial.println("Info: >>>Starting connexion to Machine Advisor");

    allOK = Esp32MQTTClient_Init((const uint8_t*)_connexionString.c_str());
    Esp32MQTTClient_SetSendConfirmationCallback(_SendConfirmationCallback);

    if (!allOK) _setError("Problem connecting to Machine Advisor. Check connection credentials");
    return(allOK);

}


// Update: To be called as fast as posible
// Update method upload the bufered messages to MA

void Esp32MAClientSend::update(bool isComOK){

    _lastTs = *_ptrTs;
    _nowMillis = millis();
    _isComOK = isComOK;
    
    _sendBufferedMessages();

}

// Send the buffered vars at constant pace to avoid comunication problems

bool Esp32MAClientSend::_sendBufferedMessages(){

    bool allOK=true;
    
    varStamp_t varStamp;

    // Peek the value of the buffer (but do not remove it). BLOCK the task if buffer is empty
    if (xQueuePeek(*_ptrxBufferCom, &varStamp, portMAX_DELAY) != pdPASS) {
        _setError("Problem getting a value from the buffer");
        allOK=false;
    }


    if (allOK) {

        String mqttMessage = _createMQTTMessageVar(varStamp.varName, varStamp.value, varStamp.ts);

        if (sendMQTTMessage(mqttMessage, _isComOK)) {

            // Remove the message from buffer, as the message is sent correctly to MA
            xQueueReceive(*_ptrxBufferCom, &varStamp, 0);
            _messageOKCount = (_messageOKCount + 1) % INTMAX_MAX;

            if (uxQueueMessagesWaiting(*_ptrxBufferCom) !=0) {
                Serial.println("Last message was buffered=" + getBufferInfo());
            }

        } else {
            _setError("Problem sending the message to Machine Advisor. Check connection status. Buffer=" + getBufferInfo());
            _messageErrorCount = (_messageErrorCount +1) % INTMAX_MAX;
            allOK = false;
        }
    }

    _lastBufferMillis = _nowMillis;

    // The task is delayed to avoid communication problems with IOT Hub.
    // TODO: Check what is the minimum period.

    vTaskDelay(portTICK_PERIOD_MS * MILLISSENDPERIOD);
    
    return(allOK);

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

    bool allOK = false;
    bool sendMessage = false;

    // Wait for a while when the connection is recovered

    if (isComOK && !_lastIsWifiOK) _recoveringComMillis = millis();
    else if (isComOK && (millis()-_recoveringComMillis)>= COMRECOVERYDELAY) {
        sendMessage=true;
    }
    else sendMessage=false;


    if (sendMessage) {

        EVENT_INSTANCE* message = Esp32MQTTClient_Event_Generate(mqttMessage.c_str(), MESSAGE);

        if(Esp32MQTTClient_SendEventInstance(message)) {

            Serial.println("Message sent =" + mqttMessage);
            allOK = true;

        } else allOK = false;

    } else allOK = false;

    _lastIsWifiOK = isComOK;

    return(allOK);
}


/// Error management

void Esp32MAClientSend::_setError(String textError){
    _globalError = true;
    _lastErrorText = textError;
    _lastErrorMillis = _lastTs;
    _numErrors = (_numErrors+1) % LONG_MAX;
    Serial.println("Error: >>>" + textError + " TS=" + String(_lastTs) + " Total Errors=" + String(getNumErrors()));
}

void Esp32MAClientSend::resetError(){
    _globalError = false;
    _lastErrorText = "";
    _lastErrorMillis = 0;
    _numErrors=0;
}

int Esp32MAClientSend::getNumErrors(){
    return (_numErrors);
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

    Serial.println("Info: >>>Getting");

    int httpCode = http.GET();  

    if (httpCode >= 200 && httpCode<=299) { 

        Serial.println(http.getSize());
        _receivedPayload = http.getString(); 
        // TODO: move to ---> http.getStream or pointer

    } else {
        _setError("HTTP request NOT successful. Code = " + String(httpCode));
        _receivedPayload="";
    }

    //TODO: Manage answer --> http.collectHeaders();

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

            Serial.print(lineStr.substring(0,coma1));
            Serial.print(" | " + lineStr.substring(coma1+1,coma2));
            Serial.println(" | " + lineStr.substring(coma2+1,size));

            iniSub = indexEndCol+1;
            indexEndCol = _receivedPayload.indexOf("\n", iniSub);

            if (iniSub == -1 || indexEndCol == -1) break;
        }
    } else _setError("No payload to parse and print. Check if it has been received from Machine Advisor");

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



