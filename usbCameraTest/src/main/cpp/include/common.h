#include <jni.h>
#include <string>
#include <vector>

#ifndef HIAIDEMO_COMMON_H
#define HIAIDEMO_COMMON_H

extern std::vector<std::string> clabels;

std::vector<std::string> split_string(const std::string &str, const std::string &delimiter);

extern "C"
JNIEXPORT void JNICALL
Java_com_huawei_hiaidemo_ModelManager_initLabels(JNIEnv *env, jclass type, jbyteArray jlabels);

#endif //HIAIDEMO_COMMON_H
