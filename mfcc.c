#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

float f2m(float f) { return 2595.0 * log10(1 + f / 700.0); }
float m2f(float m) { return 700.0 * (pow(10, m / 2595.0) - 1); }

float mfcc_filter(float *in, int f[28], int n)
{
    float sum = 0, d1, d2;
    int   i;
    d1 = f[n] - f[n - 1];
    d2 = f[n + 1] - f[n];
//  printf("n: %d, %d, %d, d1: %f\n", n, f[n-1], f[n], d1);
//  printf("n: %d, %d, %d, d2: %f\n", n, f[n], f[n+1], d2);
    for (i=f[n-1]; i< f[n  ]; i++) sum += in[i] * (i - f[n - 1]) / d1;
    for (i=f[n  ]; i<=f[n+1]; i++) sum += in[i] * (f[n + 1] - i) / d2;
    return sum;
}

#if 1
static float c(int u)
{
    if (u == 0) return (float)sqrt(1.0f / 26.0f);
    else        return (float)sqrt(2.0f / 26.0f);
}

static void dct_init_matrix(float matrix[26][26])
{
    int u, x;
    for (u=0; u<26; u++) {
        for (x=0; x<26; x++) {
            matrix[u][x] = (float)(c(u) * cos((x + 0.5f) * u * M_PI / 26.0f));
        }
    }
}

static float dct(float data[26], float matrix[26][26], int n)
{
    float sum = 0;
    int   i;
    for (i=0; i<26; i++) sum += data[i] * matrix[n][i];
    return sum;
}
#endif

void mfcc(float in[256 * 2], float out[26])
{
    static int   s_inited = 0;
    static int   s_mfcc_f[28];
    static float s_dct_mat[26][26];
    if (!s_inited) {
        float mel_end  = f2m(4000);
        float mel_width= mel_end / 27;
        int   i;
        for (i=0; i<=27; i++) {
            s_mfcc_f[i] = i * mel_width;
            s_mfcc_f[i] = m2f(s_mfcc_f[i]);
            s_mfcc_f[i] = s_mfcc_f[i] * 512 / 8000;
            printf("s_mfcc_f[%d]: %d\n", i, s_mfcc_f[i]);
        }
        dct_init_matrix(s_dct_mat);
        s_inited = 1;
    }
    
    float power[256];
    float temp [26 ];
    int   i;
    for (i=0; i<256; i++) power[i] = (in[i * 2 + 0] * in[i * 2 + 0] + in[i * 2 + 1] * in[i * 2 + 1]) / 512;
//  for (i=0; i<26 ; i++) temp [i] = log(mfcc_filter(power, s_mfcc_f, i + 1));
//  for (i=0; i<12 ; i++) out  [i] = dct(temp, s_dct_mat, i + 1);
    for (i=0; i<26 ; i++) out  [i] = log(mfcc_filter(power, s_mfcc_f, i + 1));
}
