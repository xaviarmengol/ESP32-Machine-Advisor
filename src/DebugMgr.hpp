#ifndef DEBUGMGR_HPP
#define DEBUGMGR_HPP

#include <Arduino.h>


// If using M5Stack, uncomment this 
//#define USE_M5STACK

#ifdef USE_M5STACK
#include <M5Stack.h>
#endif


class DebugMgr {

    public:
    
        DebugMgr();

        void setLibName(String libName);

        void setError(String errorText = "> Error with no text", unsigned long ts=-1);
        void setMsg(String msgText = "> Msg with no text", unsigned long ts=-1);

        void resetError();
        int getNumErrors();

        String getLastMessage();

    private:

        String _libName;

        String _lastErrorText="";
        unsigned long _lastErrorMillis=0;

        bool _globalError=false;
        long int _numErrors=0;

        String _getHumanDate(unsigned long timeStamp);

        String _lastMsg;
};

#endif