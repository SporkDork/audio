#include "audio.h"

typedef unsigned char u8;

float smooth_sample(float x) {
	if (x > -0.5 && x < 0.5) return x;
	if (x > 2.0) return 1.0;
	if (x < -2.0) return -1.0;

	float a = -0.5625, b = -0.25, c = 1.25, y; // magic values

	if (x < 0.0) y = -(a / (-x-b) + c);
	else y = a / (x-b) + c;

	return y;
}

int is_valid(audio_t *t) {
	if (!t) return 0;
	return t->buf && t->sz > 0 && t->n_ch > 0 && t->bps > 0 && t->rate > 0 && t->fmt;
}

int create_audio(audio_t *track, int n_ch, int bps, int rate, int fmt, int sz, char *name) {
	if (!track) return -1;
	if (n_ch < 1) return -2;
	if (bps < 1 || bps > 4 && bps != 8) return -3;
	if (rate < 1) return -4;
	if (fmt != 1 || fmt != 3) return -5;
	if (fmt == 1 && bps > 4 || fmt == 3 && bps != 4 && bps != 8) return -6;
	if (sz < 0) return -7;

	track->name = name ? strdup(name) : NULL;
	track->n_ch = n_ch;
	track->bps = bps;
	track->rate = rate;
	track->fmt = fmt;
	track->sz = sz;
	track->buf = calloc(n_ch, sizeof(void*));

	if (sz) {
		int i;
		for (i = 0; i < n_ch; i++) track->buf[i] = calloc(sz, sizeof(float));
	}
	return 0;
}

void transfer_audio(audio_t *dst, audio_t *src) {
	if (!dst || !src) return;

	memcpy(dst, src, sizeof(audio_t));
	dst->buf = calloc(dst->n_ch, sizeof(void*));

	int i;
	for (i = 0; i < dst->n_ch; i++) {
		dst->buf[i] = calloc(dst->sz, sizeof(float));
		memcpy(dst->buf[i], src->buf[i], dst->sz * sizeof(float));
	}

	if (dst->name) dst->name = strdup(dst->name);
	else dst->name = strdup("Untitled");
}

void rename_audio(audio_t *track, char *name) {
	if (!track || !name || !strcmp(name, track->name)) return;

	if (track->name) free(track->name);
	track->name = strdup(name);
}

void free_audio_data(audio_t *track) {
	if (!track) return;
	if (track->buf) {
		int i;
		for (i = 0; i < track->n_ch; i++) {
			if (track->buf[i]) free(track->buf[i]);
			track->buf[i] = NULL;
		}
		free(track->buf);
		track->buf = NULL;
	}
}

void close_audio(audio_t *track) {
	if (!track) return;
	free_audio_data(track);
	if (track->name) free(track->name);
	memset(track, 0, sizeof(audio_t));
}

void debug_header(wav_t *h, FILE *file) {
	if (!h || !file) return;

	char riff_magic[5] = {0}, riff_fmt[5] = {0}, fmt_magic[5] = {0}, data_magic[5] = {0};
	memcpy(&riff_magic[0], &h->riff_magic[0], 4);
	memcpy(&riff_fmt[0], &h->riff_fmt[0], 4);
	memcpy(&fmt_magic[0], &h->fmt_magic[0], 4);
	memcpy(&data_magic[0], &h->data_magic[0], 4);

	fprintf(file, "riff_magic: %s\nriff_size: %d\nriff_fmt: %s\nfmt_magic: %s\n"
			"fmt_size: %d\naudio_fmt: %d\nn_channels: %d\nsample_rate: %d\nbyte_rate: %d\n"
			"block_align: %d\nbits_per_sample: %d\ndata_magic: %s\ndata_size: %d",
			riff_magic, h->riff_size, riff_fmt, fmt_magic,
			h->fmt_size, h->audio_fmt, h->n_channels, h->sample_rate, h->byte_rate,
			h->block_align, h->bits_per_sample, data_magic, h->data_size);
}

double read_sample(void *ptr, int len, int wavfmt) {
	if (!ptr || len < 1 || (wavfmt == 1 && len > 4)) return 0.0;

	double f = 0;
	if (wavfmt == 1) { // Integer PCM
		int s = 0;
		memcpy(&s, ptr, len);

		f = (double)s / (double)((1 << (len * 8 - 1)) - 0.5);
		if (len == 1) f -= 1.0;
		else if (f > 1.0) f -= 2.0;
	}
	if (wavfmt == 3) { // Floating-point
		if (len == 4) memcpy(&f+4, ptr, 4);
		if (len == 8) memcpy(&f, ptr, 8);
	}
	return f;
}

