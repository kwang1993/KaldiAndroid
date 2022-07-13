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
            if (!model_->HCLr_fst_ ) KALDI_ERR << "No HCLr_fst_";
            if (!model_->Gr_fst_) KALDI_ERR << "No Gr_fst_";
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
bool Recognizer::AcceptWaveform(const char *data, int len)
{
    Vector<BaseFloat> wave;
    wave.Resize(len / 2, kUndefined);
    for (int i = 0; i < len / 2; i++){
        wave(i) = *(((short *)data) + i);
    }
    return AcceptWaveform(wave);
}
bool Recognizer::AcceptWaveform(const short *sdata, int len) {
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

bool Recognizer::AcceptWaveform(Vector<BaseFloat> &wave) {
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
        samples_round_start_ += samples_processed_;
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
        fst::Compose(lat, *lm_to_subtract_, &composed_lat);
        fst::Invert(&composed_lat);
        DeterminizeLattice(composed_lat, &slat);
        fst::ScaleLattice(fst::GraphLatticeScale(-1.0), &slat);

        // Add CARPA score
        TopSortCompactLatticeIfNeeded(&slat);
        ComposeCompactLatticeDeterministic(slat, carpa_to_add_, &tlat);

        // Rescore with RNNLM score on top if needed
        if (rnnlm_to_add_scale_) {
            ComposeLatticePrunedOptions compose_opts;
            compose_opts.lattice_compose_beam = 3.0;
            compose_opts.max_arcs = 3000;
            fst::ComposeDeterministicOnDemandFst<fst::StdArc> combined_rnnlm(carpa_to_add_scale_, rnnlm_to_add_scale_);

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

static bool CompactLatticeToWordAlignmentWeight(const CompactLattice &clat,
                                                std::vector<int32> *words,
                                                std::vector<int32> *begin_times,
                                                std::vector<int32> *lengths,
                                                CompactLattice::Weight *tot_weight_out) {

    typedef CompactLattice::Arc Arc;
    typedef Arc::Label Label;
    typedef CompactLattice::StateId StateId;
    typedef CompactLattice::Weight Weight;
    using namespace fst;

    words->clear();
    begin_times->clear();
    lengths->clear();
    *tot_weight_out = Weight::Zero();

    StateId state = clat.Start();
    Weight tot_weight = Weight::One();

    int32 cur_time = 0;
    if (state == kNoStateId) {
        KALDI_WARN << "Empty lattice.";
        return false;
    }
    while(1) {
        Weight final = clat.Final(state);
        size_t num_arcs = clat.NumArcs(state);
        if (final != Weight::Zero()) {
            if (num_arcs != 0) {
                KALDI_WARN << "Lattice is not linear.";
                return false;
            }
            if (!final.String().empty()) {
                KALDI_WARN << "Lattice has alignments on final-weight: probably "
                              "was not word-aligned (alignments will be approximate)";
            }
            tot_weight = Times(final, tot_weight);
            *tot_weight_out = tot_weight;
            return true;
        } else {
            if (num_arcs != 1) {
                KALDI_WARN << "Lattice is not linear: num-arcs = " << num_arcs;
                return false;
            }
            fst::ArcIterator<CompactLattice> aiter(clat, state);
            const Arc & arc = aiter.Value();
            Label word_id = arc.ilabel;// Note: ilabel==olabel, since acceptor.
            // Also note: word_id may be zero; we output it anyway.
            int32 length = arc.weight.String().size();
            words->push_back(word_id);
            begin_times->push_back(cur_time);
            lengths->push_back(length);
            tot_weight = Times(arc.weight, tot_weight);
            cur_time += length;
            state = arc.nextstate;
        }
    }
}

const char *Recognizer::NbestResult(CompactLattice &clat){
    Lattice lat, nbest_lat;
    std::vector<Lattice> nbest_lats;

    ConvertLattice(clat, &lat);
    fst::ShortestPath(lat, &nbest_lat, max_alternatives_);
    fst::ConvertNbestToVector(nbest_lat, &nbest_lats);

    json::JSON obj;
    for (int k = 0; k < nbest_lats.size(); ++k) {
        Lattice nlat = nbest_lats[k];

        CompactLattice nclat;
        fst::Invert(&nlat);
        DeterminizeLattice(nlat, &nclat);

        CompactLattice aligned_nclat;
        if (model_->word_boundary_info_) {
            WordAlignLattice(nclat,
                             *model_->trans_model_,
                             *model_->word_boundary_info_,
                             0,
                             &aligned_nclat);
            //        const CompactLattice & 	lat,
            //        const TransitionModel & 	tmodel,
            //        const WordBoundaryInfo & 	info,
            //        int32 	max_states,
            //        CompactLattice * 	lat_out

        } else {
            aligned_nclat = nclat;
        }

        std::vector<int32> words, begin_times, lengths;
        CompactLattice::Weight weight;

        CompactLatticeToWordAlignmentWeight(aligned_nclat, &words, &begin_times, &lengths, &weight);
        float likelihood = - (weight.Weight().Value1() + weight.Weight().Value2());

        stringstream text;
        json::JSON entry;

        for (int i = 0, first = 1; i < words.size(); ++i ) {
            json::JSON word;
            if (words[i] == 0) {
                continue;
            }
            if (words_) {
                word["word"] = model_->word_syms_->Find(words[i]);
                word["start"] = samples_round_start_ / sampling_frequency_ + (frame_offset_ + begin_times[i]) * 0.03;
                word["end"] = samples_round_start_ / sampling_frequency_ + (frame_offset_ + begin_times[i] +lengths[i]) * 0.03;
                entry["result"].append(word);
            }
            if (first) {
                first = 0;
            } else {
                text << " ";
            }
            text << model_->word_syms_->Find(words[i]);
        }
        entry["text"] = text.str();
        entry["confidence"] = likelihood;
        obj["alternatives"].append(entry);
    }
    return StoreReturn(obj.dump());
}

const char *Recognizer::MbrResult(CompactLattice &rlat){
    CompactLattice aligned_lat;
    if (model_->word_boundary_info_) {
        WordAlignLattice(rlat,
                         *model_->trans_model_,
                         *model_->word_boundary_info_,
                         0,
                         &aligned_lat);
        //        const CompactLattice & 	lat,
        //        const TransitionModel & 	tmodel,
        //        const WordBoundaryInfo & 	info,
        //        int32 	max_states,
        //        CompactLattice * 	lat_out
    } else {
        aligned_lat = rlat;
    }
    MinimumBayesRisk mbr(aligned_lat);
    const vector <BaseFloat> & conf = mbr.GetOneBestConfidences();
    const vector <int32> &words = mbr.GetOneBest();
    const vector <pair<BaseFloat, BaseFloat> >& times = mbr.GetOneBestTimes();

    int size = words.size();

    json::JSON obj;
    stringstream text;

    for (int i = 0; i < size ; ++i) {
        json::JSON word;

        if (words_) {
            word["word"] = model_->word_syms_->Find(words[i]);
            word["start"] = samples_round_start_ / sampling_frequency_ + (frame_offset_ + times[i].first) * 0.03;
            word["end"] = samples_round_start_ / sampling_frequency_ + (frame_offset_ + times[i].second) * 0.03;
            word["conf"] = conf[i];
            obj["result"].append(word);
        }
        if (i) {
            text << " ";
        }
        text << model_->word_syms_->Find(words[i]);
        obj["text"] = text.str();

//        if (spk_model_) {
//            Vector<BaseFloat> xvector;
//            int num_spk_frames;
//            if (GetSpkVector(xvector, &num_spk_frames)) {
//                for (int i = 0; i < xvector.Dim(); i++) {
//                    obj["spk"].append(xvector(i));
//                }
//                obj["spk_frames"] = num_spk_frames;
//            }
//        }

        return StoreReturn(obj.dump());
    }

}

const char *Recognizer::NlsmlResult(CompactLattice &clat){
    //todo
}



const char *Recognizer::StoreEmptyReturn() {
    if (!max_alternatives_) {
//        return StoreReturn("{\"text\": \"\"}");
        return StoreReturn("");
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
    if (state_ != RECOGNIZER_RUNNING) {
        return StoreEmptyReturn();
    }

    json::JSON res;

    if (partial_words_) {
        if (decoder_->NumFramesInLattice() == 0 ) {
            res["partial"] = "";
            return StoreReturn(res.dump());
        }
        CompactLattice clat, aligned_lat;

        clat = decoder_->GetLattice(decoder_->NumFramesInLattice(), false);
        //WordAlignLatticePartial
        WordAlignLattice(clat,
                        *model_->trans_model_,
                        *model_->word_boundary_info_,
                        0,
                        &aligned_lat);

        MinimumBayesRisk mbr(aligned_lat);
        const vector<BaseFloat> &conf = mbr.GetOneBestConfidences();
        const vector<int32> &words = mbr.GetOneBest();
        const vector<pair<BaseFloat, BaseFloat> > &times = mbr.GetOneBestTimes();

        int size = words.size();

        stringstream text;

        // Create JSON object
        for (int i = 0; i < size; i++) {
            json::JSON word;

            word["word"] = model_->word_syms_->Find(words[i]);
            word["start"] = samples_round_start_ / sampling_frequency_ + (frame_offset_ + times[i].first) * 0.03;
            word["end"] = samples_round_start_ / sampling_frequency_ + (frame_offset_ + times[i].second) * 0.03;
            word["conf"] = conf[i];
            res["partial_result"].append(word);

            if (i) {
                text << " ";
            }
            text << model_->word_syms_->Find(words[i]);
        }
        res["partial"] = text.str();
    } else {
        if (decoder_->NumFramesDecoded() == 0) {
            res["partial"] = "";
            return StoreReturn(res.dump());
        }
        Lattice lat;
        decoder_->GetBestPath(false, &lat);
        vector <kaldi::int32> alignment, words;
        LatticeWeight weight;
        fst::GetLinearSymbolSequence(lat, &alignment, &words, &weight);
        ostringstream text;
        for (size_t i = 0; i < words.size(); ++i) {
            if (i) {
                text << " ";
            }
            text << model_->word_syms_->Find(words[i]);
        }
        res["partial"] = text.str();
    }
    return StoreReturn(res.dump());

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
    //if (spk_model_) {
    //    spk_model_->Unref();
    //}
}

