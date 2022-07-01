
#ifndef KALDIANDROID_RECOGNIZER_H
#define KALDIANDROID_RECOGNIZER_H

#include "kaldi.h"

#include "model.h"

using namespace kaldi;

enum RecognizerState {
    RECOGNIZER_INITIALIZED,
    RECOGNIZER_RUNNING,
    RECOGNIZER_ENDPOINT,
    RECOGNIZER_FINALIZED
};

class Recognizer {
public:
    Recognizer(Model *model);
    bool AcceptWaveform(const short* sdata, int len);
    const char* Result();
private:
    ~Recognizer();
    void InitState();
    void InitRescoring();
    bool AcceptWaveform(Vector<BaseFloat>& wave);
    void CleanUp();
    void UpdateSilenceWeights();

    Model* model_ = nullptr;
    string last_result_;

    kaldi::OnlineNnet2FeaturePipeline *feature_pipeline_ = nullptr; // main feature extraction pipeline
    kaldi::OnlineSilenceWeighting *silence_weighting_ = nullptr; // weighting silence in ivector adaptation
    fst::LookaheadFst<fst::StdArc, int32> *decode_fst_ = nullptr;
    kaldi::SingleUtteranceNnet3IncrementalDecoder *decoder_ = nullptr;
    kaldi::OnlineBaseFeature *spk_feature_ = nullptr;

    float sampling_frequency_;
    int32 frame_offset_;
    int64 samples_processed_;
    int64 samples_round_start_;
    RecognizerState state_;



};

#endif // KALDIANDROID_RECOGNIZER_H