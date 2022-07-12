package com.example.kwang.kaldiandroid.audiorecord;

import com.example.kwang.kaldiandroid.R;
import com.example.kwang.kaldiandroid.services.RecognitionListener;
import com.example.kwang.kaldiandroid.services.SpeechService;
import com.example.kwang.kaldiandroid.util.KaldiUtil;
import com.example.kwang.kaldiandroid.util.Model;
import com.example.kwang.kaldiandroid.util.Recognizer;

public class KaldiRecordCallback implements RecordCallback {

    private Recognizer recognizer;

    public KaldiRecordCallback(String modelPath) {
        Model model = new Model(modelPath);
        this.recognizer = new Recognizer(model);
    }

    public void onRecord(short[] buffer, int len) {
        //KaldiUtil.startRecognition(buffer, len);
        this.recognizer.acceptWaveForm(buffer, len);
    }
}
