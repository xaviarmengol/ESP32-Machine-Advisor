#ifndef DATASTRUCTURE_H
#define DATASTRUCTURE_H

// Type: List of registered variables

#include <Arduino.h>

// Library defines

#define MAXNUMVARS 32 // Max num of variables to log
#define MAXBUFFER 64 // Max size of the ram buffer
#define MILLISSENDPERIOD 1000 // Minimum period between messages to Machine Advisor
#define COMRECOVERYDELAY 1000 // Timeout after recovering Wifi/communications
#define MAXCHARVARNAME 15 // Maximum chars of the var name

#define ENDPOINTAPI "https://api.machine-advisor.schneider-electric.com/download/{{clientidnum}}/%5B%22{{device}}%3A{{varname}}%22%5D/{{tsini}}/{{tsend}}"


typedef struct varRegister_t {
    String name;
    int* ptrValue;  // ptr to variabl
    int minPeriod;  // updating minimum period
    int threshold;  // threshold in case of updating by change (optional)
    int maxPeriod;  // max time without updating (optional)

    int _lastValue; // last value sent
    unsigned long _lastUpdateTime; // millis when las value was sent

} varRegister_t;

typedef struct varRegisterList_t {
    varRegister_t var[MAXNUMVARS];
    int num = 0;    // num of registered variables
} varRegisterList_t;


// Type: Variable with time stamp. To push to the buffer

typedef struct varStamp_t {
    char varName[MAXCHARVARNAME];
    uint8_t  varId;
    int	value;
    unsigned long ts;
} varStamp_t;

#endif