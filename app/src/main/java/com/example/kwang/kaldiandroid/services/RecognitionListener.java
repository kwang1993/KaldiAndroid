package com.example.kwang.kaldiandroid.services;

public interface RecognitionListener {
    // interface to set UI on result
    /**
     * Called when partial recognition result is available.
     */
    void onPartialResult(String hypothesis);

    /**
     * Called after silence occured.
     */
    void onResult(String hypothesis);

    /**
     * Called after stream end.
     */
    void onFinalResult(String hypothesis);

    /**
     * Called when an error occurs.
     */
    void onError(Exception exception);

    /**
     * Called after timeout expired
     */
    void onTimeout();
}
