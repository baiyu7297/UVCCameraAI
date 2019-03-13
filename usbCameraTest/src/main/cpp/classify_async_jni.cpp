#include <jni.h>
#include <string>

#include <memory.h>
#include "HIAIModelManager.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <stdlib.h>
#include <cmath>
#include <sstream>
#include "common.h"

#include <unistd.h>

#define LOG_TAG "ASYNC_DDK_MSG"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

using namespace std;

static HIAI_ModelManager *modelManager = NULL;

static HIAI_TensorBuffer *inputtensor = NULL;

static HIAI_TensorBuffer *outputtensor = NULL;

static HIAI_ModelBuffer *modelBuffer = NULL;

static HIAI_ModelManagerListener listener;
static jclass callbacksClass;
static jobject callbacksInstance;
JavaVM *jvm;

float time_use;
struct timeval tpstart, tpend;
 int async_input_N = 0;
 int async_input_C = 0;
 int async_input_H = 0;
 int async_input_W = 0;
 int async_output_N = 0;
 int async_output_C = 0;
 int async_output_H = 0;
 int async_output_W = 0;
const char* modelnameForAsync;
jstring jmodelnameForAsync;
//get Input and Output N C H W from model  after loading success the model
void getInputAndOutputFromModelForAsync(const char *modelName){
    if(modelManager == NULL || modelName == NULL){
        LOGE("modelManager or modelName is NULL !!");
    }
    HIAI_ModelTensorInfo* modelTensorInfo = HIAI_ModelManager_getModelTensorInfo(modelManager, modelName);
    if (modelTensorInfo == NULL){
        LOGE("HIAI_ModelManager_getModelTensorInfo failed!!");
        return ;
    }

    /**
     * if your model have muli-input and muli-output
     * you can get N C H W from model like as below:
     *
     for (int i = 0; i < modelTensorInfo->input_cnt; ++i)
    {
        LOGI("input[%u] N: %u-C: %u-H: %u-W: %u\n", i, modelTensorInfo->input_shape[i*4], modelTensorInfo->input_shape[i*4 + 1],
               modelTensorInfo->input_shape[i*4 + 2], modelTensorInfo->input_shape[i*4 + 3]);


        HIAI_TensorBuffer* input = HIAI_TensorBuffer_create(modelTensorInfo->input_shape[i*4], modelTensorInfo->input_shape[i*4 + 1],
                                                            modelTensorInfo->input_shape[i*4 + 2], modelTensorInfo->input_shape[i*4 + 3]);
     }
     */
    //get N C H W from model, The case use 1 input and 1 output ,So we take a simplified approach here
    LOGI("input N: %u-C: %u-H: %u-W: %u\n",  modelTensorInfo->input_shape[0], modelTensorInfo->input_shape[1],
         modelTensorInfo->input_shape[2], modelTensorInfo->input_shape[3]);
    async_input_N = modelTensorInfo->input_shape[0];
    async_input_C = modelTensorInfo->input_shape[1];
    async_input_H = modelTensorInfo->input_shape[2];
    async_input_W = modelTensorInfo->input_shape[3];

    LOGI("output N: %u-C: %u-H: %u-W: %u\n",  modelTensorInfo->output_shape[0], modelTensorInfo->output_shape[1],
         modelTensorInfo->output_shape[2], modelTensorInfo->output_shape[3]);
    async_output_N = modelTensorInfo->output_shape[0];
    async_output_C = modelTensorInfo->output_shape[1];
    async_output_H = modelTensorInfo->output_shape[2];
    async_output_W = modelTensorInfo->output_shape[3];

    if(modelTensorInfo != NULL){
        HIAI_ModelManager_releaseModelTensorInfo(modelTensorInfo);
        modelTensorInfo = NULL;
    }
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {

    LOGE("AYSNC JNI layer JNI_OnLoad");

    jvm = vm;

    return JNI_VERSION_1_6;
}


void onLoadDone(void *userdata, int taskId) {

    LOGE("AYSNC JNI layer onLoadDone: %d", taskId);
    if(taskId > 0){
        LOGI("modelnameForAsync is %s",modelnameForAsync);
        getInputAndOutputFromModelForAsync(modelnameForAsync);
    }

    JNIEnv *env;

    jvm->AttachCurrentThread(&env, NULL);

    if (callbacksInstance != NULL) {
        jmethodID onValueReceived = env->GetMethodID(callbacksClass, "onStartDone", "(I)V");
        env->CallVoidMethod(callbacksInstance, onValueReceived, taskId);
    }

}

void onRunDone(void *userdata, int taskStamp) {
    gettimeofday(&tpend, NULL);
    time_use = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;

    LOGI("AYSNC infrence time %f ms.", time_use / 1000);
    LOGE("AYSNC JNI layer onRunDone taskStamp: %d", taskStamp);

    JNIEnv *env;

    jvm->AttachCurrentThread(&env, NULL);

    float *outputBuffer = (float *) listener.userdata;

    int outputSize = 1001;

    float *scale_data = (float *) malloc(outputSize * sizeof(float));

    double max = outputBuffer[0];
    double sum = 0;
    for (int i = 0; i < 1001; i++) {
        if (outputBuffer[i] > max)
            max = outputBuffer[i];
    }

    for (int i = 0; i < 1001; i++) {
        scale_data[i] = exp(outputBuffer[i] - max);
        sum += scale_data[i];
    }

    int max_index[3] = {0};
    double max_num[3] = {0};

    for (int i = 0; i < 1001; i++) {
        outputBuffer[i] = scale_data[i] / sum;
        double tmp = outputBuffer[i];
        int tmp_index = i;
        for (int j = 0; j < 3; j++) {
            if (tmp > max_num[j]) {
                tmp_index += max_index[j];
                max_index[j] = tmp_index - max_index[j];
                tmp_index -= max_index[j];
                tmp += max_num[j];
                max_num[j] = tmp - max_num[j];
                tmp -= max_num[j];
            }
        }
    }


    /* 是否有更简单的方法？ */
    jobjectArray result;

    ostringstream stringStream;

    stringStream << clabels[max_index[0]].c_str() << " - " << max_num[0] * 100 << "%\n";

    stringStream.flush();
    const char *top1Result = strdup(stringStream.str().c_str());

    stringStream.str("");
    stringStream.clear();


    stringStream << clabels[max_index[1]].c_str() << " - " << max_num[1] * 100
                 << "%\n" << clabels[max_index[2]].c_str() << " - " << max_num[2] * 100
                 << "%\n";

    stringStream.flush();
    const char *otherResults = strdup(stringStream.str().c_str());

    stringStream.str("");
    stringStream.clear();

    stringStream << "inference time:" << time_use / 1000 << "ms\n";

    stringStream.flush();
    const char *inferenceTime = strdup(stringStream.str().c_str());

    stringStream.str("");
    stringStream.clear();

    result = (jobjectArray) env->NewObjectArray(3, env->FindClass("java/lang/String"),
                                                env->NewStringUTF(""));

    env->SetObjectArrayElement(result, 0, env->NewStringUTF(top1Result));
    env->SetObjectArrayElement(result, 1, env->NewStringUTF(otherResults));
    env->SetObjectArrayElement(result, 2, env->NewStringUTF(inferenceTime));


    if (callbacksInstance != NULL) {

        jmethodID onValueReceived = env->GetMethodID(callbacksClass, "onRunDone",
                                                     "(I[Ljava/lang/String;)V");

        env->CallVoidMethod(callbacksInstance, onValueReceived, taskStamp, result);
    }

    free(scale_data);

    free((void *) top1Result);
    free((void *) otherResults);
    free((void *) inferenceTime);

    if (inputtensor != NULL) {
        HIAI_TensorBuffer_destroy(inputtensor);
        inputtensor = NULL;
    }

    if (outputtensor != NULL) {
        HIAI_TensorBuffer_destroy(outputtensor);
        outputtensor = NULL;
    }
}


void onUnloadDone(void *userdata, int taskStamp) {
    LOGE("JNI layer onUnloadDone: %d", taskStamp);

    JNIEnv *env;

    jvm->AttachCurrentThread(&env, NULL);

    if (callbacksInstance != NULL) {
        jmethodID onValueReceived = env->GetMethodID(callbacksClass, "onStopDone", "(I)V");
        env->CallVoidMethod(callbacksInstance, onValueReceived, taskStamp);
    }


    HIAI_ModelManager_destroy(modelManager);

    modelManager = NULL;

    listener.onRunDone = NULL;
    listener.onUnloadDone = NULL;
    listener.onTimeout = NULL;
    listener.onServiceDied = NULL;
    listener.onError = NULL;
    listener.onLoadDone = NULL;
}


void onTimeout(void *userdata, int taskStamp) {
    LOGE("JNI layer onTimeout: %d", taskStamp);

    JNIEnv *env;

    jvm->AttachCurrentThread(&env, NULL);

    if (callbacksInstance != NULL) {
        jmethodID onValueReceived = env->GetMethodID(callbacksClass, "onTimeout", "(I)V");
        env->CallVoidMethod(callbacksInstance, onValueReceived, taskStamp);
    }
}

void onError(void *userdata, int taskStamp, int errCode) {
    LOGE("JNI layer onError: %d", taskStamp);

    JNIEnv *env;

    jvm->AttachCurrentThread(&env, NULL);

    if (callbacksInstance != NULL) {
        jmethodID onValueReceived = env->GetMethodID(callbacksClass, "onError", "(II)V");
        env->CallVoidMethod(callbacksInstance, onValueReceived, taskStamp, errCode);
    }
}

void onServiceDied(void *userdata) {
    LOGE("JNI layer onServiceDied:");

    JNIEnv *env;

    jvm->AttachCurrentThread(&env, NULL);

    if (callbacksInstance != NULL) {
        jmethodID onValueReceived = env->GetMethodID(callbacksClass, "onServiceDied", "()V");
        env->CallVoidMethod(callbacksInstance, onValueReceived);
    }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_huawei_hiaidemo_ModelManager_registerListenerJNI(JNIEnv *env, jobject obj,
                                                          jobject callbacks) {

    callbacksInstance = env->NewGlobalRef(callbacks);
    jclass objClass = env->GetObjectClass(callbacks);
    if (objClass) {
        callbacksClass = reinterpret_cast<jclass>(env->NewGlobalRef(objClass));
        env->DeleteLocalRef(objClass);
    }

    listener.onLoadDone = onLoadDone;
    listener.onRunDone = onRunDone;
    listener.onUnloadDone = onUnloadDone;
    listener.onTimeout = onTimeout;
    listener.onError = onError;
    listener.onServiceDied = onServiceDied;

    modelManager = HIAI_ModelManager_create(&listener);
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_huawei_hiaidemo_ModelManager_loadModelAsync(JNIEnv *env, jobject instance,
                                                     jstring jmodelName, jobject assetManager) {
    const char *modelName = env->GetStringUTFChars(jmodelName, 0);
    jmodelnameForAsync = jmodelName;
    modelnameForAsync = env->GetStringUTFChars(jmodelName, 0);
    char modelname[128] = {0};

    strcat(modelname, modelName);
    strcat(modelname, ".cambricon");

    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
    LOGI("Attempting to load model...\n");

    LOGE("model name is %s", modelname);

    AAsset *asset = AAssetManager_open(mgr, modelname, AASSET_MODE_BUFFER);

    if (nullptr == asset) {
        LOGE("AAsset is null...\n");
    }

    const void *data = AAsset_getBuffer(asset);

    if (nullptr == data) {
        LOGE("model buffer is null...\n");
    }

    off_t len = AAsset_getLength(asset);

    if (0 == len) {
        LOGE("model buffer length is 0...\n");
    }


    HIAI_ModelBuffer *modelBuffer = HIAI_ModelBuffer_create_from_buffer(modelName,
                                                                        (void *) data, len,
                                                                        HIAI_DevPerf::HIAI_DEVPREF_HIGH);
    HIAI_ModelBuffer *modelBufferArray[] = {modelBuffer};

    int ret = HIAI_ModelManager_loadFromModelBuffers(modelManager, modelBufferArray, 1);

    LOGE("ASYNC JNI LAYER load model from assets ret = %d", ret);
    env->ReleaseStringUTFChars(jmodelName, modelName);

    AAsset_close(asset);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_huawei_hiaidemo_ModelManager_runModelAsync(JNIEnv *env, jobject instance,
                                                    jstring jmodelName, jfloatArray jbuf) {
    const char *modelName = env->GetStringUTFChars(jmodelName, 0);

    if (NULL == modelManager) {
        LOGE("please load model first");

        return;
    }

    float *dataBuff = NULL;

    if (NULL != jbuf) {
        dataBuff = env->GetFloatArrayElements(jbuf, NULL);
    }

    inputtensor = HIAI_TensorBuffer_create(async_input_N, async_input_C, async_input_H, async_input_W);

    HIAI_TensorBuffer *inputtensorbuffer[] = {inputtensor};

    outputtensor = HIAI_TensorBuffer_create(async_output_N, async_output_C, async_output_H, async_output_W);

    HIAI_TensorBuffer *outputtensorbuffer[] = {outputtensor};


    float *inputbuffer = (float *) HIAI_TensorBuffer_getRawBuffer(inputtensor);

    int length = HIAI_TensorBuffer_getBufferSize(inputtensor);

    memcpy(inputbuffer, dataBuff, length);


    gettimeofday(&tpstart, NULL);

    int ret = HIAI_ModelManager_runModel(
            modelManager,
            inputtensorbuffer,
            1,
            outputtensorbuffer,
            1,
            1000,
            modelName);

    LOGE("ASYNC JNI layer runmodel ret: %d", ret);

    float *outputdata = (float *) HIAI_TensorBuffer_getRawBuffer(outputtensor);

    listener.userdata = NULL;

    listener.userdata = outputdata;

    env->ReleaseStringUTFChars(jmodelName, modelName);
    env->ReleaseFloatArrayElements(jbuf, dataBuff, 0);

}

extern "C"
JNIEXPORT void JNICALL
Java_com_huawei_hiaidemo_ModelManager_unloadModelAsync(JNIEnv *env, jobject instance) {
    if (NULL == modelManager) {
        LOGE("please load model first");

        return;
    } else {
        if (modelBuffer != NULL) {
            HIAI_ModelBuffer_destroy(modelBuffer);
            modelBuffer = NULL;
        }

        int ret = HIAI_ModelManager_unloadModel(modelManager);
        env->ReleaseStringUTFChars(jmodelnameForAsync,modelnameForAsync);
        LOGE("ASYNC JNI layer unLoadModel ret:%d", ret);
    }
}

