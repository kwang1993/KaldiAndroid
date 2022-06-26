#include "model.h"

#include <sys/stat.h>

Model::Model(const char* model_path): model_path_(model_path) {
    // new configuration with parameters initialized in model.conf
    string am_path_v2 = model_path_ + "/am/final.mdl";
    string model_conf_path_v2 = model_path_ + "/conf/model.conf"; // create it yourself for default decoding beams and silence phones.
    //    --min-active=200
    //    --max-active=5000
    //    --beam=12.0
    //    --lattice-beam=4.0
    //    --acoustic-scale=1.0
    //    --frame-subsampling-factor=3
    //    --endpoint.silence-phones=1:2:3:4:5:6:7:8:9:10
    //    --endpoint.rule2.min-trailing-silence=0.5
    //    --endpoint.rule3.min-trailing-silence=1.0
    //    --endpoint.rule4.min-trailing-silence=2.0

    // old configuration without model.conf
    string am_path_v1 = model_path_ + "/final.mdl";
    string mfcc_path_v1 = model_path_ + "/mfcc.conf";
    //    --use-energy=false
    //    --sample-frequency=16000
    //    --num-mel-bins=40
    //    --num-ceps=40
    //    --low-freq=40
    //    --high-freq=-200
    //    --allow-upsample=true
    //    --allow-downsample=true

    struct stat buffer; // <sys/stat.h> check file existing stat
    if (stat(am_path_v2.c_str(), &buffer) == 0) && (stat(model_conf_path_v2.c_str(), &buffer) == 0) {
        ConfigureV2();
        ReadDataFiles();
    } else if (stat(am_path_v1.c_str(), &buffer) == 0) && (stat(mfcc_path_v1.c_str(), &buffer) == 0) {
        ConfigureV1();
        ReadDataFiles();
    } else {
        KALDI_ERR << "Folder '" << model_path_ << "' does not contain model files. " <<
                  "Make sure you specified the model path properly in Model constructor. " <<
                  "If you are not sure about relative path, use absolute path specification.";
    }

}

