#ifndef __HIAI_MODEL_MANAGER_H__
#define __HIAI_MODEL_MANAGER_H__

#include <stdbool.h>

/**
This is the HIAI ModelManager C API:
*/
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HIAI_MODELTYPE_ONLINE = 0,
    HIAI_MODELTYPE_OFFLINE
} HIAI_ModelType;

typedef enum {
    HIAI_FRAMEWORK_NONE = 0,
    HIAI_FRAMEWORK_TENSORFLOW,
    HIAI_FRAMEWORK_KALDI,
    HIAI_FRAMEWORK_CAFFE,
    HIAI_FRAMEWORK_INVALID,
} HIAI_Framework;

typedef enum {
    HIAI_DEVPERF_UNSET = 0,
    HIAI_DEVPREF_LOW,
    HIAI_DEVPREF_NORMAL,
    HIAI_DEVPREF_HIGH,
} HIAI_DevPerf;

typedef enum {
    HIAI_DATATYPE_UINT8 = 0,
    HIAI_DATATYPE_FLOAT32 = 1,
} HIAI_DataType;

#define HIAI_DEVPERF_DEFAULT (HIAI_DEVPREF_HIGH)

typedef struct {
    const char* modelNetName;

    const char* modelNetPath;
    bool isModelNetEncrypted;
    const char* modelNetKey;

    const char* modelNetParamPath;
    bool isModelNetParamEncrypted;
    const char* modelNetParamKey;

    HIAI_ModelType modelType;
    HIAI_Framework frameworkType;
    HIAI_DevPerf   perf;
} HIAI_ModelDescription;

#define HIAI_MODEL_DESCRIPTION_INIT { \
    "", \
    "", false, "", \
    "", false, "", \
    HIAI_MODELTYPE_OFFLINE, \
    HIAI_FRAMEWORK_CAFFE, \
    HIAI_DEVPERF_DEFAULT \
}

typedef struct {
    int number;
    int channel;
    int height;
    int width;
    HIAI_DataType dataType;  /* optional */
} HIAI_TensorDescription;

#define HIAI_TENSOR_DESCRIPTION_INIT {0, 0, 0, 0, HIAI_DATATYPE_FLOAT32}

typedef struct HIAI_ModelManagerListener_struct
{
    void (*onLoadDone)(void* userdata, int taskStamp);
    void (*onRunDone)(void* userdata, int taskStamp);
    void (*onUnloadDone)(void* userdata, int taskStamp);
    void (*onTimeout)(void* userdata, int taskStamp);
    void (*onError)(void* userdata, int taskStamp, int errCode);
    void (*onServiceDied)(void* userdata);

    void* userdata;
} HIAI_ModelManagerListener;

#define HIAI_MODEL_MANAGER_LISTENER_INIT {NULL, NULL, NULL, NULL, NULL, NULL, NULL}


typedef struct HIAI_ModelBuffer HIAI_ModelBuffer;

HIAI_ModelBuffer* HIAI_ModelBuffer_create_from_file(const char* name, const char* path, HIAI_DevPerf perf);
HIAI_ModelBuffer* HIAI_ModelBuffer_create_from_buffer(const char* name, void* modelBuf, int size, HIAI_DevPerf perf);

const char* HIAI_ModelBuffer_getName(HIAI_ModelBuffer* b);
const char* HIAI_ModelBuffer_getPath(HIAI_ModelBuffer* b);
HIAI_DevPerf HIAI_ModelBuffer_getPerf(HIAI_ModelBuffer* b);

int HIAI_ModelBuffer_getSize(HIAI_ModelBuffer* b);

void HIAI_ModelBuffer_destroy(HIAI_ModelBuffer* b);



typedef struct HIAI_TensorBuffer HIAI_TensorBuffer;

HIAI_TensorBuffer* HIAI_TensorBuffer_create(int n, int c, int h, int w);

HIAI_TensorBuffer* HIAI_TensorBuffer_createFromTensorDesc(HIAI_TensorDescription* tensor);

HIAI_TensorDescription HIAI_TensorBuffer_getTensorDesc(HIAI_TensorBuffer* b);

void* HIAI_TensorBuffer_getRawBuffer(HIAI_TensorBuffer* b);

int HIAI_TensorBuffer_getBufferSize(HIAI_TensorBuffer* b);

void HIAI_TensorBuffer_destroy(HIAI_TensorBuffer* b);

typedef struct
{
    int input_cnt;
    int output_cnt;
    int *input_shape;
    int *output_shape;
} HIAI_ModelTensorInfo;


typedef struct HIAI_ModelManager HIAI_ModelManager;

HIAI_ModelTensorInfo* HIAI_ModelManager_getModelTensorInfo(HIAI_ModelManager* manager, const char* modelName);

void HIAI_ModelManager_releaseModelTensorInfo(HIAI_ModelTensorInfo* modelTensor);

HIAI_ModelManager* HIAI_ModelManager_create(HIAI_ModelManagerListener* listener);

void HIAI_ModelManager_destroy(HIAI_ModelManager* manager);

/**
 * @return >0 means load success, and the return value is a taskStamp, < 0 means error occured
**/
int HIAI_ModelManager_loadFromModelBuffers(HIAI_ModelManager* manager, HIAI_ModelBuffer* bufferArray[], int nBuffers);

int HIAI_ModelManager_loadFromModelDescriptions(HIAI_ModelManager* manager, HIAI_ModelDescription descsArray[], int nDescs);

int HIAI_ModelManager_runModel(
        HIAI_ModelManager* manager,
        HIAI_TensorBuffer* input[],
        int nInput,
        HIAI_TensorBuffer* output[],
        int nOutput,
        int ulTimeout,
        const char* modelName);

int HIAI_ModelManager_setInputsAndOutputs(
        HIAI_ModelManager* manager,
        const char* modelname,
        HIAI_TensorBuffer* input[],
        int nInput,
        HIAI_TensorBuffer* output[],
        int nOutput);

int HIAI_ModelManager_startCompute(HIAI_ModelManager* manager, const char* modelname);

int HIAI_ModelManager_unloadModel(HIAI_ModelManager* manager);
char* HIAI_GetVersion();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // __HIAI_MODEL_MANAGER_H__
