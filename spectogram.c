/* file to handle spectogram creation */
//ffmpeg includes
#include <frame.h> 

//fftw includes
#include <fftw3.h>

#include <math.h>
#include <stdbool.h>
#include "spectogram.h"
#include "config.h"

fftw_complex *fft_in=NULL,*fft_out=NULL;
fftw_plan *plan_forward;
/*
Process a single frame and return final output array of log bins
 */
double *process_av_frame(AVFrame *frame){
  fftw_complex *out=NULL;
  int i;
  int *log_frequencies = generate_log_frequencies(5512);
  double *log_bins=NULL;
  double *spec;
  out = fft_av_frame(frame);
  spec = out;
  log_bins = extract_log_bins(spec,log_frequencies);
  decompose_array(log_bins,32);
  return log_bins;
}
 
void init_fft_params(void){
  fftw_plan *plan=NULL;
  fft_in = fftw_malloc(sizeof(fftw_complex) * SAMPLE_PER_SUB_FP);
  if(fft_in == NULL){
    fprintf(stderr,"ERROR: %s Memory not allocated for input\n",__FUNCTION__);
    return -1;
  }
  fft_out = fftw_malloc(sizeof(fftw_complex) * SAMPLE_PER_SUB_FP);
  if(fft_out == NULL){
    fprintf(stderr,"ERROR: %s Memory not allocated for output\n",__FUNCTION__);
    return -1;
  }
  plan = fftw_plan_dft_1d(SAMPLE_PER_SUB_FP, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);
  if(plan == NULL){
    fprintf(stderr,"ERROR: %s fftw_plan failed to return plan\n",__FUNCTION__);
    return -1;
  }
  plan_forward = plan;
}

/*
Gets AVFrame from av_decoder.c
 */
fftw_complex *fft_av_frame(struct AVFrame *frame){
  float *arr,*log_frequencies,*log_bins;
  int i,j=0;
  arr = frame->data[0];
  //stores all samples in real part and makes img part of complex zero
  for(i=0;i<SAMPLE_PER_SUB_FP;i++){
    fft_in[i][0] = arr[i]; 
    fft_in[i][1] = 0;
    // Uncomment line below to dump output for debugging
    // fprintf(stderr,"DEBUG: %s sample:\t%d\t %lf \t %lf\n",__FUNCTION__,fft_in[i][0],fft_in[i][1]);
  }
  fftw_execute(plan_forward);
  return fft_out;
}

/* Originally this function is take from https://github.com/AddictedCS/soundfingerprinting/blob/1b25aad297f78ae59f683fc577a63e7b0ed44e96/src/SoundFingerprinting/FFT/LogUtility.cs
 */
int *generate_log_frequencies(int sample_rate){
  double logMin = log2f(318);
  double logMax = log2f(2000);

  double delta = (logMax - logMin)/32; //32 is number of log bins that we require

  int *indexes,i;
  double accDelta = 0;
  indexes = (int *) malloc(sizeof(int) * 33);
  for (i=0;i <= 32;i++){
    int freq = (int) pow(2,logMin + accDelta);
    accDelta += delta;
    indexes[i] = frequency_to_spectrum_index(freq, sample_rate, 2048);
  }
  return indexes;
}


/* Originally this function is take from https://github.com/AddictedCS/soundfingerprinting/blob/1b25aad297f78ae59f683fc577a63e7b0ed44e96/src/SoundFingerprinting/FFT/LogUtility.cs
 */
int frequency_to_spectrum_index(float frequency, int sampleRate, int spectrumLength)
{
  float fraction = frequency / ((float)sampleRate / 2); // N sampled points in time correspond to [0, N/2] frequency range
  int index = (int)round(((spectrumLength / 2) + 1) * fraction); // DFT N points defines [N/2 + 1] frequency points
  return index;
}

