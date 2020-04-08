#ifndef SDBUFFER_HPP
#define SDBUFFER_HPP

#include <Arduino.h>
#include "dataStructure.h"
#include "DebugMgr.hpp"

#define FILENAMESD "/sdbuffer.csv"
#define SD_GPIO 4 // Pin where the SD is attached

// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>
#define ESP32MALOG_SD true

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

// Class to manage SD Buffer.
// It is a FIFO buffer written in a file. 
// Pop is done positioning a pointer with seek, and reading 
// Push is done appending in the file.


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

        bool _writeAppendFile(fs::FS &fs, const char * path, const char * message, const char * option);

        uint64_t _size();

        // Error mgm

        DebugMgr _debug;

};

#endif