void write_sample(void *ptr, double sample, int len, int wavfmt) {
	if (!ptr || len < 1 || (wavfmt == 1 && len > 4)) return;

	if (wavfmt == 1) { // Integer PCM
		if (len == 1) sample += 1.0;
		else if (sample < 0.0) sample += 2.0;
		sample *= (double)((1 << (len * 8 - 1)) - 0.5);

		int i;
		for (i = 0; i < len; i++) {
			((u8*)ptr)[i] = (u8)(((unsigned int)sample & (0xff << (i * 8))) >> (i * 8));
		}
	}
	if (wavfmt == 3) { // Floating-point
		if (len == 4) memcpy(ptr, &sample+4, 4);
		if (len == 8) memcpy(ptr, &sample, 8);
	}
}

void load_samples(audio_t *track, void *buf, int size) {
	if (!track || !buf || size < 1) return;

	int i, j, p = 0, n_ch = track->n_ch, bps = track->bps;
	track->sz = size / (bps * n_ch);

	free_audio_data(track);
	track->buf = calloc(n_ch, sizeof(void*));
	for (i = 0; i < n_ch; i++) track->buf[i] = calloc(track->sz, sizeof(float));

	for (i = 0; i < track->sz && p < size; i++, p += bps*n_ch) {
		for (j = 0; j < n_ch; j++) {
			track->buf[j][i] = read_sample((u8*)buf + p + bps*j, bps, track->fmt);
		}
	}
}

void save_samples(audio_t *track, void *buf) {
	if (!track || !is_valid(track) || !track->buf || !buf) return;

	int i, j, p = 0, n_ch = track->n_ch, bps = track->bps;
	for (i = 0; i < track->sz; i++, p += n_ch*bps) {
		for (j = 0; j < n_ch; j++) {
			write_sample((u8*)buf + p + bps*j, track->buf[j][i], bps, track->fmt);
		}
	}
}

int load_wav(audio_t *track, char *fname, char *name) {
	if (!track || !fname) return -1;

	FILE *f = fopen(fname, "rb");
	if (!f) {
		printf("Error: could not open \"%s\"\n", fname);
		return -2;
	}

	fseek(f, 0, SEEK_END);
	int sz = ftell(f);
	rewind(f);
	if (sz <= sizeof(wav_t)) {
		printf("Error: \"%s\" is too small to be a WAV file\n", fname);
		return -3;
	}

	u8 *file = malloc(sz);
	fread(file, 1, sz, f);
	fclose(f);

	wav_t header = {0};
	memcpy(&header, file, sizeof(wav_t));
	if (memcmp(header.riff_magic, "RIFF", 4) || memcmp(header.riff_fmt, "WAVE", 4) || memcmp(header.fmt_magic, "fmt ", 4)) {
		printf("Error: \"%s\" is not a valid WAV file\n", fname);
		free(file);
		return -4;
	}

	int off = 0x24;
	while (memcmp(header.data_magic, "data", 4)) {
		off += header.data_size + 8;
		if (off >= sz || off < 0x24) break;
		memcpy(&header.data_magic, file + off, 8);
	}
	if (off >= sz || off < 0x24) {
		printf("Error: could not find data chunk\n");
		free(file);
		return -5;
	}
	off += 8;

	//debug_header(&header);

	int n_ch = header.n_channels, bps = (header.bits_per_sample + 7) / 8;
	if (header.audio_fmt != 1 && header.audio_fmt != 3) {
		free(file);
		return -6;
	}
	if (n_ch < 1) {
		free(file);
		return -7;
	}
	if (bps < 1 || (header.audio_fmt == 1 && bps > 4) || (header.audio_fmt == 3 && bps != 4 && bps != 8)) {
		free(file);
		return -8;
	}

	if (name) track->name = strdup(name);
	track->n_ch = n_ch;
	track->bps = bps;
	track->rate = header.sample_rate;
	track->fmt = header.audio_fmt;
	load_samples(track, file+off, header.data_size);

	return 0;
}

