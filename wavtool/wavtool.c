#include "../audio.h"

#define MAX_ARGS 5

typedef unsigned char u8;
typedef unsigned int u32;
typedef void (*command)(char **);

typedef struct {
	char name[16];
	int index;
} cmd_t;

cmd_t cmds[] = {
	{"quit", 0}, {"exit", 0},
	{"help", 1},
	{"list", 2},
	{"info", 3},
	{"open", 4}, {"load", 4},
	{"openraw", 5}, {"loadraw", 5},
	{"save", 6}, {"write", 6},
	{"saveraw", 7}, {"writeraw", 7},
	{"transfer", 8}, {"t", 8},
	{"generate", 9}, {"g", 9},
	{"mix", 10},
	{"bps", 11},
	{"rate", 12},
	{"format", 13}, {"fmt", 13},
	{"speed", 14},
	{"volume", 15}, {"amp", 15},
	{"get", 16},
	{"set", 17},
	{"show", 18}, {"display", 18},
	{"insert", 19},
	{"add", 20},
	{"remove", 21},
	{"reverse", 22},
	{"insertchannel", 23}, {"ic", 23},
	{"deletechannel", 24}, {"dc", 24}
};
int n_cmd_names = sizeof(cmds) / sizeof(cmd_t);

audio_t **tracks = NULL;
int n_tracks = 0;

const char *help_str[] = {
	"List of commands:",

	"    quit/exit\n"
	"        exit the program\n",	

	"    help [command]\n"
	"        print the usage of [command]\n"
	"        if [command] is not given, the entire command list is printed instead\n",

	"    list\n"
	"        list all tracks\n",

	"    info <track>\n"
	"        display the audio information of <track>\n",

	"    open/load <track> <file>\n"
	"        load the WAV from <file> into <track>\n",

	"    openraw/loadraw <track> <file>\n"
	"        load raw sample data from <file> into <track>\n",

	"    save/write <track> <file>\n"
	"        save contents of <track> as a WAV file\n",

	"    saveraw/writeraw <track> <file>\n"
	"        write raw sample data from <track> to <file>\n",

	"    transfer/t <dest track> <source track>\n"
	"        copy contents of <source track> to <dest track>\n"
	"        <dest track> doesn't need to be defined beforehand\n",

	"    generate/g <track> <size>\n"
	"        add <size> number of samples to the end of <track>\n"
	"        if <track> doesn't exist, you will be prompted for a list of properties\n",

	"    mix <track> <new number of samples>\n"
	"        update the number of channels of <track>\n",

	"    bps <track> <new byte count per sample>\n"
	"        change number of bytes per sample\n",

	"    rate <track> <new sample rate>\n"
	"        resample the contents of <track> to <new sample rate>\n",

	"    format/fmt <track> <new format type>\n"
	"        change the sample format of <track> to <new format type>\n"
	"        <new format type> can be:\n"
	"            \"int\" or \"1\"   - fixed-point sample format\n"
	"            \"float\" or \"3\" - floating-point sample format\n",

	"    speed <track> <multiplier>\n"
	"        multiply the speed (pitch & tempo) of <track> by the factor <multiplier>\n",

	"    volume/amp <track> <multiplier>\n"
	"        multiply the amplitude of <track> by <multiplier>\n",

	"    get <track> <channel index> <sample index>\n"
	"        print the sample at position <sample index> in channel <channel index>\n",

	"    set <track> <channel index> <sample index> <new sample value>\n"
	"        set the sample at position <sample index> in channel <channel index>\n"
	"        to <new sample value>\n",

	"    display/show <track> <sample offset> [zoom level] [channel index]\n"
	"        display samples from <track> from <sample offset>.\n"
	"        if [zoom level] is not set, it will default to 1.0.\n"
	"        if [channel index] is set, display only that channel,\n"
	"        if [channel index] is not set, display all channels\n",

	"    insert <dest track> <src track> <dest sample offset> [size]\n"
	"        insert [size] samples from <src track> into <dest track>\n"
	"        at <dest sample offset>\n",

	"    add <dest track> <src track> <dest sample offset> [size]\n"
	"        same as the insert command except it adds the extracted contents\n"
	"        to <dest track> at the point <dest sample offset>\n",

	"    remove <track> <sample offset> [size]\n"
	"        removes [size] samples from the point <sample offset>\n"
	"        if [size] is not set, everything from <sample offset> on is removed\n",

	"    reverse <track>\n"
	"        reverses the order in which the samples are arranged in <track>\n",

	"    insertchannel/ic <dest track> <src track>\n"
	"      <dest channel index> <src channel index>\n"
	"        takes the channel <src channel index> from <src track> and\n"
	"        inserts the channel into <dest track> at <dest channel index>\n"
	"        <dest track> doesn't need to be defined beforehand\n",

	"    deletechannel/dc <track> <channel index>\n"
	"        delete the channel <channel index> from the list of\n"
	"        channels in <track>\n"
};

