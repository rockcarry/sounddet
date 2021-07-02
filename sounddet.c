#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "fft.h"
#include "sounddet.h"

#define ATTACH_THRESHOLD  700
#define DETACH_THRESHOLD  500
#define ATTACH_COUNTER    2
#define DETACH_COUNTER    8
#define DET_FREQ_START    100
#define DET_FREQ_END      2500
#define SAMPLE_RATE       8000
#define FFT_LEN           512
#define AMP_THRES_FACTOR  128
#define FREQ_TO_IDX(freq) ((freq) * (FFT_LEN) / (SAMPLE_RATE))

typedef struct {
    uint32_t flags;
    int      counter;
    uint32_t state_cur;
    uint32_t state_last;

    #define RINGBUF_SIZE   8
    #define ITEM_FREQ_NUM  32
    int      ringbuf_freq[RINGBUF_SIZE][ITEM_FREQ_NUM];
    int      ringbuf_idx;

    int      freq_different;
    int      freq_total_num;
    int      freq_avg_dist;
    int      freq_min;
    int      freq_max;
    void    *fft;
} SOUNDDET;

void* sounddet_init(int samprate, int type)
{
    SOUNDDET *det = calloc(1, sizeof(SOUNDDET));
    if (!det) return NULL;
    det->flags = type;
    det->fft   = fft_init(FFT_LEN, 0);
    return det;
}

void sounddet_free(void *ctxt)
{
    SOUNDDET *det = (SOUNDDET*)ctxt;
    if (ctxt) {
        fft_free(det->fft);
        free(ctxt);
    }
}

static void handle_freq_buf(int freq_buf[RINGBUF_SIZE][ITEM_FREQ_NUM], int *num_different, int *num_total, int *avg_dist)
{
    uint8_t freq_points[DET_FREQ_END] = {0};
    int     freq_different = 0, freq_total_num = 0, freq_avg_dist = 0, freqmin = DET_FREQ_END, freqmax = 0, freqcur, i, j;

    for (i=0; i<RINGBUF_SIZE; i++) {
        for (j=0; j<ITEM_FREQ_NUM; j++) {
            if (freq_buf[i][j] > 0 && freq_buf[i][j] < DET_FREQ_END) {
                if (!freq_points[freq_buf[i][j]]) freq_different++;
                if (1                           ) freq_total_num++;
                freq_points[freq_buf[i][j]]++;
                if (freqmin > freq_buf[i][j]) freqmin = freq_buf[i][j];
                if (freqmax < freq_buf[i][j]) freqmax = freq_buf[i][j];
            }
        }
    }

    for (freqcur = freqmin, i = freqmin + 1; i <= freqmax; i++) {
        if (freq_points[i]) {
            freq_avg_dist += i - freqcur;
            freqcur = i;
        }
    }
    freq_avg_dist /= freq_different;
    *num_different = freq_different;
    *num_total     = freq_total_num;
    *avg_dist      = freq_avg_dist ;
}

uint32_t sounddet_run(void *ctxt, int16_t *pcm, int n)
{
    int       val = 0, i, j;
    SOUNDDET *det = (SOUNDDET*)ctxt;
    if (!det) return 0;

    for (i=0; i<n; i++) val += abs(pcm[i]);
    val /= n;

    det->state_last = det->state_cur;
    switch (det->state_last) {
    case 0:
        if (val > ATTACH_THRESHOLD) {
            if (det->counter < ATTACH_COUNTER) det->counter++;
            else { det->state_cur = 1; det->counter = 0; }
        } else det->counter = 0;
        break;
    case 1:
        if (val < DETACH_THRESHOLD) {
            if (det->counter < DETACH_COUNTER) det->counter++;
            else { det->state_cur = 0; det->counter = 0; }
        } else det->counter = 0;
        break;
    }

    if (!det->state_cur && det->state_cur != det->state_last) {
        memset(det->ringbuf_freq, 0, sizeof(det->ringbuf_freq));
    }

    if (det->state_cur) {
        float data[FFT_LEN * 2];
        for (i=0; i<FFT_LEN; i++) { data[i * 2 + 0] = pcm[i]; data[i * 2 + 1] = 0; }
        fft_execute(det->fft, data, data);

        for (j = 0, i = FREQ_TO_IDX(DET_FREQ_START); j < ITEM_FREQ_NUM && i <= FREQ_TO_IDX(DET_FREQ_END); i++) {
            float amp = sqrt(data[i * 2 + 0] * data[i * 2 + 0] + data[i * 2 + 1] * data[i * 2 + 1]);
            if (amp > val * AMP_THRES_FACTOR) {
                int freq = SAMPLE_RATE * i / FFT_LEN;
                det->ringbuf_freq[det->ringbuf_idx][j++] = freq;
                printf("%4d ", freq);
            }
        }
        while (j<ITEM_FREQ_NUM) det->ringbuf_freq[det->ringbuf_idx][j++] = 0;
        det->ringbuf_idx++; det->ringbuf_idx %= 8;
        printf("\n\n"); fflush(stdout);
        
        if (1) {
            int num_different, num_total, avg_dist;
            handle_freq_buf(det->ringbuf_freq, &num_different, &num_total, &avg_dist);
            printf("num_different: %d\n", num_different);
            printf("num_total    : %d\n", num_total);
            printf("avg_dist     : %d\n", avg_dist );
            fflush(stdout);
        }
    }

    return ((det->state_cur ^ det->state_last) << 16) | (det->state_cur & 0xFFFF);
}

#ifdef _TEST_
#include <string.h>
#include "wavdev.h"

static void sounddet_wavin_callback(void *wavdev, void *cbctxt, void *buf, int len)
{
    uint32_t ret = sounddet_run(cbctxt, buf, len / sizeof(int16_t));
    if (ret >> 16) {
        printf("%s %s %s\n",
            (ret & SOUNDDET_TYPE_SOUND) ? "sound" : "-----",
            (ret & SOUNDDET_TYPE_VOICE) ? "voice" : "-----",
            (ret & SOUNDDET_TYPE_BBCRY) ? "bbcry" : "-----");
        fflush(stdout);
    }
    if (1 || (ret & SOUNDDET_TYPE_SOUND)) wavdev_play(wavdev, buf, len);
}

int main(void)
{
    void *sdet = sounddet_init(SAMPLE_RATE, SOUNDDET_TYPE_SOUND|SOUNDDET_TYPE_VOICE|SOUNDDET_TYPE_BBCRY);
    void *wdev = wavdev_init  (SAMPLE_RATE , 1 , FFT_LEN , 3 , sounddet_wavin_callback , sdet, SAMPLE_RATE, 1, FFT_LEN, 3, NULL, NULL);

    wavdev_record(wdev, 1);
    while (1) {
        char cmd[256]; scanf("%256s", cmd);
        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) break;
    }
    wavdev_record(wdev, 0);

    wavdev_exit  (wdev);
    sounddet_free(sdet);
    return 0;
}
#endif
