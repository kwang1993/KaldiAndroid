package com.example.kwang.kaldiandroid.audiorecord;

import android.annotation.SuppressLint;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.util.Log;

import com.example.kwang.kaldiandroid.services.RecordListener;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

public class RecordingService {
    private AudioParams DEFAULT_FORMAT = new AudioParams(16000, 1, 16);
    private AudioRecord recorder = null;
    private Thread recordThread = null;
    private boolean bRecording = false;
    private DataOutputStream dos = null;


    @SuppressLint("MissingPermission")
    private void startRecording(AudioParams params, RecordListener callback) {
        int sampleRate = params.getSampleRate();
        int channelConfig = params.getChannelConfig();
        int audioFormat = params.getEncodingFormat();

        int bufferSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat);
        if (recorder == null) {
            recorder = new AudioRecord(MediaRecorder.AudioSource.MIC, sampleRate, channelConfig, audioFormat, bufferSize);
        }

        recordThread = new Thread(() -> {
            short [] buffer = new short[bufferSize];
            recorder.startRecording();
            bRecording = true;
            while (bRecording) {
                int read = recorder.read(buffer, 0, bufferSize);
                if (read > 0 && callback != null) {
                    callback.onRecord(buffer, read);
                }
            }
            if (callback != null) {
                callback.onRecord(buffer, -1);
            }
            releaseRecorder();
        });
        recordThread.start();

        Log.e("mediaRecorder", "startRecording");

    }

    private void releaseRecorder() {
        if (recorder != null) {
            recorder.stop();
            recorder.release();
            recorder = null;
            dos = null;
        }
    }

    public void startRecording(String filePath, AudioParams params) {
        int channelCount = params.getChannelCount();
        int bits = params.getBits();
        boolean bStoreFile = filePath != null && !filePath.isEmpty();

        startRecording(params, (buffer, len)-> {

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
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }

        });
    }
    public void startRecording(String filePath) {
        startRecording(filePath, DEFAULT_FORMAT);
    }

    public void stopRecording() {
        bRecording = false;
    }

    private void replay(String filePath, AudioParams params) {
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
            AudioTrack audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC,
                    params.getSampleRate(), params.getChannelConfig(true),
                    params.getEncodingFormat(),
                    musicLength * 2,
                    AudioTrack.MODE_STREAM
                    );
            audioTrack.play();
            audioTrack.write(music, 0, musicLength);
            audioTrack.stop();
        }
    }

    public void replay(String filePath) {
        replay(filePath, DEFAULT_FORMAT);
    }

    public void setRecorder(AudioRecord recorder) {
        this.recorder = recorder;
    }
}