void printff(const char *msg) {
	printf("%s", msg);
	fflush(stdout);
}

void debug_ptr(const char *name, void *ptr) {
	printf("%s: %p\n", name, ptr);
	fflush(stdout);
}

void sprintt(char *str, float t) {
	float f = 86400.0;
	int p = 0, s = 0;
	if (t > f) {
		int a = t / f;
		p += sprintf(str, "%d:", a);
		t -= a * f;
		s = 1;
	}
	f /= 24.0;
	while (f > 1.0) {
		if (t > f) {
			int a = t / f;
			if (s) p += sprintf(str+p, "%02d:", a);
			else p += sprintf(str+p, "%d:", a);
			t -= a * f;
			s = 1;
		}
		f /= 60.0;
	}
	if (s) sprintf(str+p, "%02.2f", t);
	else sprintf(str+p, "%.2fs", t);
}

int find_var(char *str, int verbose) {
	if (!str) return -1;
	if (!tracks || n_tracks < 1) {
		if (verbose) {
			printf("No tracks have currently been loaded\n"
			       "Use the \"load\" command to load a WAV file into a variable\n");
		}
		return -2;
	}
	int i;
	for (i = 0; i < n_tracks; i++) {
		if (!strcmp(tracks[i]->name, str)) return i;
	}
	if (verbose) printf("Error: undefined variable \"%s\"\n", str);
	return -3;
}

void prompt(char *str, char *msg) {
	memset(str, 0, 80);
	printf("%s", msg);
	fgets(str, 80, stdin);
}

int read_number(char *str) {
	if (!str) return 0;
	if (str[0] == '0' && str[1] == 'x') return strtol(str, NULL, 16);
	return atoi(str);
}

int enough_args(char **args, int n_args) {
	if (!args || n_args < 0) return 0;
	int i;
	for (i = 0; i < n_args+1; i++) {
		if (!args[i]) {
			printf("Error: insufficient arguments\n");
			return 0;
		}
	}
	return 1;
}

int find_cmd(char *name) {
	if (!name) {
		printf("No command given\n");
		return -2;
	}
	int i, cid = -1;
	for (i = 0; i < n_cmd_names; i++) {
		if (!strcmp(name, cmds[i].name)) {
			cid = cmds[i].index;
			break;
		}
	}
	if (cid < 0) {
		printf("Unrecognised command \"%s\"\n", name);
	}
	return cid;
}

void help(char **args) {
	if (args[1]) {
		int cid = find_cmd(args[1]);
		if (cid < 0) return;
		puts(help_str[cid+1]);
	}
	else {
		int i, n_cmds = cmds[n_cmd_names-1].index + 1;
		for (i = 0; i < n_cmds+1; i++) puts(help_str[i]);
	}
}

void list(char **args) {
	if (n_tracks < 1) {
		printf("No tracks have currently been loaded\n");
	}
	else {
		int i;
		for (i = 0; i < n_tracks; i++) {
			printf("    %s\n", tracks[i]->name);
		}
	}
}

