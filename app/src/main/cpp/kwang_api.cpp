//
// Created by kaicheng.wang on 2022/7/11.
//

#include "kwang_api.h"

#include "recognizer.h"
#include "model.h"

using namespace kaldi;


VModel *kwang_model_new(const char *model_path)
{
    try {
        return (VModel *)new Model(model_path);
    } catch (...) {
        return nullptr;
    }
}

void kwang_model_free(VModel *model)
{
    if (model == nullptr) {
        return;
    }
    ((Model *)model)->Unref();
}

int kwang_model_find_word(VModel *model, const char *word)
{
    return (int) ((Model *)model)->FindWord(word);
}

VRecognizer *kwang_recognizer_new(VModel *model) {
    try {
        return (VRecognizer *)new Recognizer((Model *)model);
    } catch (...) {
        return nullptr;
    }
}

void kwang_recognizer_free(VRecognizer *recognizer) {
    delete (Recognizer *)(recognizer);
}
void kwang_recognizer_reset(VRecognizer *recognizer)
{
    ((Recognizer *)recognizer)->Reset();
}

int kwang_recognizer_accept_waveform_s(VRecognizer *recognizer, const short *data, int length)
{
    try {
        return ((Recognizer *)(recognizer))->AcceptWaveform(data, length);
    } catch (...) {
        return -1;
    }
}

const char *kwang_recognizer_result(VRecognizer *recognizer)
{
    return ((Recognizer *)recognizer)->Result();
}

const char *kwang_recognizer_partial_result(VRecognizer *recognizer)
{
    return ((Recognizer *)recognizer)->PartialResult();
}

const char *kwang_recognizer_final_result(VRecognizer *recognizer)
{
    return ((Recognizer *)recognizer)->FinalResult();
}

