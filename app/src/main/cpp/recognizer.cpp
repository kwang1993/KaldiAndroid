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
    //    todo
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
    delete silence_weighting_;
    silence_weighting_ = new kaldi::OnlineSilenceWeighting(*model_->trans_model_,
                                                           model_->feature_info_.silence_weighting_config,
                                                           3); // frame_subsampling_factor >= 1
    if (decoder_) {
        frame_offset_ += decoder_->NumFramesDecoded();
    }
    if (decoder_ == nullptr || state_ == RECOGNIZER_FINALIZED || frame_offset_ > 20000) {
        samples_round_start_ += samples_processed;
        samples_processed_ = 0;
        frame_offset_ = 0;

        delete decoder_;
        delete feature_pipeline_;

        feature_pipeline_ = new kaldi::OnlineNnet2FeaturePipeline(model_->feature_info_);
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

    } else {
        decoder_->InitDecoding(frame_offset_); // call InitDecoding and then (possibly multiple times) AdvanceDecoding().
    }


}

void Recognizer::UpdateSilenceWeights() {
    if (silence_weighting_->Active() &&
        feature_pipeline_->NumFramesReady() > 0 &&
        feature_pipeline_->IvectorFeature() != nullptr ) {

        vector<pair<int32, BaseFloat> > delta_weights;
        silence_weighting_->ComputeCurrentTraceback(decoder_->Decoder());
        silence_weighting_->GetDeltaWeights(feature_pipeline_->NumFramesReady(),
                                            frame_offset_ * 3,
                                            &delta_weights);
        feature_pipeline_->UpdateFrameWeights(delta_weights);

    }
}

const char* Recognizer::Result(){
    if (state_ != RECOGNIZER_RUNNING) {
        return StoreEmptyReturn();
    }
    decoder_->FinalizeDecoding();
    state_ = RECOGNIZER_ENDPOINT;
    return GetResult();
}

const char *Recognizer::StoreEmptyReturn() {

}

const char* Recognizer::GetResult() {
    if (decoder_->NumFramesDecoded() == 0) {
        return StoreEmptyReturn();
    }
    
}

Recognizer::~Recognizer() {
    delete decoder_;
    delete feature_pipeline_;
    delete silence_weighting_;
    delete decode_fst_;
    delete spk_feature_;

    model_->Unref();
    if (spk_model_) {
        spk_model_->Unref();
    }
}
