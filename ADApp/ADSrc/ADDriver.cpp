/** ADDriver.cpp
 *
 * This is the base class from which actual area detectors are derived.
 *
 * /author Author: Mark Rivers
 *         University of Chicago
 *
 * Created:  March 20, 2008
 *
 */

#include <stdio.h>

#include <epicsString.h>
#include <epicsThread.h>
#include <asynStandardInterfaces.h>

/* Defining this will create the static table of standard parameters in ADInterface.h */
#define DEFINE_AD_STANDARD_PARAMS 1
#include "ADStdDriverParams.h"
#include "ADDriver.h"


static const char *driverName = "ADDriver";

/** Set the shutter postition. */
/** This method will open (1) or close (0) the shutter if
  * ADShutterMode==ADShutterModeEPICS. Drivers will implement setShutter if they
  * support ADShutterModeDetector. If ADShutterMode=ADShutterModeDetector they will
  * control the shutter directly, else they will call this method.
  * \param[in] open 1(open) or 0(closed)
  */
void ADDriver::setShutter(int open)
{
    ADShutterMode_t shutterMode;
    double delay;
    double shutterOpenDelay, shutterCloseDelay;

    getIntegerParam(ADShutterMode, (int *)&shutterMode);
    getDoubleParam(ADShutterOpenDelay, &shutterOpenDelay);
    getDoubleParam(ADShutterCloseDelay, &shutterCloseDelay);

    switch (shutterMode) {
        case ADShutterModeNone:
            break;
        case ADShutterModeEPICS:
            setIntegerParam(ADShutterControlEPICS, open);
            callParamCallbacks();
            delay = shutterOpenDelay - shutterCloseDelay;
            epicsThreadSleep(delay);
            break;
        case ADShutterModeDetector:
            break;
    }
}

/** Build a file name from component parts */
/** This is a convenience function that constructs a complete file name in the
  * ADFullFileName parameter from the ADFilePath, ADFileName, ADFileNumber, and
  * ADFileTemplate parameters.
  * \param maxChars
  * \param fullFileName The constructed file name.
  */
int ADDriver::createFileName(int maxChars, char *fullFileName)
{
    /* Formats a complete file name from the components defined in ADStdDriverParams.h */
    int status = asynSuccess;
    char filePath[MAX_FILENAME_LEN];
    char fileName[MAX_FILENAME_LEN];
    char fileTemplate[MAX_FILENAME_LEN];
    int fileNumber;
    int autoIncrement;
    int len;
    int addr=0;

    status |= getStringParam(addr, ADFilePath, sizeof(filePath), filePath);
    status |= getStringParam(addr, ADFileName, sizeof(fileName), fileName);
    status |= getStringParam(addr, ADFileTemplate, sizeof(fileTemplate), fileTemplate);
    status |= getIntegerParam(addr, ADFileNumber, &fileNumber);
    status |= getIntegerParam(addr, ADAutoIncrement, &autoIncrement);
    if (status) return(status);
    len = epicsSnprintf(fullFileName, maxChars, fileTemplate,
                        filePath, fileName, fileNumber);
    if (len < 0) {
        status |= asynError;
        return(status);
    }
    if (autoIncrement) {
        fileNumber++;
        status |= setIntegerParam(addr, ADFileNumber, fileNumber);
    }
    return(status);
}

/** I need some documentation.*/
asynStatus ADDriver::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *functionName = "writeInt32";

    status = setIntegerParam(function, value);

    switch (function) {
    case ADShutterControl:
        setShutter(value);
        break;
    default:
        break;
    }

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();

    if (status)
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:%s: error, status=%d function=%d, value=%d\n",
              driverName, functionName, status, function, value);
    else
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:%s: function=%d, value=%d\n",
              driverName, functionName, function, value);
    return status;
}


/* asynDrvUser routines */
/** This method returns one of the enum values for the parameters defined in ADStdDriverParams.h
  * if the driverInfo field matches one the strings defined in
  * that file.*/
/** Derived classes will typically provide an implementation of
  * drvUserCreate() that searches for parameters that are unique to that detector
  * driver. If a parameter is not matched, then ADDriver->drvUserCreate() will be
  * called to see if it is a standard driver parameter (defined in
  * ADStdDriverParams.h).
  */
