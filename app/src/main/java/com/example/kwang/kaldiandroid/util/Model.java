package com.example.kwang.kaldiandroid.util;


import com.sun.jna.PointerType;

public class Model extends PointerType implements AutoCloseable {

    public Model() {
    }

    public Model(String path) {
        super(KaldiUtil.kwang_model_new(path));
    }

    @Override
    public void close() {
        KaldiUtil.kwang_model_free(this.getPointer());
    }
}
