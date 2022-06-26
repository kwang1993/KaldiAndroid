package com.example.kwang.kaldiandroid.audiorecord;

public class WavUtil {
    public static byte [] getWaveFileHeader(int totalDataLen, int sampleRate, int channelCount, int bits) {
        byte [] header = new byte[44];
        // RIFF header
        header[0] = 'R';
        header[1] = 'I';
        header[2] = 'F';
        header[3] = 'F';

        int fileLen = totalDataLen + 36;
        header[4] = (byte) (fileLen & 0xff);
        header[5] = (byte) (fileLen>>8 & 0xff);
        header[6] = (byte) (fileLen>>16 & 0xff);
        header[7] = (byte) (fileLen>>24 & 0xff);

        // WAVE
        header[8] = 'W';
        header[9] = 'A';
        header[10] = 'V';
        header[11] = 'E';
        // fmt
        header[12] = 'f';
        header[13] = 'm';
        header[14] = 't';
        header[15] = ' ';
        // 4 bytes
        header[16] = 16;
        header[17] = 0;
        header[18] = 0;
        header[19] = 0;
        // pcm format = 1
        header[20] = 1;
        header[21] = 0;
        header[22] = (byte)channelCount;
        header[23] = 0;

        header[24] = (byte) (sampleRate & 0xff);
        header[25] = (byte) (sampleRate>>8 & 0xff);
        header[26] = (byte) (sampleRate>>16 & 0xff);
        header[27] = (byte) (sampleRate>>24 & 0xff);
        int byteRate = sampleRate * bits * channelCount / 8;
        header[28] = (byte) (byteRate & 0xff);
        header[29] = (byte) (byteRate>>8 & 0xff);
        header[30] = (byte) (byteRate>>16 & 0xff);
        header[31] = (byte) (byteRate>>24 & 0xff);
        // block align
        header[32] = (byte) (bits * channelCount / 8);
        header[33] = 0;
        header[34] = (byte) bits;
        header[35] = 0;
        // data
        header[36] = 'd';
        header[37] = 'a';
        header[38] = 't';
        header[39] = 'a';

        header[40] = (byte) (totalDataLen & 0xff);
        header[41] = (byte) (totalDataLen>>8 & 0xff);
        header[42] = (byte) (totalDataLen>>16 & 0xff);
        header[43] = (byte) (totalDataLen>>24 & 0xff);
        return header;
    }
}