asynStatus ADDriver::drvUserCreate(asynUser *pasynUser,
                                       const char *drvInfo,
                                       const char **pptypeName, size_t *psize)
{
    int status;
    int param;
    const char *functionName = "drvUserCreate";

    /* See if this is one of the standard parameters */
    status = findParam(ADStdDriverParamString, NUM_AD_STANDARD_PARAMS,
                       drvInfo, &param);

    if (status == asynSuccess) {
        pasynUser->reason = param;
        if (pptypeName) {
            *pptypeName = epicsStrDup(drvInfo);
        }
        if (psize) {
            *psize = sizeof(param);
        }
        asynPrint(pasynUser, ASYN_TRACE_FLOW,
                  "%s:%s:, drvInfo=%s, param=%d\n",
                  driverName, functionName, drvInfo, param);
        return(asynSuccess);
    } else {
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                     "%s:%s:, unknown drvInfo=%s",
                     driverName, functionName, drvInfo);
        return(asynError);
    }
}


/** All of the arguments are simply passed to
  * the constructor for the asynNDArrayDriver base class. After calling the base class
  * constructor this method sets reasonable default values for all of the parameters
  * defined in ADStdDriverParams.h.
  */
ADDriver::ADDriver(const char *portName, int maxAddr, int paramTableSize, int maxBuffers, size_t maxMemory,
                   int interfaceMask, int interruptMask,
                   int asynFlags, int autoConnect, int priority, int stackSize)

    : asynNDArrayDriver(portName, maxAddr, paramTableSize, maxBuffers, maxMemory,
          interfaceMask | asynInt32Mask | asynFloat64Mask | asynOctetMask | asynGenericPointerMask | asynDrvUserMask,
          interruptMask | asynInt32Mask | asynFloat64Mask | asynOctetMask | asynGenericPointerMask,
          asynFlags, autoConnect, priority, stackSize)

{
    //char *functionName = "ADDriver";

    /* Set some default values for parameters */
    setStringParam(ADPortNameSelf, portName);
    setStringParam(ADManufacturer, "Unknown");
    setStringParam(ADModel,        "Unknown");
    setDoubleParam (ADGain,         1.0);
    setIntegerParam(ADBinX,         1);
    setIntegerParam(ADBinY,         1);
    setIntegerParam(ADMinX,         0);
    setIntegerParam(ADMinY,         0);
    setIntegerParam(ADSizeX,        1);
    setIntegerParam(ADSizeY,        1);
    setIntegerParam(ADMaxSizeX,     1);
    setIntegerParam(ADMaxSizeY,     1);
    setIntegerParam(ADReverseX,     0);
    setIntegerParam(ADReverseY,     0);
    setIntegerParam(ADImageSizeX,   0);
    setIntegerParam(ADImageSizeY,   0);
    setIntegerParam(ADImageSizeZ,   0);
    setIntegerParam(ADImageSize,    0);
    setIntegerParam(ADDataType,     NDUInt8);
    setIntegerParam(ADColorMode,    NDColorModeMono);
    setIntegerParam(ADFrameType,    ADFrameNormal);
    setIntegerParam(ADImageMode,    ADImageContinuous);
    setIntegerParam(ADTriggerMode,  0);
    setIntegerParam(ADNumExposures, 1);
    setIntegerParam(ADNumImages,    1);
    setDoubleParam (ADAcquireTime,  1.0);
    setDoubleParam (ADAcquirePeriod,0.0);
    setIntegerParam(ADStatus,       ADStatusIdle);
    setIntegerParam(ADAcquire,      0);
    setIntegerParam(ADImageCounter, 0);
    setIntegerParam(ADNumImagesCounter, 0);
    setIntegerParam(ADNumExposuresCounter, 0);
    setDoubleParam( ADTimeRemaining, 0.0);
    setIntegerParam(ADShutterControl, 0);
    setIntegerParam(ADShutterStatus, 0);
    setIntegerParam(ADShutterMode,   0);
    setDoubleParam (ADShutterOpenDelay, 0.0);
    setDoubleParam (ADShutterCloseDelay, 0.0);
    setDoubleParam (ADTemperature, 0.0);

    setStringParam (ADFilePath,     "");
    setStringParam (ADFileName,     "");
    setIntegerParam(ADFileNumber,   0);
    setStringParam (ADFileTemplate, "");
    setIntegerParam(ADAutoIncrement, 0);
    setStringParam (ADFullFileName, "");
    setIntegerParam(ADFileFormat,   0);
    setIntegerParam(ADAutoSave,     0);
    setIntegerParam(ADWriteFile,    0);
    setIntegerParam(ADReadFile,     0);
    setIntegerParam(ADFileWriteMode,   0);
    setIntegerParam(ADFileNumCapture,  0);
    setIntegerParam(ADFileNumCaptured, 0);
    setIntegerParam(ADFileCapture,     0);

    setStringParam (ADStatusMessage,  "");
    setStringParam (ADStringToServer, "");
    setStringParam (ADStringFromServer,  "");
    setIntegerParam(ADArrayCallbacks, 1);
}
