package com.example.kwang.kaldiandroid.util;


import com.sun.jna.Native;
import com.sun.jna.Pointer;

public class KaldiUtil {
    static {
        //System.loadLibrary("KaldiUtil"); // 加载cmake生成的库。这个库名对应CMakeLists.txt中的配置
        Native.register(KaldiUtil.class, "kwang"); // JNA instead of JNI
    }
    // public static native double KaldiMathLogAdd(double x, double y);  // 本地方法
//    public static native void startEngine(String modelPath);
//    public static native void stopEngine();
//    public static native void startRecognition(short[] data, int length);
//    public static native void stopRecognition();
//    public static native String getResultString();
    public static native Pointer kwang_model_new(String path);
    public static native void kwang_model_free(Pointer model);
    public static native Pointer kwang_recognizer_new(Model model);
    public static native void kwang_recognizer_free(Pointer recognizer);
    public static native void kwang_recognizer_reset(Pointer recognizer);
    public static native boolean kwang_recognizer_accept_waveform(Pointer recognizer, byte[] data, int len);
    public static native boolean kwang_recognizer_accept_waveform_s(Pointer recognizer, short[] data, int len);
    public static native String kwang_recognizer_result(Pointer recognizer);
    public static native String kwang_recognizer_final_result(Pointer recognizer);
    public static native String kwang_recognizer_partial_result(Pointer recognizer);
}