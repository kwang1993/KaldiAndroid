package com.example.kwang.kaldiandroid;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import com.example.kwang.kaldiandroid.util.KaldiUtil;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        //double x = 1;
        //double y = 2;
        //double v = KaldiUtil.KaldiMathLogAdd(x, y);  // 调用native方法
        //Log.e("ard", "这里是java.kaldi计算的结果：" + v);

        TextView tv = (TextView) findViewById(R.id.screen);

        //tv.setText("kaldi.Math.LogAdd: \n ln(e^" + x + " + e^" + y + ") = " + v);
    }
}