void Model::ConfigureV1() {
    const char *extra_args[] = {
            "--beam=13.0",
            "--max-active=7000",
            "--lattice-beam=6.0",

            "--endpoint.silence-phones=1:2:3:4:5:6:7:8:9:10",
            "--endpoint.rule2.min-trailing-silence=0.5",
            "--endpoint.rule3.min-trailing-silence=1.0",
            "--endpoint.rule4.min-trailing-silence=2.0",

            "--frame-subsampling-factor=3",
            "--acoustic-scale=1.0",

            "--print-args=false",
    };

    // create po
    kaldi::ParseOptions po(""); // no usage str

    // register po
    nnet3_decoding_config_.Register(&po); // LatticeIncrementalDecoderConfig, compared to LatticeFasterDecoder, spread out the time of lattice determinization
    //    beam(16.0),
    //    max_active(std::numeric_limits<int32>::max()),
    //    min_active(200),
    //    lattice_beam(10.0),
    //    prune_interval(25),
    //    beam_delta(0.5),
    //    hash_ratio(2.0),
    //    prune_scale(0.01),
    //    determinize_max_delay(60),
    //    determinize_min_chunk_size(20)
    //    det_opts.minimize = false;
    endpoint_config_.Register(&po); // OnlineEndpointConfig, terminate decoding if ANY of these rules evaluates to "true".
    //    opts->Register("endpoint.silence-phones", &silence_phones, "List of silence phones");
    //    rule1.RegisterWithPrefix("endpoint.rule1", opts); 1:2:3:4, colon separated list of phones that we consider as silence for purposes of endpointing
    //    rule2.RegisterWithPrefix("endpoint.rule2", opts); times out after 0.5 seconds of silence if we reached the final-state with good probability
    //    rule3.RegisterWithPrefix("endpoint.rule3", opts); times out after 1.0 seconds of silence if we reached the final-state with OK probability
    //    rule4.RegisterWithPrefix("endpoint.rule4", opts); times out after 2.0 seconds of silence even if we did not reach a final-state
    //    rule5.RegisterWithPrefix("endpoint.rule5", opts); times out after the utterance is 20 seconds long regardless of anything else
    decodable_opts_.Register(&po); // nnet3::NnetSimpleLoopedComputationOptions, decodable object based on breaking up the input into fixed chunks
    //    extra_left_context_initial(0),
    //    frame_subsampling_factor(1),
    //    frames_per_chunk(20),
    //    acoustic_scale(0.1),
    //    debug_computation(false)

    // read po args
    vector <const char*> args;
    args.push_back("kwang"); // ???
    args.insert(args.end(), extra_args, extra_args + sizeof(extra_args) / sizeof(extra_args[0])); // vector::insert(pos,first,last)
    po.Read(args.size(), args.data()); // vector::data() from c++11, return pointer to first element (const char* const*)

    // init filenames
    final_mdl_rxfilename_ = model_path_ + "/final.mdl";
    HCLG_fst_rxfilename_ = model_path_ + "/HCLG.fst";
    HCLr_fst_rxfilename_ = model_path_ + "/HCLr.fst"; // allow rescore with Gr.fst
    Gr_fst_rxfilename_ = model_path_ + "/Gr.fst";
    disambig_tid_int_rxfilename_ = model_path_ + "/disambig_tid.int";
    words_txt_rxfilename_ = model_path_ + "/words.txt";
    word_boundary_int_rxfilename_ = model_path_ + "/word_boundary.int";
    rescore_G_carpa_rxfilename_ = model_path_ + "/rescore/G.carpa";
    rescore_G_fst_rxfilename_ = model_path_ + "/rescore/G.fst";
    ivector_final_ie_rxfilename_ = model_path_ + "/ivector/final.ie";
    mfcc_conf_rxfilename_ = model_path_ + "/mfcc.conf";
    fbank_conf_rxfilename_ = model_path_ + "/fbank.conf";
    global_cmvn_stats_rxfilename_ = model_path_ + "/global_cmvn.stats";
    pitch_conf_rxfilename_ = model_path_ + "/pitch.conf";
    rnnlm_word_feats_rxfilename_ = model_path_ + "/rnnlm/word_feats.txt";
    rnnlm_feat_embedding_final_mat_rxfilename_ = model_path_ + "/rnnlm/feat_embedding.final.mat";
    rnnlm_special_symbol_opts_conf_rxfilename_ = model_path_ + "/rnnlm/special_symbol_opts.conf";
    rnnlm_final_raw_rxfilename_ = model_path_ + "/rnnlm/final.raw";
}

void Model::ConfigureV2() {
    // create po
    kaldi::ParseOptions po(""); // No usage str

    // register po
    nnet3_decoding_config_.Register(&po);
    endpoint_config_.Register(&po);
    decodable_opts_.Register(&po);

    // read po args
    po.ReadConfigFile(model_path_ + "/conf/model.conf");

    // init filenames
    final_mdl_rxfilename_ = model_path_ + "/am/final.mdl";
    HCLG_fst_rxfilename_ = model_path_ + "/graph/HCLG.fst";
    HCLr_fst_rxfilename_ = model_path_ + "/graph/HCLr.fst"; // allow rescore with Gr.fst
    Gr_fst_rxfilename_ = model_path_ + "/graph/Gr.fst";
    disambig_tid_int_rxfilename_ = model_path_ + "/graph/disambig_tid.int";
    words_txt_rxfilename_ = model_path_ + "/graph/words.txt";
    word_boundary_int_rxfilename_ = model_path_ + "/graph/phones/word_boundary.int";
    rescore_G_carpa_rxfilename_ = model_path_ + "/rescore/G.carpa";
    rescore_G_fst_rxfilename_ = model_path_ + "/rescore/G.fst";
    ivector_final_ie_rxfilename_ = model_path_ + "/ivector/final.ie";
    mfcc_conf_rxfilename_ = model_path_ + "/conf/mfcc.conf";
    fbank_conf_rxfilename_ = model_path_ + "/conf/fbank.conf";
    global_cmvn_stats_rxfilename_ = model_path_ + "/am/global_cmvn.stats";
    pitch_conf_rxfilename_ = model_path_ + "/conf/pitch.conf";
    rnnlm_word_feats_rxfilename_ = model_path_ + "/rnnlm/word_feats.txt";
    rnnlm_feat_embedding_final_mat_rxfilename_ = model_path_ + "/rnnlm/feat_embedding.final.mat";
    rnnlm_special_symbol_opts_conf_rxfilename_ = model_path_ + "/rnnlm/special_symbol_opts.conf";
    rnnlm_final_raw_rxfilename_ = model_path_ + "/rnnlm/final.raw";
}

