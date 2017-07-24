#ifndef SPECTOGRAM_H
#define SPECTOGRAM_H
#endif

#include<fftw3.h>
#include<frame.h>
#include<stdbool.h>
#include "config.h"


void init_fft_params(void); 
fftw_complex *fft_av_frame(struct AVFrame *frame);
double *extract_log_bins(double *spec,int *log_frequency_index);
int frequency_to_spectrum_index(float frequency, int sampleRate, int spectrumLength);
int *generate_log_frequencies(int sample_rate);
double *process_av_frame(AVFrame *frame);
bool *extract_top_wavelets(double *log_bins_array,int top_wavelets/*,bool **result*/);



