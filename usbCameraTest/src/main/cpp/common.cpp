#include <jni.h>
#include <string>
#include <vector>
#include "common.h"
#include "HIAIModelManager.h"

std::vector<std::string> clabels;

std::vector<std::string> split_string(const std::string &str, const std::string &delimiter) {
    std::vector<std::string> strings;

    std::string::size_type pos = 0;
    std::string::size_type prev = 0;
    while ((pos = str.find(delimiter, prev)) != std::string::npos) {
        strings.push_back(str.substr(prev, pos - prev));
        prev = pos + 1;
    }
    strings.push_back(str.substr(prev));
    return strings;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_huawei_hiaidemo_ModelManager_initLabels(JNIEnv *env, jclass type, jbyteArray jlabels) {
    int len = env->GetArrayLength(jlabels);
    std::string words_buffer;
    words_buffer.resize(len);
    env->GetByteArrayRegion(jlabels, 0, len, (jbyte *) words_buffer.data());
    clabels = split_string(words_buffer, "\n");
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_huawei_hiaidemo_ModelManager_getHiAiVersion(JNIEnv *env, jobject instance) {

    char* versionName;
    jstring rtstr ;

    try{
     versionName = HIAI_GetVersion();
     rtstr = env->NewStringUTF(versionName);
    }catch(...){
        rtstr = env->NewStringUTF("000.000.000.000");
    }
    return rtstr;
}