package com.example.kwang.kaldiandroid.services;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Handler;
import android.os.Looper;

import com.example.kwang.kaldiandroid.audiorecord.AudioRecordManager;
import com.example.kwang.kaldiandroid.util.Recognizer;

import java.io.IOException;

public class SpeechService {
    private final Recognizer recognizer;
    private final int sampleRate;
    private final static float BUFFER_SIZE_SECONDS = 0.2f;
    private final int bufferSize;
    private final AudioRecord recorder;

    private RecognizerThread recognizerThread;

    private final Handler mainHandler = new Handler(Looper.getMainLooper());


    public SpeechService(Recognizer recognizer, int sampleRate) throws IOException {
        this.recognizer = recognizer;
        this.sampleRate = (int) sampleRate;
        bufferSize = Math.round (this.sampleRate * BUFFER_SIZE_SECONDS);
        recorder = new AudioRecord(
                MediaRecorder.AudioSource.VOICE_RECOGNITION,
                this.sampleRate,
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT,
                bufferSize * 2
        );
        if (recorder.getState() == AudioRecord.STATE_UNINITIALIZED) {
            recorder.release();
            throw new IOException(
                    "Failed to initialize recorder. Microphone might be already in use.");
        }
    }

    public SpeechService(Recognizer recognizer) throws IOException {
        this(recognizer, 16000);
    }

    public boolean startListening(RecognitionListener listener) {
        if (null != recognizerThread) {
            return false; // 0 means error
        }
        recognizerThread = new RecognizerThread(listener);
        recognizerThread.start();
        return true;
    }

    private boolean stop() {
        return stopRecognizerThread();
    }
    public boolean cancel() {
        if (recognizerThread != null) {
            recognizerThread.setPause(true);
        }
        return stopRecognizerThread();
    }
    public void shutdown() {
        recorder.release();
    }
    public void setPause(boolean paused) {
        if (recognizerThread != null) {
            recognizerThread.setPause(paused);
        }
    }
    public void reset() {
        if (recognizerThread != null) {
            recognizerThread.reset();
        }
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
        private volatile boolean paused = false;
        private volatile boolean reset = false;

        RecognitionListener listener;

        public RecognizerThread(RecognitionListener listener) {
            this(listener, NO_TIMEOUT);
        }

        public RecognizerThread(RecognitionListener listener, int timeout) {
            this.listener = listener;
            if (timeout != NO_TIMEOUT) {
                this.timeoutSamples = timeout * sampleRate / 1000;
            } else {
                this.timeoutSamples = NO_TIMEOUT;
            }
            this.remainingSamples = this.timeoutSamples;
        }

        public void setPause(boolean paused) {
            this.paused = paused;
        }

        public void reset() {
            this.reset = true;
        }

        @Override
        public void run() {
            recorder.startRecording();
            if (recorder.getRecordingState() == AudioRecord.RECORDSTATE_STOPPED) {
                recorder.stop();
                IOException ioe = new IOException(
                        "Failed to start recording. Microphone might be already in use.");
                mainHandler.post(() -> listener.onError(ioe));
            }
            short[] buffer = new short[bufferSize];

            while (!interrupted()
                    && ((timeoutSamples == NO_TIMEOUT) || (remainingSamples > 0))) {
                int nread = recorder.read(buffer, 0, buffer.length);

                if (paused) {
                    continue;
                }

                if (reset) {
                    recognizer.reset();
                    reset = false;
                }
                if (nread < 0)
                    throw new RuntimeException("error reading audio buffer");

                if (recognizer.acceptWaveForm(buffer, nread)) {
                    final String result = recognizer.getResult();
                    mainHandler.post(() -> listener.onResult(result));
                } else {
                    final String partialResult = recognizer.getPartialResult();
                    mainHandler.post(() -> listener.onPartialResult(partialResult));
                }

                if (timeoutSamples != NO_TIMEOUT) {
                    remainingSamples = remainingSamples - nread;
                }

            }
            recorder.stop();

            if (!paused) {
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

}
