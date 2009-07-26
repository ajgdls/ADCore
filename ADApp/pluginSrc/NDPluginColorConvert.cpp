/*
 * NDPluginColorConvert.cpp
 * 
 * Plugin to convert from one color mode to another
 *
 * Author: Mark Rivers
 *
 * Created December 22, 2008
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <epicsMutex.h>
#include <epicsString.h>
#include <iocsh.h>
#include <epicsExport.h>

#include "NDPluginColorConvert.h"
#include "PvApi.h"

static asynParamString_t NDPluginColorConvertParamString[] = {
    {NDPluginColorConvertColorModeOut,   "COLOR_MODE_OUT"},
};

static const char *driverName="NDPluginColorConvert";

/* This function returns 1 if it did a conversion, 0 if it did not */
template <typename epicsType>
void NDPluginColorConvert::convertColor(NDArray *pArray)
{
    NDColorMode_t colorModeOut;
    const char* functionName = "convertColor";
    int i, j;
    epicsType *pIn, *pRedIn, *pGreenIn, *pBlueIn;
    epicsType *pOut, *pRedOut, *pGreenOut, *pBlueOut;
    epicsType *pDataIn  = (epicsType *)pArray->pData;
    epicsType *pDataOut;
    NDArray *pArrayOut=NULL;
    int imageSize, rowSize, numRows;
    tPvFrame PvFrame, *pFrame=&PvFrame;
    int dims[ND_ARRAY_MAX_DIMS];
    int ndims;
    int colorMode, bayerPattern;
    NDAttribute *pAttribute;
     
    getIntegerParam(NDPluginColorConvertColorModeOut, (int *)&colorModeOut);
    pAttribute = pArray->findAttribute("colorMode");
    if (pAttribute) pAttribute->getValue(NDAttrInt32, &colorMode);
    pAttribute = pArray->findAttribute("bayerPattern");
    if (pAttribute) pAttribute->getValue(NDAttrInt32, &bayerPattern);
       
    /* This function is called with the lock taken, and it must be set when we exit.
     * The following code can be exected without the mutex because we are not accessing elements of
     * pPvt that other threads can access. */
    this->unlock();
    switch (colorMode) {
        case NDColorModeBayer:
            switch (colorModeOut) {
                case NDColorModeRGB1:
                case NDColorModeRGB2:
                case NDColorModeRGB3:
                    rowSize   = pArray->dims[0].size;
                    numRows   = pArray->dims[1].size;
                    imageSize = rowSize * numRows;
                    ndims = 3;
                    dims[0] = 3;
                    dims[1] = rowSize;
                    dims[2] = numRows;
                    /* There is a problem: the uniqueId and timeStamp are not preserved! */
                    pArrayOut = this->pNDArrayPool->alloc(ndims, dims, pArray->dataType, 0, NULL);
                    pArrayOut->uniqueId = pArray->uniqueId;
                    pArrayOut->timeStamp = pArray->timeStamp;
                    pDataOut = (epicsType *)pArrayOut->pData;
                    /* For now we use the Prosilica library functions to convert Bayer to RGB */
                    /* This requires creating their tPvFrame data structure */
                    memset(pFrame, 0, sizeof(tPvFrame));
                    pFrame->Width = pArray->dims[0].size;
                    pFrame->Height = pArray->dims[1].size;
                    pFrame->RegionX = pArray->dims[0].offset;
                    pFrame->RegionY = pArray->dims[1].offset;
                    pFrame->ImageBuffer = pArray->pData;
                    pFrame->ImageBufferSize = pArray->dataSize;
                    pFrame->ImageSize = pFrame->ImageBufferSize;
                    pFrame->BayerPattern = (tPvBayerPattern)bayerPattern;
                    switch(pArray->dataType) {
                        case NDInt8:
                        case NDUInt8:
                            pFrame->Format = ePvFmtBayer8;
                            pFrame->BitDepth = 8;
                            break;
                        case NDInt16:
                        case NDUInt16:
                            pFrame->Format = ePvFmtBayer16;
                            pFrame->BitDepth = 16;
                            break;
                        default:
                            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                                "%s:%s: error unsupported data type=%d\n",
                                driverName, functionName, pArray->dataType);
                            break;
                    }
                    break;
                default: 
                    break;
            }
            switch (colorModeOut) {
                case NDColorModeRGB1:
                    PvUtilityColorInterpolate(pFrame, pDataOut,  pDataOut+1,  pDataOut+2, 2, 0);
                    pArrayOut->dims[0].size = 3;
                    memcpy(&pArrayOut->dims[1], &pArray->dims[0], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[2], &pArray->dims[1], sizeof(NDDimension_t));
                    pArrayOut->addAttribute("colorMode", NDAttrInt32, &colorModeOut);
                    break;
                
                case NDColorModeRGB2:
                    PvUtilityColorInterpolate(pFrame, pDataOut,  pDataOut+rowSize,  pDataOut+2*rowSize, 0, 2*rowSize);
                    memcpy(&pArrayOut->dims[0], &pArray->dims[0], sizeof(NDDimension_t));
                    pArrayOut->dims[1].size = 3;
                    memcpy(&pArrayOut->dims[2], &pArray->dims[1], sizeof(NDDimension_t));
                    pArrayOut->addAttribute("colorMode", NDAttrInt32, &colorModeOut);
                    break;

                case NDColorModeRGB3:
                    PvUtilityColorInterpolate(pFrame, pDataOut,  pDataOut+imageSize,  pDataOut+2*imageSize, 0, 0);
                    memcpy(&pArrayOut->dims[0], &pArray->dims[0], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[1], &pArray->dims[1], sizeof(NDDimension_t));
                    pArrayOut->dims[2].size = 3;
                    pArrayOut->addAttribute("colorMode", NDAttrInt32, &colorModeOut);
                    break;
                default:
                    break;
            }
            break;
        case NDColorModeRGB1:
            rowSize   = pArray->dims[1].size;
            numRows   = pArray->dims[2].size;
            imageSize = rowSize * numRows;
            switch (colorModeOut) {
                case NDColorModeRGB3:
                    pArrayOut = this->pNDArrayPool->copy(pArray, NULL, 0);
                    pDataOut = (epicsType *)pArrayOut->pData;
                    pIn = pDataIn;
                    pRedOut   = pDataOut;
                    pGreenOut = pDataOut + imageSize;
                    pBlueOut  = pDataOut + 2*imageSize;
                    for (i=0; i<imageSize; i++) {
                        *pRedOut++   = *pIn++;
                        *pGreenOut++ = *pIn++;
                        *pBlueOut++  = *pIn++;
                    }
                    memcpy(&pArrayOut->dims[0], &pArray->dims[1], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[1], &pArray->dims[2], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[2], &pArray->dims[0], sizeof(NDDimension_t));
                    pArrayOut->addAttribute("colorMode", NDAttrInt32, &colorModeOut);
                    break;
                case NDColorModeRGB2:
                    pArrayOut = this->pNDArrayPool->copy(pArray, NULL, 0);
                    pDataOut = (epicsType *)pArrayOut->pData;
                    pIn = pDataIn;
                    for (i=0; i<numRows; i++) {
                        pRedOut   = pDataOut + 3*i*rowSize;
                        pGreenOut = pRedOut + rowSize;
                        pBlueOut  = pRedOut + 2*rowSize;
                        for (j=0; j<rowSize; j++) {
                            *pRedOut++   = *pIn++;
                            *pGreenOut++ = *pIn++;
                            *pBlueOut++  = *pIn++;
                        }
                    }
                    memcpy(&pArrayOut->dims[0], &pArray->dims[1], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[1], &pArray->dims[0], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[2], &pArray->dims[2], sizeof(NDDimension_t));
                    pArrayOut->addAttribute("colorMode", NDAttrInt32, &colorModeOut);
                    break;
                default:
                    break;
            }        
            break;
        case NDColorModeRGB2:
            rowSize   = pArray->dims[0].size;
            numRows   = pArray->dims[2].size;
            imageSize = rowSize * numRows;
            switch (colorModeOut) {
                case NDColorModeRGB1:
                    pArrayOut = this->pNDArrayPool->copy(pArray, NULL, 0);
                    pDataOut = (epicsType *)pArrayOut->pData;
                    pOut = pDataOut;
                    for (i=0; i<numRows; i++) {
                        pRedIn   = pDataIn + 3*i*rowSize;
                        pGreenIn = pRedIn + rowSize;
                        pBlueIn  = pRedIn + 2*rowSize;
                        for (j=0; j<rowSize; j++) {
                            *pOut++  = *pRedIn++;
                            *pOut++  = *pGreenIn++;
                            *pOut++  = *pBlueIn++;
                        }
                    }
                    memcpy(&pArrayOut->dims[0], &pArray->dims[1], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[1], &pArray->dims[0], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[2], &pArray->dims[2], sizeof(NDDimension_t));
                    pArrayOut->addAttribute("colorMode", NDAttrInt32, &colorModeOut);
                    break;
                case NDColorModeRGB3:
                    pArrayOut = this->pNDArrayPool->copy(pArray, NULL, 0);
                    pDataOut = (epicsType *)pArrayOut->pData;
                    pRedOut   = pDataOut;
                    pGreenOut = pDataOut + imageSize;
                    pBlueOut  = pDataOut + 2*imageSize;
                    for (i=0; i<numRows; i++) {
                        pRedIn   = pDataIn + 3*i*rowSize;
                        pGreenIn = pRedIn + rowSize;
                        pBlueIn  = pRedIn + 2*rowSize;
                        for (j=0; j<rowSize; j++) {
                            *pRedOut++   = *pRedIn++;
                            *pGreenOut++ = *pGreenIn++;
                            *pBlueOut++  = *pBlueIn++;
                        }
                    }
                    memcpy(&pArrayOut->dims[0], &pArray->dims[0], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[1], &pArray->dims[2], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[2], &pArray->dims[1], sizeof(NDDimension_t));
                    pArrayOut->addAttribute("colorMode", NDAttrInt32, &colorModeOut);
                    break;
                default:
                    break;
            }        
            break;
        case NDColorModeRGB3:
            rowSize   = pArray->dims[0].size;
            numRows   = pArray->dims[1].size;
            imageSize = rowSize * numRows;
            switch (colorModeOut) {
                case NDColorModeRGB1:
                    pArrayOut = this->pNDArrayPool->copy(pArray, NULL, 0);
                    pDataOut = (epicsType *)pArrayOut->pData;
                    pRedIn   = pDataIn;
                    pGreenIn = pDataIn + imageSize;
                    pBlueIn  = pDataIn + 2*imageSize;
                    pOut = pDataOut;
                    for (i=0; i<imageSize; i++) {
                        *pOut++ = *pRedIn++;
                        *pOut++ = *pGreenIn++;
                        *pOut++ = *pBlueIn++;
                    }
                    memcpy(&pArrayOut->dims[0], &pArray->dims[2], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[1], &pArray->dims[0], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[2], &pArray->dims[1], sizeof(NDDimension_t));
                    pArrayOut->addAttribute("colorMode", NDAttrInt32, &colorModeOut);
                    break;
                case NDColorModeRGB2:
                    pArrayOut = this->pNDArrayPool->copy(pArray, NULL, 0);
                    pDataOut = (epicsType *)pArrayOut->pData;
                    pRedIn   = pDataIn;
                    pGreenIn = pDataIn + imageSize;
                    pBlueIn  = pDataIn + 2*imageSize;
                    for (i=0; i<numRows; i++) {
                        pRedOut   = pDataOut + 3*i*rowSize;
                        pGreenOut = pRedOut + rowSize;
                        pBlueOut  = pRedOut + 2*rowSize;
                        for (j=0; j<rowSize; j++) {
                            *pRedOut++   = *pRedIn++;
                            *pGreenOut++ = *pGreenIn++;
                            *pBlueOut++  = *pBlueIn++;
                        }
                    }
                    memcpy(&pArrayOut->dims[0], &pArray->dims[0], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[1], &pArray->dims[2], sizeof(NDDimension_t));
                    memcpy(&pArrayOut->dims[2], &pArray->dims[1], sizeof(NDDimension_t));
                    pArrayOut->addAttribute("colorMode", NDAttrInt32, &colorModeOut);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    /* If the output array pointer is null then no conversion was done, copy the input to the output */
    if (!pArrayOut) pArrayOut = this->pNDArrayPool->copy(pArray, NULL, 1);
    this->pArrays[0] = pArrayOut;
    this->lock();
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
              "%s:%s: pArray->colorMode=%d, colorModeOut=%d, pArrayOut=%p\n",
              driverName, functionName, colorMode, colorModeOut, pArrayOut);
}

