#include "DebugMgr.hpp"

DebugMgr::DebugMgr(){}

void DebugMgr::setLibName(String libName){
    _libName = libName;
}

// Error logging methods

void DebugMgr::setError(String textError, unsigned long ts){
    _globalError = true;
    _lastErrorText = textError;
    _lastErrorMillis = ts;
    _numErrors = (_numErrors+1) % LONG_MAX;

    String strTs = ts==-1 ? "" : _getHumanDate(ts);

    String errorMsg = "Err(" + _libName + "): " + textError + " " + strTs + " TotErr=" + String(getNumErrors()); 
    Serial.println(errorMsg);

    #ifdef USE_M5STACK
    M5.Lcd.println(errorMsg);
    #endif

    _lastMsg = errorMsg;
}

void DebugMgr::resetError(){
    _globalError = false;
    _lastErrorText = "";
    _lastErrorMillis = 0;
    _numErrors=0;
}

int DebugMgr::getNumErrors(){
    return (_numErrors);
}

void DebugMgr::setMsg(String msgText, unsigned long ts) {

    String strTs = ts==-1 ? "" : _getHumanDate(ts);

    String msg = "Msg(" + _libName + "): " + msgText + " " + strTs; 
    Serial.println(msg);

    #ifdef USE_M5STACK
    M5.Lcd.println(msg);
    #endif

    _lastMsg = msg;

}

String DebugMgr::_getHumanDate(unsigned long timeStamp) {

    time_t rawtime = (time_t)timeStamp;
    struct tm ts;
    char buf[80];

    // Format time, "ddd yyyy-mm-dd hh:mm:ss"
    ts = *localtime(&rawtime);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S", &ts);
    //printf("%s\n", buf);
    return(String(buf));
}

String DebugMgr::getLastMessage() {
    return(_lastMsg);
}
