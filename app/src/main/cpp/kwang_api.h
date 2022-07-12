//
// Created by kaicheng.wang on 2022/7/11.
//

#ifndef KALDIANDROID_KWANG_API_H
#define KALDIANDROID_KWANG_API_H


#ifdef __cplusplus
extern "C" {
#endif

typedef struct VModel VModel;
typedef struct VRecognizer VRecognizer;

VModel * kwang_model_new(const char *model_path);
void kwang_model_free(VModel *model);
int kwang_model_find_word(VModel *model, const char *word);

VRecognizer *kwang_recognizer_new(VModel *model);
void kwang_recognizer_free(VRecognizer *recognizer);
void kwang_recognizer_reset(VRecognizer *recognizer);

int kwang_recognizer_accept_waveform(VRecognizer *recognizer, const char *data, int length);
int kwang_recognizer_accept_waveform_s(VRecognizer *recognizer, const short *data, int length);

const char *kwang_recognizer_result(VRecognizer *recognizer);
const char *kwang_recognizer_partial_result(VRecognizer *recognizer);
const char *kwang_recognizer_final_result(VRecognizer *recognizer);

#ifdef __cplusplus
}
#endif
#endif //KALDIANDROID_KWANG_API_H