/* Originally taken from https://github.com/AddictedCS/soundfingerprinting/blob/3a9dab0e562126fa1f3754205ff6c0c2d92b83b4/src/SoundFingerprinting/FFT/SpectrumService.cs */
double *extract_log_bins(double *spec,int *log_frequency_index)
{
  int width = 1024; // 2048/2
  double *sum_freq;
  sum_freq = (double *) malloc(sizeof(double) * 32);
  int i,j;
  for(i=0; i < 32; i++){
    int low_bound = log_frequency_index[i];
    int higher_bound = log_frequency_index[i + 1];
    //    fprintf(stderr,"DEBUG: low_bound %d higher_bound %d\n",low_bound,higher_bound);
    sum_freq[i] = 0;
    for(j = low_bound;j < higher_bound; j++){
      double re = spec[2 * j] / width;
      double img = spec[(2 * j) + 1] / width;
      sum_freq[i] += (double) ((re * re) + (img * img));
    }
    sum_freq[i] = sum_freq[i] / (higher_bound - low_bound);
  }
  return sum_freq;
}

/*
Basic Haar wavelet transform
Method: For a single array of size 2^k, wavelet transform is performed as follows:
avg(a,b) := (a+b)/2
diff(a,b) := (a-b)/2
There are total k iterations of process.
Original row is taken as pairs of numbers (2^k-1)
On these pairs avg and diff both are calculated which are written from indices 0 - (k/2) and ((k/2) + 1) - k
This step is repeated k times. 
 */
void decompose_array(double *arr,int len){
  int i,j=len;
  float result[len];
  for(i=0;i<len;i++)
    arr[i] /=sqrt(len);
  while(j > 1){
    j /= 2;
    for(i=0;i<j;i++){
      result[i] = (double)((arr[2 * i] + arr[2 * i + 1])/sqrt(2));
      result[j + i] = (double)((arr[2 * i] - arr[2 * i + 1])/sqrt(2));
    }
  }
  for(i=0;i<2*j;i++){
    arr[i] = result[i];
  }
}

void decompose_image(float **image,int rows,int cols){ //image is array of form image[128][32]
  int row,col;
  float temp[rows];
  for(row=0;row<rows;row++)
    decompose_array(image[row],rows);
  for(col=0;col<cols;col++){
    for(row=0;row<rows;row++)
      temp[row] = image[row][col];
    decompose_array(temp,rows);
    for(row=0;row<rows;row++)
      image[row][col] = temp[row];
  }
}
 
void swap(double *list,int *indexes,int i,int j){
  float tmp = list[i];
  list[i] = list[j];
  list[j] = tmp;

  int indextmp = indexes[i];
  indexes[i] = indexes[j];
  indexes[j] = indextmp;
}

/*
Originally taken from  https://github.com/AddictedCS/soundfingerprinting/blob/1b25aad297f78ae59f683fc577a63e7b0ed44e96/src/SoundFingerprinting/Utils/FastFingerprintDescriptor.cs
 */
int partition(double *list,int *indexes,int pivot_index, int lo, int hi){
  swap(list,indexes,pivot_index,lo);
  float pivot = abs(list[lo]);
  int i = lo + 1,j = lo + 1;
  for(j;j<=hi;j++){
    if(list[j] > pivot){
      swap(list, indexes, i, j);
      i++;
    }
  }

  swap(list, indexes, lo, i - 1);
  return i - 1;
}

/*
Originally taken from https://github.com/AddictedCS/soundfingerprinting/blob/1b25aad297f78ae59f683fc577a63e7b0ed44e96/src/SoundFingerprinting/Utils/FastFingerprintDescriptor.cs
*/
int find(int kth, double *list, int *indexes, int lo, int hi){
  int r = (rand() % (hi - lo)) + 2; // generate a random number between lo to hi - 1
  int pi = partition(list, indexes,r,lo,hi);
  if(pi == kth){
    return pi;
  }

  if(pi > kth){
    return find(kth, list, indexes, lo, pi - 1);
  }

  return find(kth, list, indexes, pi + 1, hi);
}

bool *encode_fingerprint(double *log_bins_array, int *indexes, int top_wavelets){
  bool *result = malloc(sizeof(bool) * 8192);
  int i,index;
  for(i=0;i<top_wavelets;i++){
    index = i;
    double value = log_bins_array[i];
    if(value > 0){
      result[index * 2] = true;
    }
    else if(value < 0){
      result[(index * 2) + 1] = false;
    }
  }
  return result;
}
  
bool *extract_top_wavelets(double *log_bins_array,int top_wavelets/*,bool **result*/){
  int indexes[4096],i;
  for(i=0;i<4096;i++)
    indexes[i] = i;
  find(top_wavelets - 1,log_bins_array, indexes,0,4095);
  bool *result = encode_fingerprint(log_bins_array, indexes, top_wavelets);
  return result;
}
