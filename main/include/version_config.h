#ifndef VERSION_CONFIG_H
#define VERSION_CONFIG_H
#include "sdkconfig.h"

#define FIRMWARE_VERSION CONFIG_FIRMWARE_VERSION


// alternatively you could add your global method getLibInterfaceVersion here
const char* getFirmwareVersion()
{
    return FIRMWARE_VERSION;
}

#endif // VERSION_CONFIG_H
