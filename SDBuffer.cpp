
#include <Arduino.h>
#include "SDBuffer.hpp"

SDBuffer::SDBuffer() {}

bool SDBuffer::init(){

    if (!_SDinit) {

        // If we use M5Stack, the SD initialitzation is done in M5 intit.
        #ifndef USE_M5STACK
            if(!SD.begin(SD_GPIO)) _debug._setError("Card Mount Failed");

            else {
                uint8_t cardType = SD.cardType();
                if (cardType == CARD_NONE) _debug._setError("No SD card attached");    
                else _SDinit = true;
            }
        #endif

        #ifdef USE_M5STACK
            _SDinit = true;
        #endif

        debug.setLibName("SDBuff");
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
    debug.setMsg("Save data: " + dataMessage);

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

            debug.setError("Failed to open file for writing");
            allOK = false;

        } else {

            file.seek(_currentPointer);

            String lineStr = file.readStringUntil('\n');

            debug.setMsg("Pop from SD: " + lineStr);

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

    debug.setMsg("Push to SD: " + lineStr);
    
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

    File file = fs.open(path, option);

    if (!file) allOK=false;
    else if (!file.print(message)) { 
        debug.setError("Failed to open or appending");
    } else allOK=true;

    file.close();

    return(allOK);

}

