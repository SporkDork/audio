#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	char riff_magic[4];
	int riff_size;
	char riff_fmt[4];
	char fmt_magic[4];
	int fmt_size;
	short audio_fmt;
	short n_channels;
	int sample_rate;
	int byte_rate;
	short block_align;
	short bits_per_sample;
	char data_magic[4];
	int data_size;
} wav_t;

typedef struct {
	char *name;  // File name
	int n_ch;    // Number of channels
	int bps;     // Bytes per sample
	int rate;    // Sample rate
	int fmt;     // WAV format. 1 = Integer PCM, 3 = Floating-point. Other values are not supported.
	float **buf; // An array of sample buffers, one for each channel
	int sz;      // Length in samples
} audio_t;

// Custom Clipping Reduction
float smooth_sample(float x);

// audio_t Constructor
int create_audio(audio_t *track, int n_ch, int bps, int rate, int fmt, int sz, char *name);

// Create a copy of an existing audio track so that is has no reference to the original
void transfer_audio(audio_t *dst, audio_t *src);

void rename_audio(audio_t *track, char *name);

// audio_t Destructors
void free_audio_data(audio_t *track); // frees all memory containing audio channel data
void close_audio(audio_t *track); // frees and resets all memory and variables in an audio_t struct

// Debug WAV Header Information
void debug_header(wav_t *h, FILE *file);

// I/O functions
double read_sample(void *ptr, int len, int wavfmt);
void write_sample(void *ptr, double sample, int len, int wavfmt);
void load_samples(audio_t *track, void *buf, int size);
void save_samples(audio_t *track, void *buf);
int load_wav(audio_t *track, char *fname, char *name);
void write_wav(audio_t *track, char *fname);

// Audio Effects
void amplify_audio(audio_t *track, float factor);
void resample_audio(audio_t *track, float factor);
//void timescale_audio(audio_t *track, float factor, int frame_size); // TODO
void mix_audio(audio_t *track, int n_ch);
void reverse_audio(audio_t *track);

// Audio Data Manipulation
void resize_audio(audio_t *track, int sz);
void remove_audio(audio_t *track, int offset, int size);
void add_audio(audio_t *dst, audio_t *src, int offset, int size, float amplitude);
void insert_audio(audio_t *dst, audio_t *src, int offset, int size, float amplitude);

// Audio Channel Manipulation
void replace_channel(audio_t *dst, audio_t *src, int dst_ch, int src_ch);
void insert_channel(audio_t *dst, audio_t *src, int dst_ch, int src_ch);
void remove_channel(audio_t *track, int ch);

#endif
