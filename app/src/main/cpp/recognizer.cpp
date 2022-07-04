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

void Recognizer::InitRescoring() { // ?????
    if (model_->graph_lm_fst_) {
        fst::CacheOptions cache_opts(true, -1);
        fst::ArcMapFstOptions mapfst_opts(cache_opts);
        fst::StdToLatticeMapper<BaseFloat> mapper; // maps a normal arc (StdArc) to a LatticeArc by putting the StdArc weight as the first element of the LatticeWeight. Useful when doing LM rescoring.
        lm_to_subtract_ = new fst::ArcMapFst<fst::StdArc, LatticeArc, fst::StdToLatticeMapper<BaseFloat> >(
                *model_->graph_lm_fst_, mapper, mapfst_opts);
        carpa_to_add_ = new ConstArpaLmDeterministicFst(model_->const_arpa_);

        if (model_->rnnlm_enabled_) {
            int lm_order = 4;
            rnnlm_info_ = new kaldi::rnnlm::RnnlmComputeStateInfo(
                    model_->rnnlm_compute_opts_, model_->rnnlm_, model_->word_embedding_mat_);
            rnnlm_to_add_ = new kaldi::rnnlm::KaldiRnnlmDeterministicFst(lm_order, *rnnlm_info_);
            rnnlm_to_add_scale_ = new fst::ScaleDeterministicOnDemandFst(0.5, rnnlm_to_add_);
            carpa_to_add_scale_ = new fst::ScaleDeterministicOnDemandFst(-0.5, carpa_to_add_);
        }
    }
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



const char* Recognizer::GetResult() { // ?????
    if (decoder_->NumFramesDecoded() == 0) {
        return StoreEmptyReturn();
    }
    // Original from decoder, subtracted graph weight, rescored with carpa, rescored with rnnlm
    CompactLattice clat, slat, tlat, rlat;
    clat = decoder_->GetLattice(decoder_->NumFramesDecoded(), true); // num_frames_to_include, use_final_probs

    if (lm_to_subtract_ && carpa_to_add_) { // from InitRescoring
        Lattice lat, composed_lat;

        // Delete old score
        ConvertLattice(clat, &lat);
        fst::ScaleLattice(fst::GraphLatticeScale(-1.0), &lat);
        fst::Compose(lat, *lm_to_subtract, &composed_lat);
        fst::Invert(&composed_lat);
        DeterminizeLattice(composed_lat, &slat);
        fst::ScaleLattice(fst::GraphLatticeScale(-1.0), &slat);

        // Add CARPA score
        TopSortCompactLatticeIfNeeded(&slat);
        ComposeCompactLatticeDeterministic(slat, capra_to_add_, &tlat);

        // Rescore with RNNLM score on top if needed
        if (rnnlm_to_add_scale_) {
            ComposeLatticePrunedOptions compose_opts;
            compose_opts.lattice_compose_beam = 3.0;
            compose_opts.max_arcs = 3000;
            fst::ComposeDeterministicOnDemandFst<StdArc> combined_rnnlm(carpa_to_add_scale_, rnnlm_to_add_scale_);

            TopSortCompactLatticeIfNeeded(&tlat);
            ComposeCompactLatticePruned(compose_opts, tlat,
                                        &combined_rnnlm, &rlat);
            rnnlm_to_add_->Clear();
        } else {
            rlat = tlat;
        }
    } else {
        rlat = clat;
    }

    // Pruned composition can return empty lattice.
    if (rlat.Start() != 0) {
        return StoreEmptyReturn();
    }

    // Apply rescoring weight
    fst::ScaleLattice(fst::GraphLatticeScale(0.9), &rlat);
    if (max_alternatives_ == 0) {
        return MbrResult(rlat);
    } else if (nlsml_) {
        return NlsmlResult(rlat);
    } else {
        return NbestResult(rlat);
    }
}

const char *Recognizer::NbestResult(CompactLattice &rlat){
    //todo
}

const char *Recognizer::MbrResult(CompactLattice &rlat){
    //todo
}

const char *Recognizer::NlsmlResult(CompactLattice &rlat){
    //todo
}


const char *Recognizer::StoreEmptyReturn() {
    if (! max_alternatves_) {
        return StoreReturn("{\"text\": \"\"}")
    } else if (nlsml_) {
        return StoreReturn("<?xml version=\"1.0\"?>\n"
                           "<result grammar=\"default\">\n"
                           "<interpretation confidence=\"1.0\">\n"
                           "<instance/>\n"
                           "<input><noinput/></input>\n"
                           "</interpretation>\n"
                           "</result>\n");
    } else {
        return StoreReturn("{\"alternatives\" : [{\"text\": \"\", \"confidence\" : 1.0}] }");
    }
}


// Store result in recognizer and return as const string
const char *Recognizer::StoreReturn(const string &res)
{
    last_result_ = res;
    return last_result_.c_str();
}

const char* Recognizer::FinalResult()
{
    if (state_ != RECOGNIZER_RUNNING) {
        return StoreEmptyReturn();
    }

    feature_pipeline_->InputFinished();
    UpdateSilenceWeights();
    decoder_->AdvanceDecoding();
    decoder_->FinalizeDecoding();
    state_ = RECOGNIZER_FINALIZED;
    GetResult();

    // Free some memory while we are finalized, next
    // iteration will reinitialize them anyway
    delete decoder_;
    delete feature_pipeline_;
    delete silence_weighting_;
    delete spk_feature_;

    feature_pipeline_ = nullptr;
    silence_weighting_ = nullptr;
    decoder_ = nullptr;
    spk_feature_ = nullptr;

    return last_result_.c_str();
}

const char* Recognizer::PartialResult()
{
    //todo
}

void Recognizer::Reset()
{
    if (state_ == RECOGNIZER_RUNNING) {
        decoder_->FinalizeDecoding();
    }
    StoreEmptyReturn();
    state_ = RECOGNIZER_ENDPOINT;
}

Recognizer::~Recognizer() {
    delete decoder_;
    delete feature_pipeline_;
    delete silence_weighting_;
    delete decode_fst_;
    delete spk_feature_;

    delete lm_to_subtract_;
    delete carpa_to_add_;
    delete carpa_to_add_scale_;
    delete rnnlm_info_;
    delete rnnlm_to_add_;
    delete rnnlm_to_add_scale_;

    model_->Unref();
    if (spk_model_) {
        spk_model_->Unref();
    }
}
