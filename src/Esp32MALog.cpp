
#include "Esp32MALog.hpp"

// Constructor

Esp32MAClientLog::Esp32MAClientLog(bool enableSDLog){

    _enableSDLog = enableSDLog;

    // creation of freertos FIFO queue (Thread safe)

    _xBufferCom = xQueueCreate( MAXBUFFER, sizeof(varStamp_t));

    if(_xBufferCom == NULL){
        _debug.setError("Creating memory thread safe buffer. Check memory allocation.");
    }

    _debug.setLibName("Log");

}

// Registering variables to be sent to Machine Advisor IOT Hub

// minPeriod = period in milliseconds
// thershold (optinal) = if filled, the variable will be sent if the change>threshold and minPeriod has ocurred
// maxPeriod (optional) = if filled, maximum period without sending the variable

int Esp32MAClientLog::registerVar(String name, int *ptrValue, int minPeriod, int threshold, int maxPeriod){

    int varId=-1;
    bool allOk=false;

    // TODO: Done here to make it compatible with M5Stack SD management
    // Should the init be done in a separate method?

    if (!_logInitialized) {

        if (_enableSDLog ) {
            if (!_sdBufferCom.init()) {
                _debug.setError("Problem mounting the SD. Check SD Card.", _lastTs);
            } else _debug.setMsg("SD Initalized", _lastTs);
            
            if (!_sdBufferCom.setFileName(FILENAMESD)) {
                _debug.setError("Problem intializing SD file for buffer. Check SD card.", _lastTs);
            } else _debug.setMsg("Buffer file created", _lastTs);
        }

        _logInitialized = true;
    }

    // Starting register variable 

    if (_varList.num < MAXNUMVARS) {

        allOk = _registerVarAtPosition(_varList.num, name, ptrValue, minPeriod, threshold, maxPeriod);

        if (allOk) {
            varId = _varList.num;
            _varList.num ++;
            _debug.setMsg("Variable registered: " + name, _lastTs);
        }

    } else {

        varId = -1;
        _debug.setError("No more space for new variables. Increase the pre-allocated memory.", _lastTs);
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
        _debug.setError("Only can be updated a variable already registered. Register it first.", _lastTs);
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
    _debug.setError("Variable not found", _lastTs);
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
                _debug.setError("Problem moving data from SD buffer to memory buffer. Check SD.", _lastTs);
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
        String errorMsg;
        errorMsg = "Problem pushing a var to the buffer. Buffer=" + getBufferInfo() + String("\n");
        errorMsg += "Value Lost: " + String(varStamp.varName) + " " + String(varStamp.value) + " " + String(varStamp.ts) + String("\n");
        errorMsg += "Messages Lost: " + String(_varsNotBufferedAndLost) + String("");
        _debug.setError(errorMsg, _lastTs);

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
        if (!allOKLogSD) _debug.setError("Problem pushing a value to a SD Buffer. Check SD.", _lastTs);

    } 

    // In case of NOT logging to SD, or error logging to SD (ie, SD extracted), try to log to Buffer
    if (!logToSD || !allOKLogSD) {
        
        allOKBuffer = (xQueueSendToBack(_xBufferCom, ptrVarStamp, 0) == pdPASS);
        if (uxQueueMessagesWaiting(_xBufferCom)>=2) {
            _debug.setError("Buffering to RAM. " + getBufferInfo(), _lastTs);
        }

        // In case of buffer error (overload) and SD enabled, create a new file and log to SD.

        if (!allOKBuffer && _enableSDLog) {

            _sdBufferCom.createFile(FILENAMESD);
            allOKNewFileSD = _sdBufferCom.push(ptrVarStamp);
            if (!allOKNewFileSD) _debug.setError ("Problem pushing value to a new SD file. Check SD.", _lastTs);
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



// Get the current time stamp pointer

unsigned long* Esp32MAClientLog::_getTsPtr() {

    return(&_lastTs);

}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////