/** Callback function that is called by the NDArray driver with new NDArray data.
  * Looks for the NDArray attribute called "colorMode" to determine the color
  * mode of the input array.  Uses the parameter NDPluginColorConvertColorModeOut
  * to determine the desired color mode of the output array.  The NDArray is converted
  * between these color modes if possible.  If not the input array is passed on without
  * being changed.  Does callbacks to all registered clients on the asynGenericPointer
  * interface with the output array.
  * \param[in] pArray  The NDArray from the callback.
  */ 
void NDPluginColorConvert::processCallbacks(NDArray *pArray)
{
    /* This function converts the color mode.
     * If no conversion can be performed it simply uses the input as the output
     * It is called with the mutex already locked.  It unlocks it during long calculations when private
     * structures don't need to be protected.
     */
     
    const char* functionName = "processCallbacks";
    
    /* Get the attributes for this plugin */
    pArray = this->getAttributesCopy(pArray, true);
     
    /* Call the base class method */
    NDPluginDriver::processCallbacks(pArray);

    /* We always keep the last array so read() can use it.  
     * Release previous one. Reserve new one below. */
    if (this->pArrays[0]) {
        this->pArrays[0]->release();
        this->pArrays[0] = NULL;
    }

    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
              "%s:%s: dataType=%d\n",
              driverName, functionName, pArray->dataType);

    switch (pArray->dataType) {
        case NDInt8:
            this->convertColor<epicsInt8>(pArray);
            break;
        case NDUInt8:
            this->convertColor<epicsUInt8>(pArray);
            break;
        case NDInt16:
            this->convertColor<epicsInt16>(pArray);
            break;
        case NDUInt16:
            this->convertColor<epicsUInt16>(pArray);
            break;
       case NDInt32:
            this->convertColor<epicsInt32>(pArray);
            break;
        case NDUInt32:
            this->convertColor<epicsUInt32>(pArray);
            break;
        case NDFloat32:
            this->convertColor<epicsFloat32>(pArray);
            break;
        case NDFloat64:
            this->convertColor<epicsFloat64>(pArray);
            break;
        default:
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
                      "%s:%s: ERROR: unknown data type=%d\n",
                      driverName, functionName, pArray->dataType);
            break;
    }
    
    callParamCallbacks();
    /* Call any clients who have registered for NDArray callbacks */
    doCallbacksGenericPointer(this->pArrays[0], NDArrayData, 0);
    
    /* Release the array */
    pArray->release();
}




