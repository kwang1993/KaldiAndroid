
/* DO NOT EDIT THIS FILE - it is machine generated */
// 引用Android准备好的
#include <cstring>
#include <jni.h>
#include <cinttypes>
#include <android/log.h>

// 这里引用kaldi及自己写的
#include "model.h"
#include "recognizer.h" // 这里可以直接include自己需要的h。


#define LOG_TAG "System.out"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__) // 调用Android的log。

/* Header for class com_example_kwang_kaldiandroid_util_KaldiUtil */

#ifndef _Included_com_example_kwang_kaldiandroid_util_KaldiUtil
#define _Included_com_example_kwang_kaldiandroid_util_KaldiUtil
#ifdef __cplusplus
extern "C" {
#endif
/* 这个是原h文件声明的函数，这里直接修改为实现函数。
 * Class:     com_example_kwang_kaldiandroid_util_KaldiUtil
 * Method:    KaldiMathLogAdd
 * Signature: (DD)D
 */
//JNIEXPORT jdouble JNICALL Java_com_example_kwang_kaldiandroid_util_KaldiUtil_KaldiMathLogAdd
//  (JNIEnv *jniEnv, jclass, jdouble x, jdouble y) {
//
//    LOGE("进入JNI的范畴了", "");
//    LOGE("调用kaldi的函数", "");
//
//    double v = kaldi::LogAdd(x, y); // 调用 base/kaldi-math.h 中的函数
//
//    LOGE("kaldi计算结束，返回java", "");
//
//    return v;
//
//  };

jboolean bRecording = false;

//JNIEXPORT void JNICALL Java_com_example_kwang_kaldiandroid_util_KaldiUtil_startEngine
//        (JNIEnv *jniEnv, jclass, jstring model_path) {
//    Model* model = new Model(jniEnv->GetStringUTFChars(model_path, 0));
//    Recognizer* recognizer = new Recognizer(model);
//    LOGE("Start engine", "");
//
//};

JNIEXPORT void JNICALL Java_com_example_kwang_kaldiandroid_util_KaldiUtil_startEngine
        (JNIEnv *jniEnv, jclass) {

    LOGE("Start engine", "");

};

JNIEXPORT void JNICALL Java_com_example_kwang_kaldiandroid_util_KaldiUtil_stopEngine
        (JNIEnv *jniEnv, jclass) {


    LOGE("Stop engine", "");

};

JNIEXPORT void JNICALL Java_com_example_kwang_kaldiandroid_util_KaldiUtil_startRecognition
        (JNIEnv *jniEnv, jclass) {

    bRecording = true;
    LOGE("Start recognition", "");

};

JNIEXPORT void JNICALL Java_com_example_kwang_kaldiandroid_util_KaldiUtil_stopRecognition
        (JNIEnv *jniEnv, jclass) {

    bRecording = false;
    LOGE("Stop recognition", "");

};

JNIEXPORT jstring JNICALL Java_com_example_kwang_kaldiandroid_util_KaldiUtil_getResultString
        (JNIEnv *jniEnv, jclass) {


    LOGE("Get result string", "");
    jstring s  = jniEnv->NewStringUTF(""); // 不能强转
    if (bRecording) {
        const char *str = "processing\n";
        s = jniEnv->NewStringUTF(str);
    }
    return s;
};

#ifdef __cplusplus
}
#endif
#endif