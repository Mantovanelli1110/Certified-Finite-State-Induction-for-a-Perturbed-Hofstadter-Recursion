// certificate_exporter.c
//
// Corrected exporter:
//   - exports finite radius-R grammar
//   - exports all observed arithmetic realizations per context
//   - exports global block anchor L in each A-record for independent parity checks
//   - does NOT require identical pullback profiles for same context
//
// Compile:
//   gcc -O3 -std=c11 certificate_exporter.c -o certexport
//
// Run:
//   ./certexport

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define N 50000000

#define R 20
#define CTX_LEN (2*R + 1)

#define MAX_WORDS 64
#define MAX_RETURNS 8000000
#define MAX_WORD_LEN 256
#define MAX_CONTEXTS 200000
#define MAX_EXTENSIONS 200000
#define MAX_REALIZATIONS 6000000
#define MAX_ARITH_LEN 512

static int64_t Q[N + 10];
static int64_t T[N + 10];
static int S[N + 10];
static int C7[N + 10];

static int64_t mark_pos[MAX_RETURNS + 10];
static int seq[MAX_RETURNS + 10];
static int seq_len = 0;

typedef struct {
    int len;
    int w[MAX_WORD_LEN];
} Word;

typedef struct {
    int id;
    int ctx[CTX_LEN];
    int count;
} ContextEntry;

typedef struct {
    int a, b, c;
} Extension;

typedef struct {
    int context_id;
    int center;
} Realization;

static Word dict[MAX_WORDS];
static int dict_count = 0;

static ContextEntry entries[MAX_CONTEXTS];
static int entry_count = 0;

static Extension extensions[MAX_EXTENSIONS];
static int extension_count = 0;

static Realization realizations[MAX_REALIZATIONS];
static int realization_count = 0;

int valid_s(int64_t n) {
    return n >= 5 && n <= N - 5;
}

int code7(int64_t n) {
    int64_t a = T[n] + 1;
    int64_t b = T[n - 1] + 2;

    if (!valid_s(n - 2)) return -1;
    if (!valid_s(n - 1)) return -1;
    if (!valid_s(n)) return -1;
    if (!valid_s(n + 1)) return -1;
    if (!valid_s(n + 2)) return -1;
    if (!valid_s(a)) return -1;
    if (!valid_s(b)) return -1;

    return (S[n - 2] << 6)
         | (S[n - 1] << 5)
         | (S[n]     << 4)
         | (S[n + 1] << 3)
         | (S[n + 2] << 2)
         | (S[a]     << 1)
         | S[b];
}

int marker_at(int64_t n) {
    return C7[n] == 0b1010100
        && C7[n + 1] == 0b0101011;
}

int same_word(int *w, int len, Word *W) {
    if (len != W->len) return 0;

    for (int i = 0; i < len; i++) {
        if (w[i] != W->w[i]) return 0;
    }

    return 1;
}

int get_word_id(int *w, int len) {
    for (int i = 0; i < dict_count; i++) {
        if (same_word(w, len, &dict[i])) return i;
    }

    if (dict_count >= MAX_WORDS) {
        printf("Too many return words\n");
        exit(1);
    }

    dict[dict_count].len = len;

    for (int i = 0; i < len; i++) {
        dict[dict_count].w[i] = w[i];
    }

    return dict_count++;
}

void compute_QTS(void) {
    printf("Computing Q up to N=%d...\n", N);
    fflush(stdout);

    Q[1] = 1;
    Q[2] = 1;

    for (int64_t n = 3; n <= N; n++) {
        int64_t a = n - Q[n - 1];
        int64_t b = n - Q[n - 2];

        if (a <= 0 || b <= 0 || a >= n || b >= n) {
            printf("INVALID recursion at n=%lld a=%lld b=%lld\n",
                   (long long)n, (long long)a, (long long)b);
            exit(1);
        }

        Q[n] = Q[a] + Q[b] + ((n % 2 == 0) ? 1 : -1);
    }

    for (int64_t n = 2; n <= N; n++) {
        T[n] = n - Q[n - 1];
    }

    for (int64_t n = 3; n <= N - 1; n++) {
        int64_t d = Q[n + 1] - Q[n - 1];

        if (d == 0) S[n] = 0;
        else if (d == 2) S[n] = 1;
        else {
            printf("S outside {0,1} at n=%lld diff=%lld\n",
                   (long long)n, (long long)d);
            exit(1);
        }
    }

    for (int64_t n = 10; n <= N - 10; n++) {
        C7[n] = code7(n);
    }
}

