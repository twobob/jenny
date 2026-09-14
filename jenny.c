#define _CRT_SECURE_NO_WARNINGS
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PPQ 480
#define TICKS16 (PPQ / 4)
#define MAXPAT 128
#define MAXBAR 64

typedef struct { long long tick; int on; int note; int vel; } Ev;

static Ev *evs;
static size_t nev, capev;

static void add_ev(long long tick, int on, int note, int vel) {
    if (nev == capev) {
        capev = capev ? capev * 2 : 1024;
        evs = realloc(evs, capev * sizeof *evs);
        if (!evs) { fprintf(stderr, "jenny: out of memory\n"); exit(1); }
    }
    evs[nev].tick = tick; evs[nev].on = on; evs[nev].note = note; evs[nev].vel = vel;
    nev++;
}

static int ev_cmp(const void *pa, const void *pb) {
    const Ev *a = pa, *b = pb;
    if (a->tick != b->tick) return a->tick < b->tick ? -1 : 1;
    if (a->on != b->on) return a->on - b->on;
    return a->note - b->note;
}

static void usage(void) {
    fprintf(stderr,
        "usage: jenny [options] [input.txt]   (reads stdin when no input file)\n"
        "  -o FILE      output MIDI file (default jenny.mid)\n"
        "  --gm N       General MIDI program 1-128 (default 1, Acoustic Grand Piano)\n"
        "  --oct N      octave of the chord root, C4 = 60 (default 4)\n"
        "  --beat PAT   per-sixteenth pattern of 0/1/x, up to 128 (default 1xxxxxxxxxxxxxx0)\n"
        "  --vel N      hit velocity 1-127 (default 75)\n"
        "  --bpm N      tempo (default 120)\n"
        "  --metre N/D  time signature, sets sixteenths per bar for --beat (default 4/4)\n"
        "  --accent V   velocity of hits on the downbeat: +N or -N relative to --vel, or N absolute\n"
        "  --arp [STYLE] arpeggiate the chord while the --beat pattern is 1 or held (default classic)\n"
        "               classic   12312312, notes 1-3 in eighths restarting each bar\n"
        "               DIGITS    your own bar pattern of chord notes, lowest = 1, e.g. 1323\n"
        "               up down updown downup upanddown downandup converge diverge\n"
        "               condiverge pinkyup pinkyupdown thumbup thumbupdown chord\n"
        "               random randomother randomonce\n"
        "  --rate N     arp step, 8 for eighths or 16 for sixteenths (default 8)\n");
    exit(2);
}

static int int_arg(int argc, char **argv, int *i, int lo, int hi) {
    if (*i + 1 >= argc) usage();
    char *end;
    long v = strtol(argv[++*i], &end, 10);
    if (*end || v < lo || v > hi) {
        fprintf(stderr, "jenny: %s wants an integer %d-%d\n", argv[*i - 1], lo, hi);
        exit(2);
    }
    return (int)v;
}

static const char *arp_names[] = {
    "up", "down", "updown", "downup", "upanddown", "downandup", "converge", "diverge",
    "condiverge", "pinkyup", "pinkyupdown", "thumbup", "thumbupdown", "chord",
    "random", "randomother", "randomonce", NULL
};

static const char *arp;
static int arp_step = 2;
static unsigned long rng = 12345;

static int rnd(int n) {
    rng = rng * 1103515245UL + 12345UL;
    return (int)((rng >> 16) % (unsigned long)n);
}

static void shuffle(int *s, int n) {
    for (int i = 0; i < n; i++) s[i] = i;
    for (int i = n - 1; i > 0; i--) { int j = rnd(i + 1), t = s[i]; s[i] = s[j]; s[j] = t; }
}