/* asynDrvUser interface methods */
/** Sets pasynUser->reason to one of the enum values for the parameters defined for
  * this class if the drvInfo field matches one the strings defined for it.
  * If the parameter is not recognized by this class then calls NDPluginDriver::drvUserCreate.
  * Uses asynPortDriver::drvUserCreateParam.
  * \param[in] pasynUser pasynUser structure that driver modifies
  * \param[in] drvInfo String containing information about what driver function is being referenced
  * \param[out] pptypeName Location in which driver puts a copy of drvInfo.
  * \param[out] psize Location where driver puts size of param 
  * \return Returns asynSuccess if a matching string was found, asynError if not found. */
asynStatus NDPluginColorConvert::drvUserCreate(asynUser *pasynUser,
                                       const char *drvInfo, 
                                       const char **pptypeName, size_t *psize)
{
    asynStatus status;
    //const char *functionName = "drvUserCreate";
    
    status = this->drvUserCreateParam(pasynUser, drvInfo, pptypeName, psize, 
                                      NDPluginColorConvertParamString, NUM_COLOR_CONVERT_PARAMS);

    /* If not, then call the base class method, see if it is known there */
    if (status) status = NDPluginDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);
    return(status);
}


/** Constructor for NDPluginColorConvert; most parameters are simply passed to NDPluginDriver::NDPluginDriver.
  * After calling the base class constructor this method sets reasonable default values for all of the 
  * ROI parameters.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] queueSize The number of NDArrays that the input queue for this plugin can hold when 
  *            NDPluginDriverBlockingCallbacks=0.  Larger queues can decrease the number of dropped arrays,
  *            at the expense of more NDArray buffers being allocated from the underlying driver's NDArrayPool.
  * \param[in] blockingCallbacks Initial setting for the NDPluginDriverBlockingCallbacks flag.
  *            0=callbacks are queued and executed by the callback thread; 1 callbacks execute in the thread
  *            of the driver doing the callbacks.
  * \param[in] NDArrayPort Name of asyn port driver for initial source of NDArray callbacks.
  * \param[in] NDArrayAddr asyn port driver address for initial source of NDArray callbacks.
  * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited number of buffers.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  */
