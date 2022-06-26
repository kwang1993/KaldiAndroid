package com.example.kwang.kaldiandroid.audiorecord;

import android.media.AudioFormat;

public class AudioParams {

    enum Format {
        SINGLE_8_BIT, DOUBLE_8_BIT, SINGLE_16_BIT, DOUBLE_16_BIT
    }
    private Format format;
    private int sampleRate;

    AudioParams(int sampleRate, Format f) {
        this.sampleRate = sampleRate;
        this.format = f;
    }
    AudioParams(int sampleRate, int channelCount, int bits) {
        this.sampleRate = sampleRate;
        setFormat(channelCount, bits);
    }
    public int getSampleRate() {
        return this.sampleRate;
    }
    public int getBits(){
        return (this.format == Format.SINGLE_8_BIT || this.format == Format.DOUBLE_8_BIT)? 8: 16;
    }
    public int getEncodingFormat() {
        return (this.format == Format.SINGLE_8_BIT || this.format == Format.DOUBLE_8_BIT)?
                AudioFormat.ENCODING_PCM_8BIT : AudioFormat.ENCODING_PCM_16BIT;
    }
    public int getChannelCount() {
        return (this.format == Format.SINGLE_8_BIT || this.format == Format.SINGLE_16_BIT)? 1:2;
    }
    public int getChannelConfig() {
        return getChannelConfig(false);
    }
    public int getChannelConfig(boolean bOut) {
        int channelConfig;
        if (bOut) {
            channelConfig = (this.format == Format.SINGLE_8_BIT || this.format == Format.SINGLE_16_BIT)?
                    AudioFormat.CHANNEL_OUT_MONO : AudioFormat.CHANNEL_OUT_STEREO;
        } else {
            channelConfig = (this.format == Format.SINGLE_8_BIT || this.format == Format.SINGLE_16_BIT)?
                    AudioFormat.CHANNEL_IN_MONO : AudioFormat.CHANNEL_IN_STEREO;
        }
        return channelConfig;
    }
    private void setFormat(int channelCount, int bits) {
        if (channelCount != 1 && channelCount != 2 || bits !=8 && bits != 16) {
            throw new IllegalArgumentException("不支持其他格式 channelCount = $channelCount bits = $bits");
        }
        if (channelCount == 1) {
            if (bits == 8) {
                this.format = Format.SINGLE_8_BIT;
            } else {
                this.format = Format.SINGLE_16_BIT;
            }
        } else {
            if (bits == 8) {
                this.format = Format.DOUBLE_8_BIT;
            } else {
                this.format = Format.DOUBLE_16_BIT;
            }
        }
    }
}