void write_wav(audio_t *track, char *fname) {
	if (!fname || !track || !track->buf || !track->name || track->n_ch < 1 || track->bps < 1 || !track->fmt || track->sz < 1 ||
	    (track->fmt == 3 && track->bps != 4 && track->bps != 8) || (track->fmt != 3 && track->bps > 4)) {
		fprintf(stderr, "Invalid audio track\n");
		return;
	}

	int sz = track->sz * track->n_ch * track->bps;
	wav_t header = {0};

	memcpy(header.riff_magic, "RIFF", 4);
	header.riff_size = sz + 36;
	memcpy(header.riff_fmt, "WAVE", 4);
	memcpy(header.fmt_magic, "fmt ", 4);
	header.fmt_size = 16;
	header.audio_fmt = track->fmt;
	header.n_channels = track->n_ch;
	header.sample_rate = track->rate;
	header.byte_rate = track->rate * track->n_ch * track->bps;
	header.block_align = header.byte_rate / track->rate;
	header.bits_per_sample = track->bps * 8;
	memcpy(header.data_magic, "data", 4);
	header.data_size = sz;

	FILE *f = fopen(fname, "wb");
	if (!f) {
		fprintf(stderr, "Could not create new file\n");
		return;
	}
	fwrite(&header, sizeof(wav_t), 1, f);

	u8 *file = calloc(sz, 1);
	save_samples(track, file);
	fwrite(file, 1, sz, f);

	fclose(f);
	return;
}

// Audio Editing

void amplify_audio(audio_t *track, float factor) {
	if (!track || !track->buf || !is_valid(track) || factor == 1.0) return;

	int i, j;
	for (i = 0; i < track->n_ch; i++) {
		for (j = 0; j < track->sz; j++) {
			if (factor > 1.0) track->buf[i][j] = smooth_sample(track->buf[i][j] * factor);
			else track->buf[i][j] *= factor;
		}
	}
}

void resample_audio(audio_t *track, float factor) {
	if (!track || !track->buf || !track->sz) return;
	if (factor <= 0.0) return;
	if (factor == 1.0) return;

	int i, j, sz = (int)((float)track->sz / factor);
	for (i = 0; i < track->n_ch; i++) {
		float *new_buf = calloc(sz, sizeof(float));

		double pos = 0.0;
		for (j = 0; j < sz; j++) {
			if (pos >= (float)(track->sz-1)) {
				new_buf[j] = track->buf[i][track->sz-1];
				break;
			}

			int p = (int)pos;
			double r = pos - (double)p;
			float x = track->buf[i][p], y = track->buf[i][p+1];

			new_buf[j] = x + (float)r * (y-x);
			pos += (double)factor;
		}
		free(track->buf[i]);
		track->buf[i] = new_buf;
	}

	track->sz = sz;
}

void mix_audio(audio_t *track, int n_ch) {
	if (!track || !track->buf || !track->sz || track->n_ch < 1 || n_ch < 1) return;
	if (n_ch == track->n_ch) return;

	int i, j;
	float **new_buf = calloc(n_ch, sizeof(void*));
	for (i = 0; i < n_ch; i++) new_buf[i] = calloc(track->sz, sizeof(float));

	float pos = 0.0, factor = (float)n_ch / (float)track->n_ch;
	for (i = 0; i < track->n_ch; i++) {
		float f = factor;
		while (f > 0.0) {
			int p = (int)pos;
			float l = 1.0 - (pos - (float)p), v = f;
			if (l < f) {
				v = l;
				pos += l;
				f -= l;
			}
			else {
				pos += f;
				f = 0.0;
			}
			for (j = 0; j < track->sz; j++) new_buf[p][j] += track->buf[i][j] * v;
		}
		free(track->buf[i]);
		track->buf[i] = NULL;
	}
	free(track->buf);
	track->buf = new_buf;
	track->n_ch = n_ch;
}

void reverse_audio(audio_t *track) {
	if (!track || !is_valid(track)) return;

	float *buf = calloc(track->sz, sizeof(float));
	int i, j;
	for (i = 0; i < track->n_ch; i++) {
		memcpy(buf, track->buf[i], track->sz * sizeof(float));
		for (j = 0; j < track->sz; j++) track->buf[i][j] = buf[track->sz-j-1];
	}
	free(buf);
}

void resize_audio(audio_t *track, int sz) {
	if (!track || sz < 1) return;
	if (!track->buf) {
		if (track->n_ch < 1) return;
		track->buf = calloc(track->n_ch, sizeof(void*));
	}

	int i;
	for (i = 0; i < track->n_ch; i++) {
		track->buf[i] = realloc(track->buf[i], sz * sizeof(float));
		if (sz > track->sz) memset(track->buf[i] + track->sz, 0, (sz - track->sz) * sizeof(float));
	}
	track->sz = sz;
}