void extract_return_sequence(void) {
    printf("Extracting return sequence...\n");
    fflush(stdout);

    int64_t last = -1;
    int block[MAX_WORD_LEN];

    for (int64_t n = 20; n <= N - 20; n++) {
        if (!marker_at(n)) continue;

        if (last >= 0) {
            int len = (int)(n - last);

            if (len >= MAX_WORD_LEN) {
                printf("Return word too long len=%d\n", len);
                exit(1);
            }

            for (int i = 0; i < len; i++) {
                block[i] = C7[last + i];
            }

            int id = get_word_id(block, len);
	if (seq_len >= MAX_RETURNS) {
    	printf("Too many return blocks: seq_len=%d MAX_RETURNS=%d\n",
           seq_len, MAX_RETURNS);
    	exit(1);
}
            seq[seq_len] = id;
            mark_pos[seq_len] = last;
            seq_len++;
        }

        last = n;
    }

    if (last < 0) {
        printf("No return markers found\n");
        exit(1);
    }

    mark_pos[seq_len] = last;

    printf("dict_count=%d\n", dict_count);
    printf("seq_len=%d\n", seq_len);
}

int find_return_block(int64_t x) {
    int lo = 0;
    int hi = seq_len;

    if (x < mark_pos[0]) return -1;
    if (x >= mark_pos[seq_len]) return -1;

    while (lo + 1 < hi) {
        int mid = lo + (hi - lo) / 2;

        if (mark_pos[mid] <= x) lo = mid;
        else hi = mid;
    }

    return lo;
}

int same_context_array(int *a, int *b) {
    for (int i = 0; i < CTX_LEN; i++) {
        if (a[i] != b[i]) return 0;
    }

    return 1;
}

void build_context_at(int center, int *ctx) {
    for (int j = -R; j <= R; j++) {
        ctx[j + R] = seq[center + j];
    }
}

int add_context(int *ctx, int center) {
    for (int i = 0; i < entry_count; i++) {
        if (same_context_array(ctx, entries[i].ctx)) {
            entries[i].count++;

            if (realization_count >= MAX_REALIZATIONS) {
                printf("Too many realizations\n");
                exit(1);
            }

            realizations[realization_count].context_id = entries[i].id;
            realizations[realization_count].center = center;
            realization_count++;

            return entries[i].id;
        }
    }

    if (entry_count >= MAX_CONTEXTS) {
        printf("Too many contexts\n");
        exit(1);
    }

    entries[entry_count].id = entry_count;
    entries[entry_count].count = 1;

    for (int i = 0; i < CTX_LEN; i++) {
        entries[entry_count].ctx[i] = ctx[i];
    }

    if (realization_count >= MAX_REALIZATIONS) {
        printf("Too many realizations\n");
        exit(1);
    }

    realizations[realization_count].context_id = entry_count;
    realizations[realization_count].center = center;
    realization_count++;

    entry_count++;
    return entry_count - 1;
}

int suffix_prefix_match(ContextEntry *A, ContextEntry *B) {
    for (int i = 1; i < CTX_LEN; i++) {
        if (A->ctx[i] != B->ctx[i - 1]) return 0;
    }

    return 1;
}

int find_shifted_context(ContextEntry *A, ContextEntry *B) {
    int y[CTX_LEN];

    for (int i = 0; i < CTX_LEN - 1; i++) {
        y[i] = A->ctx[i + 1];
    }

    y[CTX_LEN - 1] = B->ctx[CTX_LEN - 1];

    for (int c = 0; c < entry_count; c++) {
        if (same_context_array(y, entries[c].ctx)) return c;
    }

    return -1;
}

void build_context_entries(void) {
    printf("Building radius-%d contexts...\n", R);
    fflush(stdout);

    for (int center = R; center + R + 1 < seq_len; center++) {
        int ctx[CTX_LEN];

        build_context_at(center, ctx);
        add_context(ctx, center);
    }

    printf("context_count=%d\n", entry_count);
    printf("realization_count=%d\n", realization_count);
}

void build_extensions(void) {
    printf("Building extension table...\n");
    fflush(stdout);

    int bad = 0;

    for (int i = 0; i < entry_count; i++) {
        for (int j = 0; j < entry_count; j++) {
            if (!suffix_prefix_match(&entries[i], &entries[j])) continue;

            int k = find_shifted_context(&entries[i], &entries[j]);

            if (k < 0) {
                bad++;
                continue;
            }

            if (extension_count >= MAX_EXTENSIONS) {
                printf("Too many extensions\n");
                exit(1);
            }

            extensions[extension_count].a = entries[i].id;
            extensions[extension_count].b = entries[j].id;
            extensions[extension_count].c = entries[k].id;
            extension_count++;
        }
    }

    if (bad) {
        printf("BAD EXTENSIONS during export: %d\n", bad);
        exit(1);
    }

    printf("extension_count=%d\n", extension_count);
}