static int arp_seq(int n, int *s) {
    int len = 0, top = n - 1;
    if (n == 1) { s[0] = 0; return 1; }
    if (!strcmp(arp, "up")) for (int i = 0; i < n; i++) s[len++] = i;
    else if (!strcmp(arp, "down")) for (int i = top; i >= 0; i--) s[len++] = i;
    else if (!strcmp(arp, "updown")) {
        for (int i = 0; i < n; i++) s[len++] = i;
        for (int i = top - 1; i > 0; i--) s[len++] = i;
    } else if (!strcmp(arp, "downup")) {
        for (int i = top; i >= 0; i--) s[len++] = i;
        for (int i = 1; i < top; i++) s[len++] = i;
    } else if (!strcmp(arp, "upanddown")) {
        for (int i = 0; i < n; i++) s[len++] = i;
        for (int i = top; i >= 0; i--) s[len++] = i;
    } else if (!strcmp(arp, "downandup")) {
        for (int i = top; i >= 0; i--) s[len++] = i;
        for (int i = 0; i < n; i++) s[len++] = i;
    } else if (!strcmp(arp, "converge") || !strcmp(arp, "diverge") || !strcmp(arp, "condiverge")) {
        int c[16];
        for (int lo = 0, hi = top; lo <= hi; lo++, hi--) {
            c[len++] = lo;
            if (hi != lo) c[len++] = hi;
        }
        if (!strcmp(arp, "converge")) memcpy(s, c, (size_t)len * sizeof *s);
        else if (!strcmp(arp, "diverge")) for (int i = 0; i < len; i++) s[i] = c[len - 1 - i];
        else {
            memcpy(s, c, (size_t)len * sizeof *s);
            for (int i = 1; i < n - 1; i++) s[len + i - 1] = c[n - 1 - i];
            len += n - 2;
        }
    } else if (!strcmp(arp, "pinkyup") || !strcmp(arp, "pinkyupdown")) {
        for (int i = 0; i < top; i++) { s[len++] = i; s[len++] = top; }
        if (!strcmp(arp, "pinkyupdown"))
            for (int i = top - 2; i > 0; i--) { s[len++] = i; s[len++] = top; }
    } else if (!strcmp(arp, "thumbup") || !strcmp(arp, "thumbupdown")) {
        for (int i = 1; i < n; i++) { s[len++] = 0; s[len++] = i; }
        if (!strcmp(arp, "thumbupdown"))
            for (int i = top - 1; i > 1; i--) { s[len++] = 0; s[len++] = i; }
    } else if (!strcmp(arp, "randomonce")) {
        shuffle(s, n);
        len = n;
    }
    return len;
}

static void play_arp(const int *notes, int nn, long long abs16, int dur, long pos,
                     const char *pat, int patlen, int bar16, int vel, int accent_vel) {
    int seq[64], seqlen = 0, digits = isdigit((unsigned char)arp[0]);
    int chord = !strcmp(arp, "chord"), random = !strcmp(arp, "random");
    int other = !strcmp(arp, "randomother");
    if (!digits && !chord && !random && !other) seqlen = arp_seq(nn, seq);
    int held = 0, step = 0, on[16], non = 0;
    for (int k = 0; k <= dur; k++) {
        long long t = (abs16 + k) * TICKS16;
        long p = pos + k;
        if (k < dur) {
            char c = pat[p % patlen];
            held = c == '1' || (c == 'x' && (held || k == 0));
        }
        int grid = p % arp_step == 0;
        if (non && (k == dur || !held || grid)) {
            for (int j = 0; j < non; j++) add_ev(t, 0, on[j], 0);
            non = 0;
        }
        if (k == dur || !held || !grid) continue;
        if (chord) { for (int j = 0; j < nn; j++) on[non++] = notes[j]; }
        else if (digits) {
            size_t len = strlen(arp);
            on[non++] = notes[(arp[(size_t)((p % bar16) / arp_step) % len] - '1') % nn];
        } else if (random) on[non++] = notes[rnd(nn)];
        else if (other) {
            if (step % nn == 0) shuffle(seq, nn);
            on[non++] = notes[seq[step % nn]];
        } else on[non++] = notes[seq[step % seqlen]];
        step++;
        int v = p % bar16 == 0 ? accent_vel : vel;
        for (int j = 0; j < non; j++) add_ev(t, 1, on[j], v);
    }
}

static void play_chord(const int *notes, int nn, long long abs16, int dur, long pos,
                       const char *pat, int patlen, int bar16, int vel, int accent_vel) {
    if (arp) { play_arp(notes, nn, abs16, dur, pos, pat, patlen, bar16, vel, accent_vel); return; }
    int sounding = 0;
    for (int k = 0; k < dur; k++) {
        char c = pat[(pos + k) % patlen];
        long long t = (abs16 + k) * TICKS16;
        int strike = c == '1' || (k == 0 && c == 'x');
        if (strike || c == '0') {
            if (sounding) for (int j = 0; j < nn; j++) add_ev(t, 0, notes[j], 0);
            sounding = 0;
        }
        if (strike) {
            int v = (pos + k) % bar16 == 0 ? accent_vel : vel;
            for (int j = 0; j < nn; j++) add_ev(t, 1, notes[j], v);
            sounding = 1;
        }
    }
    if (sounding)
        for (int j = 0; j < nn; j++) add_ev((abs16 + dur) * TICKS16, 0, notes[j], 0);
}