void Model::ReadDataFiles() {
    struct stat buffer; // <sys/stat.h> check file existing stat

    // log main args in configs
    KALDI_LOG << "Decoding params beam=" << nnet3_decoding_config_.beam <<
                 " max-active=" << nnet3_decoding_config_.max_active <<
                 " lattice-beam=" << nnet3_decoding_config_.lattice_beam;
    KALDI_LOG << "Silence phones " << endpoint_config_.silence_phones;

    // OnlineNnet2FeaturePipelineInfo, init feature info
    if (stat(mfcc_conf_rxfilename_.c_str(), &buffer) == 0) {
        feature_info_.feature_type = "mfcc";
        ReadConfigFromFile(mfcc_conf_rxfilename_, &feature_info_.mfcc_opts); // MfccOptions
        feature_info_.mfcc_opts.frame_opts.allow_downsample = true; // MfccOptions, FrameExtractionOptions
    } else if (stat(fbank_conf_rxfilename_.c_str(), &buffer) == 0) {
        feature_info_.feature_type = "fbank";
        ReadConfigFromFile(fbank_conf_rxfilename_, &feature_info_.fbank_opts);
        feature_info_.fbank_opts.frame_opts.allow_downsample = true; // It is safe to downsample
    } else {
        KALDI_ERR << "Failed to find feature config file";
    }
    // OnlineSilenceWeightingConfig, Config for weighting silence in iVector adaptation.
    feature_info_.silence_weighting_config.silence_weight = 1e-3;
    feature_info_.silence_weighting_config.silence_phones_str = endpoint_config_.silence_phones;

    // Create transition model and nnet
    trans_model_ = new kaldi::TransitionModel();
    nnet_ = new kaldi::nnet3::AmNnetSimple();
    { // 先初始化模型对象，再通过Input 类保存文件内容，最后模型读取以文件流的形式进行读取，所有模型类都配有Read 这一成员函数
        bool binary;
        kaldi::Input ki(final_mdl_rxfilename_, &binary); // Open(rxfilename, *binary)
        trans_model_->Read(ki.Stream(), binary); // Read phone, state, pdf from std::istream&
        nnet_->Read(ki.Stream(), binary); // Read contexts, priors
        kaldi::nnet3::SetBatchnormTestMode(true, &(nnet_->GetNnet())); // Use batch norm
        kaldi::nnet3::SetDropoutTestMode(true, &(nnet_->GetNnet())); // Use dropout mask containing (1-dropout_prob) in all elements.
        kaldi::nnet3::CollapseModel(kaldi::nnet3::CollapseModelConfig(), &(nnet_->GetNnet())); // compile final.mdl
    }
    decodable_info_ = new kaldi::nnet3::DecodableNnetSimpleLoopedInfo(decodable_opts_, nnet_);

    // ivector

    // cmvn
    // pitch
    // HCLG
    // words
    // RNN rescoring

}