void write_arithmetic_realization(FILE *f, int rid) {
    int context_id = realizations[rid].context_id;
    int center = realizations[rid].center;

    int64_t L = mark_pos[center];
    int64_t U = mark_pos[center + 1];

    int valid_count = 0;

    for (int64_t n = L; n < U; n++) {
        // TRUE recursion arguments
        int64_t a = T[n];
        int64_t b = T[n - 1] + 1;

        int ba = find_return_block(a);
        int bb = find_return_block(b);

        if (ba >= 0 && bb >= 0) valid_count++;
    }

    fprintf(f, "A %d %d %d %lld %d",
            rid,
            context_id,
            center,
            (long long)L,
            valid_count);

    for (int64_t n = L; n < U; n++) {
        // TRUE recursion arguments
        int64_t a = T[n];
        int64_t b = T[n - 1] + 1;

        int ba = find_return_block(a);
        int bb = find_return_block(b);

        if (ba < 0 || bb < 0) continue;

        int rel_a = ba - center;
        int rel_b = bb - center;

        int offa = (int)(a - mark_pos[ba]);
        int offb = (int)(b - mark_pos[bb]);

        int sym_a = seq[ba];
        int sym_b = seq[bb];

        int t = (int)(n - L);

        int64_t q_t = Q[n];
        int64_t q_a = Q[a];
        int64_t q_b = Q[b];

        int eps = (n % 2 == 0) ? 1 : -1;

        fprintf(
            f,
            " %d %lld %d %d %d %lld %d %d %d %lld %d",
            t,
            (long long)q_t,
            rel_a,
            offa,
            sym_a,
            (long long)q_a,
            rel_b,
            offb,
            sym_b,
            (long long)q_b,
            eps
        );
    }

    fprintf(f, "\n");
}

void export_certificate(const char *filename) {
    printf("Writing %s...\n", filename);
    fflush(stdout);

    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Cannot open certificate file for writing\n");
        exit(1);
    }

    fprintf(f, "WORDS %d\n", dict_count);

    for (int i = 0; i < dict_count; i++) {
        fprintf(f, "W %d %d", i, dict[i].len);

        for (int j = 0; j < dict[i].len; j++) {
            fprintf(f, " %d", dict[i].w[j]);
        }

        fprintf(f, "\n");
    }

    fprintf(f, "\nCONTEXTS %d\n", entry_count);

    for (int i = 0; i < entry_count; i++) {
        fprintf(f, "C %d", entries[i].id);

        for (int j = 0; j < CTX_LEN; j++) {
            fprintf(f, " %d", entries[i].ctx[j]);
        }

        fprintf(f, " CENTER %d COUNT %d\n",
                entries[i].ctx[R],
                entries[i].count);
    }

    fprintf(f, "\nEXTENSIONS %d\n", extension_count);

    for (int i = 0; i < extension_count; i++) {
        fprintf(f, "E %d %d %d\n",
                extensions[i].a,
                extensions[i].b,
                extensions[i].c);
    }

    fflush(f);

    printf("Writing realizations header: %d\n", realization_count);
    fflush(stdout);

    fprintf(f, "\nREALIZATIONS %d\n", realization_count);
    fflush(f);

    for (int i = 0; i < realization_count; i++) {
        write_arithmetic_realization(f, i);

        if ((i + 1) % 100000 == 0) {
            printf("  written realizations: %d / %d\n", i + 1, realization_count);
            fflush(stdout);
            fflush(f);

            if (ferror(f)) {
                printf("FILE WRITE ERROR after realization %d\n", i + 1);
                fclose(f);
                exit(1);
            }
        }
    }

    fflush(f);

    if (ferror(f)) {
        printf("FILE WRITE ERROR before close\n");
        fclose(f);
        exit(1);
    }

    if (fclose(f) != 0) {
        printf("ERROR: fclose failed. Disk full or write interrupted.\n");
        exit(1);
    }

    printf("Finished writing certificate.txt\n");
    fflush(stdout);
}
int main(void) {
    compute_QTS();
    extract_return_sequence();
    build_context_entries();
    build_extensions();
    export_certificate("certificate.txt");

    printf("\nRESULT: MULTI-REALIZATION CERTIFICATE EXPORTED\n");
    printf("radius=%d\n", R);
    printf("words=%d\n", dict_count);
    printf("seq_len=%d\n", seq_len);
    printf("contexts=%d\n", entry_count);
    printf("extensions=%d\n", extension_count);
    printf("realizations=%d\n", realization_count);
    printf("output=certificate.txt\n");

    return 0;
}