#ifndef

#include "recognizer.h"
#include "json.h"
#include "language_model.h"

Recognizer::Recognizer(Model *model): model_(model) {
    model_->Ref();

    // main feature extraction pipeline initiated with OnlineNnet2FeaturePipelineInfo
    feature_pipeline_ = new kaldi::OnlineNnet2FeaturePipeline(model_->feature_info_);

    // weighting silence in iVector adaptation, insert silence phones
    silence_weighting_ = new kaldi::OnlineSilenceWeighting(*model_->trans_model_,
                                                           model_->feature_info_.silence_weighting_config,
                                                           3); // frame_subsampling_factor >= 1
    if (!model_->HCLG_fst_) {
        if (model_->HCLr_fst_ && model_->Gr_fst_) {
            decode_fst_ = fst::LookaheadComposeFst(*model_->HCLr_fst_,
                                                   *model_->Gr_fst_,
                                                   model_->disambig_); // vector to_remove
            // Lookahead composition is used for optimized online composition of FSTs during decoding.
            // See nnet3bin/nnet3-latgen-faster-lookahead.cc
            // see DefaultLookAhead in fst/compose.h for details of compose filters
        } else {
            KALDI_ERR << "Can't create decoding graph";
        }
    }
    decoder_ = new kaldi::SingleUtteranceNnet3IncrementalDecoder(
            model_->nnet3_decoding_config_,
            *model_->trans_model_,
            *model_->decodable_info_,
            model_->HCLG_fst_ ? *model_->HCLG_fst_ : *decode_fst_,
            feature_pipeline_);
    //    SingleUtteranceNnet3IncrementalDecoderTpl(
    //    const LatticeIncrementalDecoderConfig &decoder_opts,
    //    const TransitionModel &trans_model,
    //    const nnet3::DecodableNnetSimpleLoopedInfo &info,
    //    const FST &fst,
    //    OnlineNnet2FeaturePipeline *features);

    InitState();
    InitRescoring();
}


void Recognizer::InitState() {
    sampling_frequency_ = 16000;
    frame_offset_ = 0;
    samples_processed_ = 0;
    samples_round_start_ = 0;

    state_ = RECOGNIZER_INITIALIZED;
}

void Recognizer::InitRescoring() {

}

bool Recognizer::AcceptWaveForm(const short *sdata, int len) {
    Vector<BaseFloat> wave;
    wave.Resize(len, kUndefined);
    //    typedef enum {
    //        kSetZero,
    //        kUndefined,
    //        kCopyData
    //    } MatrixResizeType;
    for (int i = 0; i < len; ++i) {
        wave(i) = sdata[i];
    }
    return AcceptWaveform(wave);
}

bool Recognizer::AcceptWaveForm(Vector<BaseFloat> &wave) {
    // Cleanup if we finalized previous utterance or the whole feature pipeline
    if (!(state_ == RECOGNIZER_RUNNING || state_ == RECOGNIZER_INITIALIZED)) {
        CleanUp();
    }
    state_ = RECOGNIZER_RUNNING;

    int step = static_cast<int>(sampling_frequency_ * 0.2);
    for (int i = 0; i < wave.Dim(); i += step) {
        SubVector <BaseFloat> r = wave.Range(i, std::min(step, wave.Dim() - i));
        feature_pipeline_->AcceptWaveform(sampling_frequency_, r);
        UpdateSilenceWeights();
        decoder_->AdvanceDecoding();
    }
    samples_processed_ += wave.Dim();
    if (spk_feature_) {
        spk_feature_->AcceptWaveform(sampling_frequency_, wave);
    }
    if (decoder_->EndpointDetected(model_->endpoint_config_)) {
        return true;
    }
    return false;
}

void Recognizer::CleanUp() {

}

void Recognizer::UpdateSilenceWeights() {

}