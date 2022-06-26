

#ifndef KALDIANDROID_MODEL_H
#define KALDIANDROID_MODEL_H

#include <atomic>

#include "kaldi.h"

using namespace std;
using namespace kaldi;

class Model {

public:
    Model(const char* model_path);

private:
    ~Model();
    void ConfigureV1();
    void ConfigureV2();
    void ReadDataFiles();

    string model_path_;
    string final_mdl_rxfilename_;
    string HCLG_fst_rxfilename_;
    string HCLr_fst_rxfilename_;
    string Gr_fst_rxfilename_;
    string disambig_tid_int_rxfilename_;
    string words_txt_rxfilename_;
    string word_boundary_int_rxfilename_;
    string rescore_G_carpa_rxfilename_;
    string rescore_G_fst_rxfilename_;
    string ivector_final_ie_rxfilename_;
    string mfcc_conf_rxfilename_;
    string fbank_conf_rxfilename_;
    string global_cmvn_stats_rxfilename_;
    string pitch_conf_rxfilename_;
    string rnnlm_word_feats_rxfilename_;
    string rnnlm_feat_embedding_final_mat_rxfilename_;
    string rnnlm_special_symbol_opts_conf_rxfilename_;
    string rnnlm_final_raw_rxfilename_;

    kaldi::LatticeIncrementalDecoderConfig nnet3_decoding_config_; // compared to LatticeFasterDecoder, spread out the time of lattice determinization
    kaldi::OnlineEndpointConfig endpoint_config_; //  terminate decoding if ANY of these rules evaluates to "true".
    kaldi::nnet3::NnetSimpleLoopedComputationOptions decodable_opts_; // decodable object based on breaking up the input into fixed chunks
    kaldi::OnlineNnet2FeaturePipelineInfo feature_info_; // feature_type("mfcc"), add_pitch(false)

    kaldi::TransitionModel* trans_model_ = nullptr;
    kaldi::nnet3::AmNnetSimple* nnet_ = nullptr;
    kaldi::nnet3::DecodableNnetSimpleLoopedInfo* decodable_info_ = nullptr;

};

#endif // KALDIANDROID_MODEL_H