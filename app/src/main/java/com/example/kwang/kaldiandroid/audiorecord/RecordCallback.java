package com.example.kwang.kaldiandroid.audiorecord;

public interface RecordCallback {
    void onRecord(short[] buffer, int len);
}
