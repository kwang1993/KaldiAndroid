
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
    const char* FinalResult();
    const char* PartialResult();
    void Reset();
    ~Recognizer();
private:
    void InitState();
    void InitRescoring();
    bool AcceptWaveform(Vector<BaseFloat>& wave);
    void CleanUp();
    void UpdateSilenceWeights();
    const char* GetResult();
    const char *MbrResult(CompactLattice &clat);
    const char *NbestResult(CompactLattice &clat);
    const char *NlsmlResult(CompactLattice &clat);
    const char *StoreEmptyReturn();
    const char *StoreReturn(const string &res);


    Model* model_ = nullptr;

    kaldi::OnlineNnet2FeaturePipeline *feature_pipeline_ = nullptr; // main feature extraction pipeline
    kaldi::OnlineSilenceWeighting *silence_weighting_ = nullptr; // weighting silence in ivector adaptation
    fst::LookaheadFst<fst::StdArc, int32> *decode_fst_ = nullptr;
    kaldi::SingleUtteranceNnet3IncrementalDecoder *decoder_ = nullptr;
    // Speaker identification
    //SpkModel *spk_model_ = nullptr;
    OnlineBaseFeature *spk_feature_ = nullptr;
    // Rescoring
    fst::ArcMapFst<fst::StdArc, LatticeArc, fst::StdToLatticeMapper<BaseFloat> > *lm_to_subtract_ = nullptr;
    kaldi::ConstArpaLmDeterministicFst *carpa_to_add_ = nullptr;
    fst::ScaleDeterministicOnDemandFst *carpa_to_add_scale_ = nullptr;
    // RNNLM rescoring
    kaldi::rnnlm::RnnlmComputeStateInfo *rnnlm_info_ = nullptr;
    kaldi::rnnlm::KaldiRnnlmDeterministicFst* rnnlm_to_add_ = nullptr;
    fst::DeterministicOnDemandFst<fst::StdArc> *rnnlm_to_add_scale_ = nullptr;


    float sampling_frequency_;
    int32 frame_offset_;
    int64 samples_processed_;
    int64 samples_round_start_;
    RecognizerState state_;

    int max_alternatives_ = 0; // Disable alternatives by default
    bool words_ = false;
    bool partial_words_ = false;
    bool nlsml_ = false;
    string last_result_;

};

#endif // KALDIANDROID_RECOGNIZER_H