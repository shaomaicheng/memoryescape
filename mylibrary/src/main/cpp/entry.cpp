#include <jni.h>
#include "android/log.h"
#include "utils.h"

extern "C"
JNIEXPORT void JNICALL
Java_com_example_mylibrary_MemoryEscapeInit_init(JNIEnv *env, jclass clazz) {
   __android_log_print(logLevel, TAG, "内存逃逸init");
}