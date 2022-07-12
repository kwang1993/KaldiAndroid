package com.example.kwang.kaldiandroid.util;

import com.sun.jna.PointerType;

public class Recognizer extends PointerType implements AutoCloseable {

    public Recognizer(Model model) {
        super(KaldiUtil.kwang_recognizer_new(model));
    }

    public boolean acceptWaveForm(byte[] data, int len) {
        return KaldiUtil.kwang_recognizer_accept_waveform(this.getPointer(), data, len);
    }

    public boolean acceptWaveForm(short[] data, int len) {
        return KaldiUtil.kwang_recognizer_accept_waveform_s(this.getPointer(), data, len);
    }

    public String getResult() {
        return KaldiUtil.kwang_recognizer_result(this.getPointer());
    }

    public String getPartialResult() {
        return KaldiUtil.kwang_recognizer_partial_result(this.getPointer());
    }

    public String getFinalResult() {
        return KaldiUtil.kwang_recognizer_final_result(this.getPointer());
    }

    public void reset() {
        KaldiUtil.kwang_recognizer_reset(this.getPointer());
    }

    @Override
    public void close() throws Exception {
        KaldiUtil.kwang_recognizer_free(this.getPointer());
    }
}
