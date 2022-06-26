
#ifndef KALDIANDROID_RECOGNIZER_H
#define KALDIANDROID_RECOGNIZER_H

#include "kaldi.h"

#include "model.h"

using namespace kaldi;

class Recognizer {
public:
    Recognizer(Model *model);
    bool AcceptWaveform(const short* sdata, int len);
    const char* Result();
private:
    ~Recognizer();

    Model* model_ = nullptr;
    string last_result_;

    OnlineNnet2FeaturePipeline *feature_pipeline_ = nullptr; // main feature extraction pipeline

};

#endif // KALDIANDROID_RECOGNIZER_H