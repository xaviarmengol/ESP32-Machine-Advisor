#include "Esp32MAClient.hpp"

// Constructor

Esp32MAClientLog::Esp32MAClientLog(bool enableSDLog){

    _enableSDLog = enableSDLog;

    // creation of freertos FIFO queue (Thread safe)

    _xBufferCom = xQueueCreate( MAXBUFFER, sizeof(varStamp_t));

    if(_xBufferCom == NULL){
        _setError("Problem creating buffer to interact between tasks. Check memory allocation.");
    }

    // Creation SD Buffer

    if (_enableSDLog) {

        if (!_sdBufferCom.init()) {
            _setError("Problem mounting the SD. Check SD Card.");
        }
        
        if (!_sdBufferCom.setFileName(FILENAMESD)) {
            _setError("Problem intializing SD file for buffer. Check SD card.");
        }

    }

}


// Registering variables to be sent to Machine Advisor IOT Hub

// minPeriod = period in milliseconds
// thershold (optinal) = if filled, the variable will be sent if the change>threshold and minPeriod has ocurred
// maxPeriod (optional) = if filled, maximum period without sending the variable

int Esp32MAClientLog::registerVar(String name, int *ptrValue, int minPeriod, int threshold, int maxPeriod){

    int varId=-1;
    bool allOk=false;

    if (_varList.num < MAXNUMVARS) {

        allOk = _registerVarAtPosition(_varList.num, name, ptrValue, minPeriod, threshold, maxPeriod);

        if (allOk) {
            varId = _varList.num;
            _varList.num ++;
        }

    } else {

        varId = -1;
        _setError("No more space for new variables. Increase the pre-allocated memory.");
    }

    return (varId);
}


// Registering a variable with a varID code

bool Esp32MAClientLog::_registerVarAtPosition(int varID, String name, int *ptrValue, int minPeriod, int threshold, int maxPeriod){

    // TODO: Verify that values are correct

    // Only can be updated a position already written, or the next one.
    if (varID <= _varList.num) {
        _varList.var[varID].name = name;
        _varList.var[varID].ptrValue = ptrValue;
        _varList.var[varID].minPeriod = minPeriod;
        _varList.var[varID].threshold = threshold;
        _varList.var[varID].maxPeriod = maxPeriod;

        _varList.var[varID]._lastUpdateTime = 0;
        _varList.var[varID]._lastValue = 0;

        return(true);

    } else {
        _setError("Only can be updated a variable already registered. Register it first.");
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
        
        if (_varList.var[i].ptrValue == ptrValue){
            return(i);
        }
    }
    _setError("Variable not found");
    return (-1);
}


// Check if a variable should be updated. If so, push it to the communicaitons buffer
// Alse save the last two variables, and update the SD buffer.
// To be called as fast as posible

void Esp32MAClientLog::update(unsigned long ts){

    _lastTs = ts;
    _nowMillis = millis();
    

    for (int varId=0; varId<_varList.num; varId++){

        // Try to move data from SD to memory buffer
        _updateSDBuffer();

        if(_shouldVarBeUpdated(varId)) _pushVarToBuffer(varId, ts);

    }

    if (_coldStart) _coldStart = false;
}


// Update SD Buffer: Move data to normal buffer if it is posible

void Esp32MAClientLog::_updateSDBuffer(){
    
    if (!_sdBufferCom.empty() && _enableSDLog) {

        int spacesAvailableBuffer = uxQueueSpacesAvailable(_xBufferCom);
        int sizeSDBuffer = _sdBufferCom.bufferSize();

        int maxMovements = min(spacesAvailableBuffer, sizeSDBuffer);

        for (int i=0; i<maxMovements; i++){
            varStamp_t varStamp;
            if(_sdBufferCom.pop(&varStamp)) {
                xQueueSendToBack(_xBufferCom, &varStamp, 0);
            } else {
                _setError("Problem moving data from SD buffer to memory buffer. Check SD.");
            }
        }
    }

}


// Calculate if the variable should be updated or not