void info(char **args) {
	if (!enough_args(args, 1)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	int n_ch = tracks[idx]->n_ch, bps = tracks[idx]->bps, rate = tracks[idx]->rate,
		sz = tracks[idx]->sz, fmt = tracks[idx]->fmt;

	char fmt_str[20];
	if (fmt == 1) strcpy(fmt_str, "Integer PCM");
	else if (fmt == 3) strcpy(fmt_str, "Floating-point");
	else sprintf(fmt_str, "Unknown (%d)", fmt);

	char time_str[20] = {0};
	sprintt(time_str, (float)sz / (float)rate);

	printf("    Number of Channels: %d\n"
		"    Bytes per Sample: %d\n"
		"    Sample Rate: %d\n"
		"    Sample Format: %s\n"
		"    Number of Samples: %d (%s)\n",
		n_ch, bps, rate, fmt_str, sz, time_str);
}

void add_track(audio_t *t, char *name) {
	int idx = find_var(name, 0);
	if (idx < 0) {
		tracks = realloc(tracks, ++n_tracks * sizeof(audio_t));
		idx = n_tracks-1;
		tracks[idx] = calloc(1, sizeof(audio_t));
	}
	else {
		close_audio(tracks[idx]);
	}
	transfer_audio(tracks[idx], t);
	rename_audio(tracks[idx], name);
}

void open_wav(char **args) {
	if (!enough_args(args, 2)) return;

	audio_t temp = {0};
	int r = load_wav(&temp, args[2], args[1]);
	if (r < 0) {
		printf("Failed to load WAV file (%d)\n", r);
		return;
	}

	add_track(&temp, temp.name);
	close_audio(&temp);
}

int prompt_properties(audio_t *t) {
	if (!t) return -1;
	
	char query[80] = {0};
	prompt(query, "Number of channels: ");
	int n_ch = atoi(query);
	if (n_ch < 1) {
		printf("Error: must be greater than 0\n");
		return -2;
	}

	prompt(query, "Bytes per sample: ");
	int bps = atoi(query);
	if (bps < 1 || (bps > 4 && bps != 8)) {
		printf("Error: if PCM, must be from 1-4. If floating-point, must be 4 or 8\n");
		return -3;
	}

	prompt(query, "Sample rate: ");
	int rate = atoi(query);
	if (rate < 1) {
		printf("Error: must be greater than 0\n");
		return -4;
	}

	prompt(query, "Sample format (if unsure type \"int\"): ");
	char *ptr = query;
	if (query[0] == '\"') ptr += 1;

	int fmt = 0;
	if (!strncmp(ptr, "int", 3) || !strncmp(ptr, "1", 1)) {
		if (bps > 4) {
			printf("Error: bytes per sample (%d) can only be from 1-4\n", bps);
			return -5;
		}
		fmt = 1;
	}
	else if (!strncmp(ptr, "float", 5) || !strncmp(ptr, "3", 1)) {
		if (bps != 4 && bps != 8) {
			printf("Error: bytes per sample (%d) must be 4 or 8\n", bps);
			return -6;
		}
		fmt = 3;
	}
	else {
		printf("Unrecognised sample format\n");
		return -7;
	}

	t->n_ch = n_ch;
	t->bps = bps;
	t->rate = rate;
	t->fmt = fmt;
	return 0;
}

void open_raw(char **args) {
	if (!enough_args(args, 2)) return;

	audio_t temp = {0};
	FILE *f = fopen(args[2], "rb");
	if (!f) {
		printf("Error: could not open \"%s\"\n", args[2]);
		return;
	}

	fseek(f, 0, SEEK_END);
	int sz = ftell(f);
	rewind(f);
	if (sz < 1) {
		printf("Error: \"%s\" is an empty file\n", args[2]);
		return;
	}

	if (prompt_properties(&temp) < 0) return;

	unsigned char *file = malloc(sz);
	fread(file, 1, sz, f);

	temp.name = strdup(args[1]);
	temp.sz = sz / (temp.bps * temp.n_ch);
	temp.buf = calloc(temp.n_ch, sizeof(void*));

	load_samples(&temp, file, sz);
	free(file);

	add_track(&temp, temp.name);
	close_audio(&temp);
}

void transfer(char **args) {
	if (!enough_args(args, 2)) return;

	int idx2 = find_var(args[2], 1);
	if (idx2 < 0) return;

	add_track(tracks[idx2], args[1]);
}

void save_wav(char **args) {
	if (!enough_args(args, 2)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	write_wav(tracks[idx], args[2]);
}

void save_raw(char **args) {
	if (!enough_args(args, 2)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	int sz = tracks[idx]->sz, n_ch = tracks[idx]->n_ch, bps = tracks[idx]->bps, fmt = tracks[idx]->fmt;
	int p = 0, s = n_ch * bps * sz;
	u8 *file = calloc(s, 1);
	save_samples(tracks[idx], file);

	FILE *f = fopen(args[2], "wb");
	if (!f) {
		printf("Could not create \"%s\"\n", tracks[idx]->name);
		free(file);
		return;
	}
	fwrite(file, 1, s, f);
	free(file);
	fclose(f);
}

void generate(char **args) {
	if (!enough_args(args, 2)) return;

	audio_t temp = {0};

	int idx = find_var(args[1], 0);
	if (idx < 0) {
		if (!prompt_properties(&temp) < 0) return;
	}
	else {
		transfer_audio(&temp, tracks[idx]);
	}

	int size = atoi(args[2]);
	resize_audio(&temp, temp.sz + size);
	add_track(&temp, args[1]);
}

void mix(char **args) {
	if (!enough_args(args, 2)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	int n_ch = atoi(args[2]);
	if (n_ch < 1) printf("Invalid new number of channels\n");
	else mix_audio(tracks[idx], n_ch);
}

void bps_cmd(char **args) {
	if (!enough_args(args, 2)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	int bps = atoi(args[2]);
	if (bps < 1 || bps > 8) printf("Invalid new number of bytes per sample\n");
	else tracks[idx]->bps = bps;
}

void rate_cmd(char **args) {
	if (!enough_args(args, 2)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	int rate = atoi(args[2]);
	if (rate < 1) printf("Invalid new sample rate\n");
	else {
		resample_audio(tracks[idx], (float)tracks[idx]->rate / (float)rate);
		tracks[idx]->rate = rate;
	}
}

void fmt_cmd(char **args) {
	if (!enough_args(args, 2)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	if (!strcmp(args[2], "int") || !strcmp(args[2], "1"))
		tracks[idx]->fmt = 1;
	else if (!strncmp(args[2], "float", 5) || !strcmp(args[2], "3"))
		tracks[idx]->fmt = 3;
	else printf("Unrecognised sample format\n");
}

void speed(char **args) {
	if (!enough_args(args, 2)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	float factor = atof(args[2]);
	if (factor <= 0.0) printf("Invalid pitch factor\n");
	else resample_audio(tracks[idx], factor);
}

void amplify(char **args) {
	if (!enough_args(args, 2)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	int i, j;
	float factor = atof(args[2]);
	for (i = 0; i < tracks[idx]->n_ch; i++) {
		for (j = 0; j < tracks[idx]->sz; j++) tracks[idx]->buf[i][j] = smooth_sample(tracks[idx]->buf[i][j] * factor);
	}
}

void get_cmd(char **args) {
	if (!enough_args(args, 3)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	int ch = atoi(args[2]);
	if (ch < 0) return;
	if (ch >= tracks[idx]->n_ch) {
		printf("Error: invalid channel index (number of channels: %d)\n", tracks[idx]->n_ch);
		return;
	}

	int pos = atoi(args[3]);
	if (pos < 0) return;
	if (pos >= tracks[idx]->sz) {
		printf("Error: sample index is too large for track size (%d)\n", tracks[idx]->sz);
		return;
	}

	printf("%.3f", tracks[idx]->buf[ch][pos]);
	if (tracks[idx]->fmt == 1) {
		u32 x = 0;
		write_sample(&x, tracks[idx]->buf[ch][pos], tracks[idx]->bps, 1);
		printf(" (%u)", x);
	}
	printf("\n");
}

void set_cmd(char **args) {
	if (!enough_args(args, 4)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	int ch = atoi(args[2]);
	if (ch < 0) return;
	if (ch >= tracks[idx]->n_ch) {
		printf("Error: invalid channel index (number of channels: %d)\n", tracks[idx]->n_ch);
		return;
	}

	int pos = atoi(args[3]);
	if (pos < 0) return;
	if (pos >= tracks[idx]->sz) {
		printf("Error: sample index is too large for track size (%d)\n", tracks[idx]->sz);
		return;
	}

	float s = atof(args[4]);
	if (s < -1.0) s = -1.0;
	if (s > 1.0) s = 1.0;

	float old = tracks[idx]->buf[ch][pos];
	tracks[idx]->buf[ch][pos] = s;
	printf("%s[%d][%d]: %.3f -> %.3f\n", args[1], ch, pos, old, s);
}

void display(char **args) {
	if (!enough_args(args, 2)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	int pos = atoi(args[2]);
	if (pos < 0) return;
	if (pos >= tracks[idx]->sz) {
		printf("Error: sample index is too large for track size (%d)\n", tracks[idx]->sz);
		return;
	}

	float scale = args[3] ? atof(args[3]) : 1.0;
	if (scale <= 0.0) scale = 1.0;

	int ch = args[4] ? atoi(args[4]) : -1;
	if (ch >= tracks[idx]->n_ch) ch = -1;

	if (pos >= tracks[idx]->sz) return;

	audio_t temp = {0};
	if (ch >= 0) insert_channel(&temp, tracks[idx], 0, ch);
	else transfer_audio(&temp, tracks[idx]);

	remove_audio(&temp, 0, pos);

	int sz = (int)(scale * 72.0) + 1;
	if (sz < temp.sz) remove_audio(&temp, sz, 0);
	resample_audio(&temp, scale);

	sz = temp.sz < 72 ? temp.sz : 72;
	int *set = calloc(sz, sizeof(int));

	int i, j, c;
	printf("    ");
	for (i = 0; i < sz; i++) putchar('_');
	printf("\n");

	for (c = 0; c < temp.n_ch; c++) {
		for (i = 0; i < sz; i++) {
			set[i] = (int)((temp.buf[c][i] + 1.0) * 4.5);
			if (set[i] > 8) set[i] = 8;
			if (set[i] < 0) set[i] = 0;
		}

		for (i = 8; i >= 0; i--) {
			printf("   |");
			char e = i ? ' ' : '_';
			e = i == 4 ? '-' : e;
			for (j = 0; j < sz; j++) putchar(set[j] == i ? '#' : e);
			printf("|\n");
		}
	}
	printf("\n");
	close_audio(&temp);
}

void apply(char **args, int mode) {
	if (!enough_args(args, 3)) return;

	int idx = find_var(args[1], 0);
	if (idx < 0) {
		audio_t temp = {0};
		add_track(&temp, args[1]);
		idx = n_tracks-1;
	}

	int idx2 = find_var(args[2], 1);
	if (idx2 < 0) return;

	int offset = atoi(args[3]);
	int size = args[4] ? atoi(args[4]) : 0;
	float amp = args[5] ? atof(args[5]) : 1.0;

	if (mode) insert_audio(tracks[idx], tracks[idx2], offset, size, amp);
	else add_audio(tracks[idx], tracks[idx2], offset, size, amp);
}

void insert(char **args) {
	apply(args, 1);
}

void add(char **args) {
	apply(args, 0);
}

void remove_cmd(char **args) {
	if (!enough_args(args, 2)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	int offset = atoi(args[2]);
	int size = args[3] ? atoi(args[3]) : 0;
	remove_audio(tracks[idx], offset, size);
}

void reverse(char **args) {
	if (!enough_args(args, 1)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	reverse_audio(tracks[idx]);
}

void insert_ch(char **args) {
	if (!enough_args(args, 4)) return;

	int idx2 = find_var(args[2], 1);
	if (idx2 < 0) return;

	audio_t temp = {0};
	int idx = find_var(args[1], 0);
	if (idx >= 0 && tracks[idx]->n_ch > 0) transfer_audio(&temp, tracks[idx]);

	int dst_ch = atoi(args[3]);
	int src_ch = atoi(args[4]);
	insert_channel(&temp, tracks[idx2], dst_ch, src_ch);

	add_track(&temp, args[1]);
	close_audio(&temp);
}

void delete_ch(char **args) {
	if (!enough_args(args, 2)) return;

	int idx = find_var(args[1], 1);
	if (idx < 0) return;

	int ch = atoi(args[2]);
	remove_channel(tracks[idx], ch);
}

command commands[] = {
	NULL, help, list, info, open_wav, open_raw, save_wav, save_raw, transfer,
	generate, mix, bps_cmd, rate_cmd, fmt_cmd, speed, amplify,
	get_cmd, set_cmd, display, insert, add, remove_cmd, reverse,
	insert_ch, delete_ch
};

int main(int argc, char **argv) {
	printf("WAV Tool\n\n");

	int i, j;
	char cmd[80] = {0};
	char *args[MAX_ARGS];
	while (1) {
		prompt(cmd, "> ");
		args[0] = strtok(cmd, " \n");
		for (i = 1; i < MAX_ARGS; i++) args[i] = strtok(NULL, " \n");
		if (!args[0]) {
			printf("Type \"help\" for the list of commands (no quotation marks)\n");
			continue;
		}

		int cid = find_cmd(args[0]);
		if (cid < 0) continue;

		if (!commands[cid]) break;
		commands[cid](args);
	}

	if (tracks) {
		for (i = 0; i < n_tracks; i++) close_audio(tracks[i]);
		free(tracks);
	}
	return 0;
}