void remove_audio(audio_t *track, int offset, int size) {
	if (!track || !is_valid(track) || offset < 0 || offset >= track->sz || !size) return;
	if (size < 0 || size > track->sz) size = track->sz;
	if (offset+size > track->sz) size = track->sz - offset;

	int i, j, p, sz = track->sz - size;
	for (i = 0; i < track->n_ch; i++) {
		float *buf = calloc(sz, sizeof(float));
		for (j = 0, p = 0; j < sz; j++, p++) {
			if (j == offset) p += size;
			buf[j] = track->buf[i][p];
		}
		free(track->buf[i]);
		track->buf[i] = buf;
	}
	track->sz = sz;
}

void apply_audio(audio_t *dst, audio_t *src, int offset, int size, float amplitude, int insert) {
	if (!dst || !is_valid(src) || size < 1) return;

	if (dst->n_ch < 1) dst->n_ch = src->n_ch;
	if (dst->bps < 1) dst->bps = src->bps;
	if (dst->rate < 1) dst->rate = src->rate;
	if (!dst->fmt) dst->fmt = src->fmt;

	int i, j, alt = 1;
	audio_t track = {0};
	if (src) {
		char *name = track.name;
		memcpy(&track, src, sizeof(audio_t));
		if (name) track.name = name;
		else track.name = strdup(track.name);

		if (src->rate != dst->rate || src->n_ch != dst->n_ch || dst == src || !name) {
			track.buf = calloc(track.n_ch, sizeof(void*));
			for (i = 0; i < track.n_ch; i++) {
				track.buf[i] = malloc(track.sz * sizeof(float));
				memcpy(track.buf[i], src->buf[i], track.sz * sizeof(float));
			}

			resample_audio(&track, (float)dst->rate / (float)track.rate);
			track.rate = dst->rate;

			mix_audio(&track, dst->n_ch);
		}
		else alt = 0;
	}
	else {
		memcpy(&track, dst, sizeof(audio_t));
		track.buf = calloc(track.n_ch, sizeof(void*));
	}
	track.name = NULL;

	if (size > 0 && size != track.sz) {
		for (i = 0; i < track.n_ch; i++) {
			track.buf[i] = realloc(track.buf[i], size * sizeof(void*));
			if (size > track.sz) memset(track.buf[i] + track.sz, 0, (size - track.sz) * sizeof(float));
		}
		track.sz = size;
		alt = 1;
	}

	if (!dst->buf) {
		dst->buf = calloc(dst->n_ch, sizeof(void*));
		dst->sz = 0;
	}

	int sz = 0;
	for (i = 0; i < dst->n_ch; i++) {
		sz = dst->sz;
		int off = offset;
		if (off < 0) {
			off = -off;
			if (insert && off < track.sz) off = track.sz;
			dst->buf[i] = realloc(dst->buf[i], (sz + off) * sizeof(float));
			memmove(dst->buf[i] + off, dst->buf[i], sz * sizeof(float));
			if (insert) {
				memset(dst->buf[i], 0, off * sizeof(float));
				memcpy(dst->buf[i], track.buf[i], track.sz * sizeof(float));
				continue;
			}
			memset(dst->buf[i], 0, off * sizeof(float));
			sz += off;
			off = 0;
		}

		if (insert) {
			int move = 0;
			if (off > dst->sz) move = off - dst->sz;
			dst->buf[i] = realloc(dst->buf[i], (dst->sz + move + track.sz) * sizeof(float));

			if (off < dst->sz) memmove(dst->buf[i] + off+track.sz, dst->buf[i] + off, (dst->sz - off) * sizeof(float));
			else memset(dst->buf[i] + dst->sz, 0, (move + track.sz) * sizeof(float));

			memcpy(dst->buf[i] + off, track.buf[i], track.sz * sizeof(float));
		}
		else {
			if (off+track.sz > sz) {
				dst->buf[i] = realloc(dst->buf[i], (off + track.sz) * sizeof(float));
				memset(dst->buf[i] + sz, 0, ((off + track.sz) - sz) * sizeof(float));
				sz = off + track.sz;
			}

			for (j = 0; j < track.sz && j < sz - off; j++) {
				if (track.buf[i][j] == 0.0 && amplitude <= 1.0) dst->buf[i][off+j] = track.buf[i][j] * amplitude;
				else dst->buf[i][off+j] = smooth_sample(dst->buf[i][off+j] + track.buf[i][j] * amplitude);
			}
		}
	}
	if (insert) dst->sz += track.sz;
	else dst->sz = sz;

	if (alt) close_audio(&track);
}

void add_audio(audio_t *dst, audio_t *src, int offset, int size, float amplitude) {
	apply_audio(dst, src, offset, size, amplitude, 0);
}

void insert_audio(audio_t *dst, audio_t *src, int offset, int size, float amplitude) {
	apply_audio(dst, src, offset, size, amplitude, 1);
}