bool Esp32MAClientLog::_shouldVarBeUpdated(int varId){

    bool updatedDueToThreshold;
    bool updatedDueToMinPeriod;
    bool updatedDueToMaxPeriod;
    
    unsigned long elapsedTimeVar = _nowMillis - _varList.var[varId]._lastUpdateTime;

    updatedDueToThreshold = (abs(*(_varList.var[varId].ptrValue) - _varList.var[varId]._lastValue) > _varList.var[varId].threshold);
    updatedDueToMinPeriod = (elapsedTimeVar >= _varList.var[varId].minPeriod);
    updatedDueToMaxPeriod = (elapsedTimeVar > _varList.var[varId].maxPeriod && _varList.var[varId].maxPeriod != -1);

    bool varToBeUpdated =  ( updatedDueToMinPeriod && updatedDueToThreshold) || updatedDueToMaxPeriod || _coldStart;

        
    return(varToBeUpdated);
}


// Push variable to the communication buffer

bool Esp32MAClientLog::_pushVarToBuffer(int varId, unsigned long ts) {

    varStamp_t varStamp;

    bool isValueBuffered=false;

    _fillVarFromIdTs(&varStamp, varId, ts);

    // Send structure to buffer

    isValueBuffered = _pushVarToBufferHardware(&varStamp);

    // Either if can be queued or not, move to the next schedule

    _varList.var[varId]._lastUpdateTime = _nowMillis;
    _varList.var[varId]._lastValue = varStamp.value;
    
    if (!isValueBuffered) {

        _varsNotBufferedAndLost++;
        _setError("Problem pushing a var to the buffer. Buffer=" + getBufferInfo());
        Serial.println("Value Lost: " + String(varStamp.varName) + " " + String(varStamp.value) + " " + String(varStamp.ts));
        Serial.println("Messages Lost: " + String(_varsNotBufferedAndLost));

    } 

    return(isValueBuffered);
}




bool Esp32MAClientLog::_pushVarToBufferHardware(varStamp_t* ptrVarStamp) {

    bool allOKLogSD=false;
    bool allOKBuffer=false;
    bool allOKNewFileSD=false;
    bool logToSD;

    logToSD = !_sdBufferCom.empty() && _enableSDLog;

    // If logging to SD, continue logging to SD until SD Buffer is empty
    if (logToSD) {

        allOKLogSD = _sdBufferCom.push(ptrVarStamp);
        if (!allOKLogSD) _setError("Problem pushing a value to a SD Buffer. Check SD.");
    } 

    // In case of NOT logging to SD, or error logging to SD (ie, SD extracted), try to log to Buffer
    if (!logToSD || !allOKLogSD) {
        
        allOKBuffer = (xQueueSendToBack(_xBufferCom, ptrVarStamp, 0) == pdPASS);
        if (uxQueueMessagesWaiting(_xBufferCom)>=2) {
            _setError("Buffering to RAM. " + getBufferInfo());
        }

        // In case of buffer error (overload) and SD enabled, create a new file and log to SD.

        if (!allOKBuffer && _enableSDLog) {

            _sdBufferCom.createFile(FILENAMESD);
            allOKNewFileSD = _sdBufferCom.push(ptrVarStamp);
            if (!allOKNewFileSD) _setError ("Problem pushing value to a new SD file. Check SD.");
        }

    }

    // If something worked, return true.
    return (allOKLogSD || allOKBuffer || allOKNewFileSD);
}



void Esp32MAClientLog::_fillVarFromIdTs(varStamp_t *ptrVar, int varId, unsigned long ts) {

    strcpy(ptrVar->varName, _varList.var[varId].name.substring(0,MAXCHARVARNAME).c_str());
    ptrVar->varId = varId;
    ptrVar->value = *(_varList.var[varId].ptrValue);
    ptrVar->ts = ts;

}



// Return buffer pointer.

QueueHandle_t* Esp32MAClientLog::_getPtrBuffer(){
    return(&_xBufferCom);
}


// Return buffer log information

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

unsigned long* Esp32MAClientLog::_getTsPtr() {

    return(&_lastTs);

}



///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////


// Easy constructor (Based directly on Esp32MAClientLog object)

Esp32MAClientSend::Esp32MAClientSend(String assetName, Esp32MAClientLog &logClient) {

    _assetName = assetName;
    _ptrxBufferCom = logClient._getPtrBuffer();
    _ptrTs = logClient._getTsPtr();
}


// Constructor (Detailed)

