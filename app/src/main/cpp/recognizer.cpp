#ifndef

#include "recognizer.h"


Recognizer::Recognizer(Model *model): model_(model) {
    feature_pipeline_ = new kaldi::OnlineNnet2FeaturePipeline(model_->feature_info_); // OnlineNnet2FeaturePipelineInfo

}