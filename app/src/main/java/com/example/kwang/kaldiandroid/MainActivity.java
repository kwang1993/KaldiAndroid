package com.example.kwang.kaldiandroid;

import static java.lang.Thread.sleep;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.content.pm.PackageManager;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ToggleButton;

import com.example.kwang.kaldiandroid.audiorecord.AudioParams;
import com.example.kwang.kaldiandroid.audiorecord.AudioRecordManager;
import com.example.kwang.kaldiandroid.audiorecord.RecordCallback;
import com.example.kwang.kaldiandroid.util.KaldiUtil;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;


public class MainActivity extends AppCompatActivity{

    private Handler textHandler = new Handler(); // 通知有输出文本要显示
    private StringBuffer sb = new StringBuffer(); // 存储输出文本
    private AudioRecordManager audioRecordManager = new AudioRecordManager();
    private String filePath = Environment.getExternalStorageDirectory().getAbsolutePath() + "/Recordings/audio.wav";

    private EditText screen, state;
    private Button replay, delete, clear;
    private ToggleButton startStop;

    private enum ButtonState {
        IDLE, RECORDING, REPLAY, DELETE
    }

    private void updateButtonState(ButtonState state) {
        switch (state) {
            case RECORDING:
                startStop.setEnabled(true);
                replay.setEnabled(false);
                delete.setEnabled(false);
                break;
            case REPLAY:
            case DELETE:
            case IDLE:
            default:
                startStop.setEnabled(true);
                replay.setEnabled(true);
                delete.setEnabled(true);
        }
    }
    private void getPermissions() {
        String [] permissions = {
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
                Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.RECORD_AUDIO
        };

        for (int i = 0; i < permissions.length; i++) {
            if (this.checkSelfPermission(permissions[i]) != PackageManager.PERMISSION_GRANTED) {
                this.requestPermissions(permissions, i);
            }
        }
    }
    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode < permissions.length) {
            if (grantResults.length > 1
                    && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                // pass
            } else {
                Log.e("mediaRecorder", "Recording permission not found");
                finish();
            }
        }
    }
    private void updateState(String text) {
        state.setText(text);
    }

    private void startRecording() {
        updateButtonState(ButtonState.RECORDING);
        audioRecordManager.startRecording(filePath);
        updateState("Recording started: " + filePath);

    }
    private void stopRecording() {
        audioRecordManager.stopRecording();
        updateButtonState(ButtonState.IDLE);
        updateState("Recording stopped: " + filePath);
    }
    private void startEngine(){
        getPermissions();
        KaldiUtil.startEngine();
        updateState("Engine started!");
    }
    private void stopEngine(){
        KaldiUtil.stopEngine();
        updateState("Engine stopped!");
    }
    private void startRecognition(){
        startRecording();
        KaldiUtil.startRecognition();
    }
    private void stopRecognition(){
        stopRecording();
        KaldiUtil.stopRecognition();
    }
    private String getResultString(){
        return KaldiUtil.getResultString();
    }

    private void startScreenLogging() {
        Thread textThread = new Thread(() -> {
            while (startStop.getText() == startStop.getTextOn()) {
                try {
                    sleep(100);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                String text = getResultString();
                if (text != "") {
                    sb.append(text);
                    textHandler.post(() -> screen.setText(sb.toString()));
                }
            }
        });
        textThread.start();
    }
    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopRecognition();
        stopEngine();
    }

    private void initLayout() {
        // 状态栏
        state = (EditText) findViewById(R.id.state);

        // 显示输出
        screen = (EditText) findViewById(R.id.screen);
        screen.setEnabled(false);
        // tv.setText("kaldi.Math.LogAdd: \n ln(e^" + x + " + e^" + y + ") = " + v);

        // 删除录音
        delete = (Button) findViewById(R.id.delete);
        delete.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                File f = new File(filePath);
                if (f.exists()) {
                    f.delete();
                    Log.e("audioRecord", "File deleted: " + filePath);
                    updateState("File deleted: " + filePath);
                } else {
                    Log.e("audioRecord", "File not exist for deletion: " + filePath);
                    updateState("No file to delete: " + filePath);
                }
            }
        });

        // 播放录音
        replay = (Button) findViewById(R.id.replay);
        replay.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                File f = new File(filePath);
                if (f.exists()){
                    audioRecordManager.replay(filePath);
                    updateState("Replayed audio: " + filePath);
                } else{
                    updateState("No audio to replay: " + filePath);
                }

            }
        });

        // 清除输出
        clear = (Button) findViewById(R.id.clear);
        clear.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                sb = new StringBuffer();
                screen.setText("");
                updateState("Screen cleared!");
            }
        });


        // 开始结束
        startStop = (ToggleButton) findViewById(R.id.startStop);
        startStop.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (startStop.getText() == startStop.getTextOn()) {
                    startRecognition();
                    startScreenLogging();
                }
                else {
                    stopRecognition();
                }
            }
        });

        updateButtonState(ButtonState.IDLE);

    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // double x = 1;
        // double y = 2;
        // double v = KaldiUtil.KaldiMathLogAdd(x, y);  // 调用native方法
        // Log.e("ard", "这里是java.kaldi计算的结果：" + v);
        // screen.setText("kaldi.Math.LogAdd: \n ln(e^" + x + " + e^" + y + ") = " + v);

        initLayout();
        // 启动识别引擎
        startEngine();
    }

}
