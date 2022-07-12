package com.example.kwang.kaldiandroid.services;

import android.os.Handler;
import android.os.Looper;

import com.example.kwang.kaldiandroid.util.Recognizer;

import java.io.IOException;
import java.io.InputStream;

public class SpeechStreamService {
    private final Recognizer recognizer;
    private final InputStream inputStream;
    private final int sampleRate;
    private final static float BUFFER_SIZE_SECONDS = 0.2f;
    private final int bufferSize;

    private Thread recognizerThread;

    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    public SpeechStreamService(Recognizer recognizer, InputStream inputStream) {
        this(recognizer, inputStream, 16000);
    }

    public SpeechStreamService(Recognizer recognizer, InputStream inputStream, int sampleRate) {
        this.recognizer = recognizer;
        this.sampleRate = sampleRate;
        this.inputStream = inputStream;
        bufferSize = Math.round(this.sampleRate * BUFFER_SIZE_SECONDS * 2);
    }

    public boolean startListening(RecognitionListener listener) {
        if (null != recognizerThread)
            return false;

        recognizerThread = new RecognizerThread(listener);
        recognizerThread.start();
        return true;
    }

    public boolean stopListening() {
        return stopRecognizerThread();
    }

    private boolean stopRecognizerThread() {
        if (null == recognizerThread)
            return false;

        try {
            recognizerThread.interrupt();
            recognizerThread.join();
        } catch (InterruptedException e) {
            // Restore the interrupted status.
            Thread.currentThread().interrupt();
        }

        recognizerThread = null;
        return true;
    }

    private final class RecognizerThread extends Thread {

        private int remainingSamples;
        private final int timeoutSamples;
        private final static int NO_TIMEOUT = -1;
        RecognitionListener listener;

        public RecognizerThread(RecognitionListener listener, int timeout) {
            this.listener = listener;
            if (timeout != NO_TIMEOUT)
                this.timeoutSamples = timeout * sampleRate / 1000;
            else
                this.timeoutSamples = NO_TIMEOUT;
            this.remainingSamples = this.timeoutSamples;
        }

        public RecognizerThread(RecognitionListener listener) {
            this(listener, NO_TIMEOUT);
        }

        @Override
        public void run() {

            byte[] buffer = new byte[bufferSize];

            while (!interrupted()
                    && ((timeoutSamples == NO_TIMEOUT) || (remainingSamples > 0))) {
                try {
                    int nread = inputStream.read(buffer, 0, buffer.length);
                    if (nread < 0) {
                        break;
                    } else {
                        boolean isSilence = recognizer.acceptWaveForm(buffer, nread);
                        if (isSilence) {
                            final String result = recognizer.getResult();
                            mainHandler.post(() -> listener.onResult(result));
                        } else {
                            final String partialResult = recognizer.getPartialResult();
                            mainHandler.post(() -> listener.onPartialResult(partialResult));
                        }
                    }

                    if (timeoutSamples != NO_TIMEOUT) {
                        remainingSamples = remainingSamples - nread;
                    }

                } catch (IOException e) {
                    mainHandler.post(() -> listener.onError(e));
                }
            }

            // If we met timeout signal that speech ended
            if (timeoutSamples != NO_TIMEOUT && remainingSamples <= 0) {
                mainHandler.post(() -> listener.onTimeout());
            } else {
                final String finalResult = recognizer.getFinalResult();
                mainHandler.post(() -> listener.onFinalResult(finalResult));
            }
        }
    }
}