NDPluginColorConvert::NDPluginColorConvert(const char *portName, int queueSize, int blockingCallbacks, 
                                           const char *NDArrayPort, int NDArrayAddr, 
                                           int maxBuffers, size_t maxMemory,
                                           int priority, int stackSize)
    /* Invoke the base class constructor */
    : NDPluginDriver(portName, queueSize, blockingCallbacks, 
                   NDArrayPort, NDArrayAddr, 1, NDPluginColorConvertLastParam, maxBuffers, maxMemory,
                   asynGenericPointerMask, 
                   asynGenericPointerMask,
                   0, 1, priority, stackSize)  /* Not ASYN_CANBLOCK or ASYN_MULTIDEVICE, do autoConnect */
{
    asynStatus status;
    const char *functionName = "NDPluginColorConvert";


    /* Set the plugin type string */    
    setStringParam(NDPluginDriverPluginType, "NDPluginSColorConvert");
    
    status = setIntegerParam(0, NDPluginColorConvertColorModeOut,       NDColorModeMono);
    if (!status)  printf("%s:%s: failed to set integer param for color mode\n",
                         driverName, functionName);

    /* Try to connect to the array port */
    status = connectToArrayPort();
}

extern "C" int NDColorConvertConfigure(const char *portName, int queueSize, int blockingCallbacks, 
                                          const char *NDArrayPort, int NDArrayAddr, 
                                          int maxBuffers, size_t maxMemory,
                                          int priority, int stackSize)
{  
    new NDPluginColorConvert(portName, queueSize, blockingCallbacks, NDArrayPort, NDArrayAddr, 
                             maxBuffers, maxMemory, priority, stackSize);
    return(asynSuccess);
}