Esp32MAClientSend::Esp32MAClientSend(String assetName, QueueHandle_t* ptrxBufferCom, unsigned long* ptrTs) {

    _assetName = assetName;
    _ptrxBufferCom = ptrxBufferCom;
    _ptrTs = ptrTs;
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

    Serial.println("");
    Serial.println("Info: >>>Starting connexion to Machine Advisor");

    allOK = Esp32MQTTClient_Init((const uint8_t*)_connexionString.c_str());
    Esp32MQTTClient_SetSendConfirmationCallback(_SendConfirmationCallback);

    if (!allOK) _setError("Problem connecting to Machine Advisor. Check connection credentials");
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
                    Serial.println("Last message was buffered=" + getBufferInfo());
                }

            } else {
                _setError("Problem sending the message to Machine Advisor. Check connection status. Buffer=" + getBufferInfo());
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


        if (isMessageSent) Serial.println("Message sent =" + mqttMessage);

    } 

    _lastIsWifiOK = isComOK;
    _lastIsComFullOK = isComFullOK;

    return(isMessageSent);
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



///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

SDBuffer::SDBuffer() {}

bool SDBuffer::init(){

    if (!_SDinit) {

        if(!SD.begin(SD_GPIO)) Serial.println("Card Mount Failed");

        else {
            uint8_t cardType = SD.cardType();
            if (cardType == CARD_NONE) Serial.println("No SD card attached");    
            else _SDinit = true;
        }
    }

    return(_SDinit);
}


bool SDBuffer::setFileName(String fileName) {

    _fileName = fileName;

    return(createFile(_fileName));
      
}


bool SDBuffer::fileExist() {

    return(SD.exists(_fileName));

}

bool SDBuffer::createFile(String fileName) {

    bool allOK=false;

    String dataMessage = "VarName,Value,TimeStamp\n";
    Serial.print("Save data: ");
    Serial.println(dataMessage);

    allOK = _writeAppendFile(SD, fileName.c_str(), dataMessage.c_str(), FILE_WRITE);

    if (allOK) {
        _bufferSize = 0;
        _currentPointer = dataMessage.length();
    }

    return (allOK); 
}


int SDBuffer::bufferSize() {
    return (_bufferSize);
}


uint64_t SDBuffer::_size() {
    return (SD.usedBytes());
}

bool SDBuffer::pop(varStamp_t* ptrVarStamp, bool onlyPeek){

    bool allOK=false;

    if (_bufferSize > 0) {

        File file = SD.open(_fileName, FILE_READ);

        if(!file) {

            Serial.println("Failed to open file for writing");
            allOK = false;

        } else {

            file.seek(_currentPointer);

            String lineStr = file.readStringUntil('\n');

            Serial.println("Pop from SD: " + lineStr);

            if (!onlyPeek)  {
                _currentPointer = file.position();
                _bufferSize--;
            }

            int coma1 = lineStr.indexOf(",");
            int coma2 = lineStr.indexOf(",", coma1+1);
            int size = lineStr.length();

            strcpy(ptrVarStamp->varName, lineStr.substring(0,coma1).substring(0,MAXCHARVARNAME).c_str());
            ptrVarStamp->value = lineStr.substring(coma1+1,coma2).toInt();
            ptrVarStamp->ts = strtoul(lineStr.substring(coma2+1,size).c_str(), NULL, 0);

            file.close();

            allOK = true;
        }
    } 

    return (allOK);

}

bool SDBuffer::peek(varStamp_t* ptrVarStamp) {
    return(pop(ptrVarStamp, true));
}

bool SDBuffer::push(varStamp_t* ptrVarStamp){

    bool allOK=false;

    String lineStr = String(ptrVarStamp->varName) + ',' + String(ptrVarStamp->value) + ',' + String(ptrVarStamp->ts) + '\n';

    Serial.println("Push to SD: " + lineStr);
    
    allOK = _writeAppendFile(SD, _fileName.c_str(), lineStr.c_str(), FILE_APPEND);


    if (allOK) _bufferSize ++; // Increase buffer counter if no error appending the file

    return(allOK);
}

bool SDBuffer::empty(){

    return(_bufferSize == 0);

}




// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
bool SDBuffer::_writeAppendFile(fs::FS &fs, const char * path, const char * message, const char* option) {

    bool allOK=false;

    // TODO: Change it to dettect if SD has been extracted
    if(fs.exists(path)) {

        File file = fs.open(path, option);

        if (!file) allOK=false;
        else if (!file.print(message)) { 
            Serial.println("Failed to open or appending");
        } else allOK=true;

        file.close();

    } 
    
    return(allOK);

}