static void put_vlq(FILE *f, unsigned long v) {
    unsigned char buf[5];
    int n = 0;
    buf[n++] = v & 0x7f;
    while (v >>= 7) buf[n++] = (unsigned char)(0x80 | (v & 0x7f));
    while (n) fputc(buf[--n], f);
}

static void put_be32(FILE *f, unsigned long v) {
    fputc((v >> 24) & 0xff, f); fputc((v >> 16) & 0xff, f);
    fputc((v >> 8) & 0xff, f);  fputc(v & 0xff, f);
}

int main(int argc, char **argv) {
    const char *in_path = NULL, *out_path = "jenny.mid";
    int gm = 1, oct = 4, vel = 75, bpm = 120;
    int num = 4, den = 4;
    const char *accent = NULL;
    char pat[MAXPAT + MAXBAR + 1] = "1xxxxxxxxxxxxxx0";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o")) { if (++i >= argc) usage(); out_path = argv[i]; }
        else if (!strcmp(argv[i], "--gm")) gm = int_arg(argc, argv, &i, 1, 128);
        else if (!strcmp(argv[i], "--oct")) oct = int_arg(argc, argv, &i, -1, 9);
        else if (!strcmp(argv[i], "--vel")) vel = int_arg(argc, argv, &i, 1, 127);
        else if (!strcmp(argv[i], "--bpm")) bpm = int_arg(argc, argv, &i, 1, 1000);
        else if (!strcmp(argv[i], "--metre")) {
            char *end;
            if (++i >= argc) usage();
            num = (int)strtol(argv[i], &end, 10);
            den = *end == '/' ? (int)strtol(end + 1, &end, 10) : 0;
            if (*end || num < 1 || (den != 1 && den != 2 && den != 4 && den != 8 && den != 16)
                || num * 16 / den > MAXBAR) {
                fprintf(stderr, "jenny: --metre wants N/D, D one of 1 2 4 8 16, bar at most 64 sixteenths\n");
                return 2;
            }
        }
        else if (!strcmp(argv[i], "--arp")) {
            arp = "12312312";
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                const char *a = argv[++i];
                int ok = 0;
                for (int j = 0; arp_names[j]; j++) if (!strcmp(a, arp_names[j])) ok = 1;
                if (!strcmp(a, "classic")) ok = 1;
                else if (strspn(a, "123456789") == strlen(a) && strlen(a) <= MAXPAT) ok = 2;
                if (!ok) { fprintf(stderr, "jenny: unknown --arp style %s\n", a); usage(); }
                if (strcmp(a, "classic")) arp = a;
            }
        }
        else if (!strcmp(argv[i], "--rate")) {
            int r = int_arg(argc, argv, &i, 1, 16);
            if (r != 8 && r != 16) { fprintf(stderr, "jenny: --rate is 8 or 16\n"); return 2; }
            arp_step = 16 / r;
        }
        else if (!strcmp(argv[i], "--accent")) { if (++i >= argc) usage(); accent = argv[i]; }
        else if (!strcmp(argv[i], "--beat")) {
            if (++i >= argc) usage();
            size_t n = strlen(argv[i]);
            if (n == 0 || n > MAXPAT) { fprintf(stderr, "jenny: --beat must be 1-%d long\n", MAXPAT); return 2; }
            for (size_t j = 0; j < n; j++) {
                char c = (char)tolower((unsigned char)argv[i][j]);
                if (c != '0' && c != '1' && c != 'x') { fprintf(stderr, "jenny: --beat takes only 0, 1 and x\n"); return 2; }
                pat[j] = c;
            }
            pat[n] = 0;
        }
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) usage();
        else if (argv[i][0] == '-' && argv[i][1]) { fprintf(stderr, "jenny: unknown option %s\n", argv[i]); usage(); }
        else in_path = argv[i];
    }
    int accent_vel = vel;
    if (accent) {
        char *end;
        long v = strtol(accent, &end, 10);
        if (*end || !*accent) { fprintf(stderr, "jenny: --accent wants +N, -N or N\n"); return 2; }
        if (accent[0] == '+' || accent[0] == '-') v += vel;
        accent_vel = v < 1 ? 1 : v > 127 ? 127 : (int)v;
    }
    int bar16 = num * 16 / den;
    int patlen = (int)strlen(pat);
    while (patlen % bar16) pat[patlen++] = '0';
    pat[patlen] = 0;

    FILE *in = in_path ? fopen(in_path, "r") : stdin;
    if (!in) { fprintf(stderr, "jenny: cannot open %s\n", in_path); return 1; }

    char line[4096];
    int col_token = -1, col_start = -1;
    long long run16 = 0;
    int nchords = 0;
    while (fgets(line, sizeof line, in)) {
        char *f[64];
        int nf = 0;
        for (char *p = strtok(line, " \t\r\n"); p && nf < 64; p = strtok(NULL, " \t\r\n")) f[nf++] = p;
        if (nf == 0) continue;
        if (!strcmp(f[0], "idx") || !strcmp(f[0], "tonic")) {
            col_token = col_start = -1;
            for (int j = 0; j < nf; j++) {
                if (!strcmp(f[j], "token")) col_token = j;
                if (!strcmp(f[j], "start16")) col_start = j;
            }
            continue;
        }
        const char *ts = f[col_token >= 0 && col_token < nf ? col_token : nf - 1];
        char *end;
        uint64_t tok = strlen(ts) == 16 ? strtoull(ts, &end, 16) : strtoull(ts, &end, 10);
        if (*end || tok == 0) continue;

        uint32_t a = (uint32_t)tok, b = (uint32_t)(tok >> 32);
        int tonic = a & 15, degree = (a >> 5) & 15, mask = (a >> 9) & 0x7ff, bass = (a >> 20) & 15;
        int bar = b & 0xffff, onset = (b >> 16) & 63, dur = (b >> 22) & 255;

        long long abs16 = run16;
        if (col_start >= 0 && col_start < nf) abs16 = (long long)(atof(f[col_start]) + 0.5);
        run16 = abs16 + dur;
        if (degree > 11 || dur == 0) continue;

        int notes[16], nn = 0;
        int root = (oct + 1) * 12 + (tonic + degree) % 12;
        if (bass) notes[nn++] = root + bass - 12;
        notes[nn++] = root;
        for (int iv = 1; iv <= 11; iv++)
            if (mask & (1 << (iv - 1))) notes[nn++] = root + iv;
        int ok = 0;
        for (int j = 0; j < nn; j++)
            if (notes[j] >= 0 && notes[j] <= 127) notes[ok++] = notes[j];
        play_chord(notes, ok, abs16, dur, (long)bar * bar16 + onset, pat, patlen, bar16, vel, accent_vel);
        nchords++;
    }
    if (in != stdin) fclose(in);

    qsort(evs, nev, sizeof *evs, ev_cmp);

    FILE *out = fopen(out_path, "wb");
    if (!out) { fprintf(stderr, "jenny: cannot write %s\n", out_path); return 1; }
    fwrite("MThd", 1, 4, out); put_be32(out, 6);
    fputc(0, out); fputc(0, out); fputc(0, out); fputc(1, out);
    fputc(PPQ >> 8, out); fputc(PPQ & 0xff, out);
    fwrite("MTrk", 1, 4, out);
    long len_pos = ftell(out);
    put_be32(out, 0);
    long body = ftell(out);

    unsigned long mpq = 60000000UL / (unsigned long)bpm;
    put_vlq(out, 0); fputc(0xff, out); fputc(0x51, out); fputc(3, out);
    fputc((mpq >> 16) & 0xff, out); fputc((mpq >> 8) & 0xff, out); fputc(mpq & 0xff, out);
    put_vlq(out, 0); fputc(0xff, out); fputc(0x58, out); fputc(4, out);
    int dpow = 0;
    while ((1 << dpow) < den) dpow++;
    fputc(num, out); fputc(dpow, out); fputc(24, out); fputc(8, out);
    put_vlq(out, 0); fputc(0xc0, out); fputc(gm - 1, out);

    long long last = 0;
    for (size_t j = 0; j < nev; j++) {
        put_vlq(out, (unsigned long)(evs[j].tick - last));
        last = evs[j].tick;
        fputc(evs[j].on ? 0x90 : 0x80, out);
        fputc(evs[j].note, out);
        fputc(evs[j].vel, out);
    }
    put_vlq(out, 0); fputc(0xff, out); fputc(0x2f, out); fputc(0, out);

    long endp = ftell(out);
    fseek(out, len_pos, SEEK_SET);
    put_be32(out, (unsigned long)(endp - body));
    fclose(out);
    fprintf(stderr, "jenny: %d chords, %zu notes -> %s\n", nchords, nev / 2, out_path);
    free(evs);
    return 0;
}