/** EPICS iocsh shell commands */
static const iocshArg initArg0 = { "portName",iocshArgString};
static const iocshArg initArg1 = { "frame queue size",iocshArgInt};
static const iocshArg initArg2 = { "blocking callbacks",iocshArgInt};
static const iocshArg initArg3 = { "NDArrayPort",iocshArgString};
static const iocshArg initArg4 = { "NDArrayAddr",iocshArgInt};
static const iocshArg initArg5 = { "maxBuffers",iocshArgInt};
static const iocshArg initArg6 = { "maxMemory",iocshArgInt};
static const iocshArg initArg7 = { "priority",iocshArgInt};
static const iocshArg initArg8 = { "stackSize",iocshArgInt};
static const iocshArg * const initArgs[] = {&initArg0,
                                            &initArg1,
                                            &initArg2,
                                            &initArg3,
                                            &initArg4,
                                            &initArg5,
                                            &initArg6,
                                            &initArg7,
                                            &initArg8};
static const iocshFuncDef initFuncDef = {"NDColorConvertConfigure",9,initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
    NDColorConvertConfigure(args[0].sval, args[1].ival, args[2].ival, 
                               args[3].sval, args[4].ival, args[5].ival, 
                               args[6].ival, args[7].ival, args[8].ival);
}

extern "C" void NDColorConvertRegister(void)
{
    iocshRegister(&initFuncDef,initCallFunc);
}

extern "C" {
epicsExportRegistrar(NDColorConvertRegister);
}
