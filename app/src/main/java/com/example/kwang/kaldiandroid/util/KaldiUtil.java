package com.example.kwang.kaldiandroid.util;


public class KaldiUtil {
    static {
        System.loadLibrary("KaldiUtil"); // 加载cmake生成的库。这个库名对应CMakeLists.txt中的配置
    }
    // public static native double KaldiMathLogAdd(double x, double y);  // 本地方法
    public static native void startEngine();
    public static native void stopEngine();
    public static native void startRecognition();
    public static native void stopRecognition();
    public static native String getResultString();
}