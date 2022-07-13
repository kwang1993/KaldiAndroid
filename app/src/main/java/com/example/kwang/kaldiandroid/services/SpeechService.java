package com.example.kwang.kaldiandroid.services;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.os.Handler;
import android.os.Looper;

import com.example.kwang.kaldiandroid.util.Recognizer;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

public class SpeechService {
    private final Recognizer recognizer;
    private final int sampleRate;
    private final int bufferSize;
    private final AudioRecord recorder;
    private final static int DEFAULT_SAMPLE_RATE = 16000;

    private RecognizerThread recognizerThread;

    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    // init recorder
    public SpeechService(Recognizer recognizer, int sampleRate) throws IOException {
        this.recognizer = recognizer;
        this.sampleRate = sampleRate;
        bufferSize = AudioRecord.getMinBufferSize(
                this.sampleRate,
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT);
        recorder = new AudioRecord(
                MediaRecorder.AudioSource.MIC,
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
        this(recognizer, DEFAULT_SAMPLE_RATE);
    }

    public boolean startListening(RecognitionListener listener) {
        return startListening(listener, null);
    }

    public boolean startListening(RecognitionListener listener, String filePath) {
        if (recognizerThread != null) {
            return false; // in use
        }
        recognizerThread = new RecognizerThread(listener, filePath);
        recognizerThread.start();
        return true;
    }
    public boolean stopListening() {
        return stopRecognizerThread();
    }

    // release recorder
    public void release() {
        recorder.release();
    }
    public void setPause(boolean paused) {
        if (recognizerThread != null) {
            recognizerThread.setPause(paused);
        }
    }

    public boolean cancel() {
        if (recognizerThread != null) {
            recognizerThread.setPause(true); // Skip acceptWaveform and getResult
        }
        return stopRecognizerThread();
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
        recognizerThread = null; // necessary to release hold of file
        return true;
    }

    public void replay(String filePath) {
        File file = new File(filePath);
        if (file.exists()) {
            int musicLength = (int) (file.length() / 2);
            short[] music = new short[musicLength];

            try {
                DataInputStream dis = new DataInputStream(new BufferedInputStream(new FileInputStream(file)));
                int i = 0;
                while (true) {
                    if (!(dis.available() > 0)) break;
                    music[i] = dis.readShort();
                    i++;
                }
                dis.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
            AudioTrack audioTrack = new AudioTrack(
                    AudioManager.STREAM_MUSIC,
                    DEFAULT_SAMPLE_RATE,
                    AudioFormat.CHANNEL_OUT_MONO,
                    AudioFormat.ENCODING_PCM_16BIT,
                    musicLength * 2,
                    AudioTrack.MODE_STREAM
            );
            audioTrack.play();
            audioTrack.write(music, 0, musicLength);
            audioTrack.stop();
        }
    }

    private final class RecognizerThread extends Thread implements RecordingListener {
        private int remainingSamples;
        private final int timeoutSamples;
        private final static int NO_TIMEOUT = -1;
        private volatile boolean paused = false;
        private volatile boolean reset = false;
        private final String filePath;
        RecognitionListener listener;
        private DataOutputStream dos = null;
        private boolean bStoreFile;

        public RecognizerThread(RecognitionListener listener) {
            this(listener, NO_TIMEOUT, null);
        }

        public RecognizerThread(RecognitionListener listener, String filePath) {
            this(listener, NO_TIMEOUT, filePath);
        }
        public RecognizerThread(RecognitionListener listener, int timeout, String filePath) {
            this.listener = listener;
            if (timeout != NO_TIMEOUT) {
                this.timeoutSamples = timeout * sampleRate / 1000;
            } else {
                this.timeoutSamples = NO_TIMEOUT;
            }
            this.remainingSamples = this.timeoutSamples;

            this.filePath = filePath;

            this.bStoreFile = filePath != null && !filePath.isEmpty();
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

                if (bStoreFile && nread > 0) {
                    this.onRecord(buffer, nread);
                }

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

            if (bStoreFile) {
                this.onRecord(buffer, -1);
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

        @Override
        public void onRecord(short[] buffer, int len) {
            if (bStoreFile) {
                if (dos == null) {
                    File f = new File(filePath);
                    if (f.exists()) {
                        f.delete();
                    }
                    try {
                        dos = new DataOutputStream(new BufferedOutputStream(new FileOutputStream(f))) ;
                        //os.write(getWaveFileHeader(0, params.getSampleRate(), channelCount, bits));
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
                if (len > 0) {
                    try {
                        for (int i =0 ;i < len; ++i) {
                            dos.writeShort(buffer[i]);
                        }
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                } else {
                    try {
//                        int fileLen = (int)file.length() - 44; // minus header len
//
//                        file.seek(0);
//                        file.write(getWaveFileHeader(fileLen, params.getSampleRate(), channelCount, bits));
                        dos.close();
                        dos = null;
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }

        }
    }

}