void replace_channel(audio_t *dst, audio_t *src, int dst_ch, int src_ch) {
	if (!dst || !is_valid(src) || dst_ch < 0 || src_ch < 0 ||
	   (is_valid(dst) && dst_ch >= dst->n_ch) ||
	   (!is_valid(dst) && dst_ch > 0) ||
	    src_ch >= src->n_ch) return;

	if (dst->n_ch < 1) dst->n_ch = 1;
	if (dst->bps < 1) dst->bps = src->bps;
	if (dst->rate < 1) dst->rate = src->rate;
	if (!dst->fmt) dst->fmt = src->fmt;
	if (dst->sz < 1) dst->sz = src->sz;

	if (!dst->buf) {
		dst->buf = calloc(1, sizeof(void*));
		dst->n_ch = 1;
		dst_ch = 0;
		if (src->sz > dst->sz) dst->sz = src->sz;
		dst->buf[dst_ch] = calloc(dst->sz, sizeof(float));
	}
	else {
		if (src->sz > dst->sz) resize_audio(dst, src->sz);
		if (!dst->buf[dst_ch]) dst->buf[dst_ch] = calloc(dst->sz, sizeof(float));
		memset(dst->buf[dst_ch], 0, dst->sz * sizeof(float));
	}
	memcpy(dst->buf[dst_ch], src->buf[src_ch], dst->sz);
}

void fcopy(float *dst, float *src, int dst_sz, int src_sz) {
	int i;
	for (i = 0; i < dst_sz; i++) {
		if (i < src_sz) dst[i] = src[i];
		else dst[i] = 0.0;
	}
}

void insert_channel(audio_t *dst, audio_t *src, int dst_ch, int src_ch) {
	if (!dst || !src || !is_valid(src) || src_ch < 0 || src_ch >= src->n_ch) return;

	int i;
	if (dst->n_ch < 1) dst->n_ch = 1;
	if (dst->bps < 1) dst->bps = src->bps;
	if (dst->rate < 1) dst->rate = src->rate;
	if (!dst->fmt) dst->fmt = src->fmt;
	if (dst->sz < 1) dst->sz = src->sz;
	if (!dst->buf) dst->buf = calloc(dst->n_ch, sizeof(void*));

	int add;
	if (src->sz > dst->sz) resize_audio(dst, src->sz);

	// insert the new channel followed by (-dst_ch - 1) blank channels before the start of the channel list in 'dst'
	if (dst_ch < 0) {
		add = -dst_ch;
		dst->n_ch += add;
		dst->buf = realloc(dst->buf, dst->n_ch * sizeof(void*));

		for (i = dst->n_ch-1; i >= add; i--) dst->buf[i] = dst->buf[i-add];
		for (i = 1; i < add; i++) dst->buf[i] = calloc(dst->sz, sizeof(float));

		fcopy(dst->buf[0], src->buf[src_ch], dst->sz, src->sz);
	}

	// append (dst_ch - dst->n_ch) blank channels followed by the new channel to the channel list in 'dst'
	else if (dst_ch >= dst->n_ch) {
		add = dst_ch - dst->n_ch + 1;
		dst->n_ch += add;
		dst->buf = realloc(dst->buf, dst->n_ch * sizeof(void*));

		for (i = 0; i < dst->n_ch-add; i++) dst->buf[i] = dst->buf[i+add];
		for (i = dst->n_ch-add; i < dst->n_ch-1; i++) dst->buf[i] = calloc(dst->sz, sizeof(float));

		fcopy(dst->buf[dst->n_ch-1], src->buf[src_ch], dst->sz, src->sz);
	}

	// insert the new channel before the channel indexed by 'dst_ch' in the channel list in 'dst'
	else {
		if (dst->buf[dst_ch]) {
			dst->buf = realloc(dst->buf, ++dst->n_ch * sizeof(void*));
			for (i = dst->n_ch-1; i >= dst_ch+1; i--) dst->buf[i] = dst->buf[i-1];
		}
		dst->buf[dst_ch] = calloc(dst->sz, sizeof(float));
		fcopy(dst->buf[dst_ch], src->buf[src_ch], dst->sz, src->sz);
	}
}

void remove_channel(audio_t *track, int ch) {
	if (!track || !is_valid(track) || ch < 0 || ch >= track->n_ch) return;
	free(track->buf[ch]);

	int i;
	for (i = ch; i < track->n_ch; i++) track->buf[i] = track->buf[i+1];

	track->buf[--track->n_ch] = NULL;
}
