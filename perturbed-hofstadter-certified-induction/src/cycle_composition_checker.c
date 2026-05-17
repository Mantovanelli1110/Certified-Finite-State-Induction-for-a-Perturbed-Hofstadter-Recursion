// cycle_composition_checker.c
//
// Prüft 4-Schritt-Komposition im Zyklus:
//
//   V --expand via U--> T-symbols
//     --expand via T--> S-symbols
//     --expand via S--> P-symbols
//
// Praktischer Zweck:
//   Für V0..V(K-1) wird geprüft, ob nach Expansion über
//   U,T,S alle Endsymbole < S_count bzw. innerhalb eines
//   angegebenen Kernlimits bleiben.
//
// Compile:
//   gcc -O3 -std=c11 cycle_composition_checker.c -o ccomp
//
// Run:
//   ./ccomp s_certificate.txt S t_certificate.txt T u_certificate.txt U v_certificate.txt V 28
//
// Optional mit End-Kernlimit:
//   ./ccomp s_certificate.txt S t_certificate.txt T u_certificate.txt U v_certificate.txt V 28 53

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WORDS 100000
#define MAX_EXPANSION 20000000

typedef struct {
    int used;
    int id;
    int len;
    int *w;
    int count;
} Word;

typedef struct {
    char level;
    int count;
    Word *words;
} Cert;

typedef struct {
    int *a;
    int len;
    int cap;
} Vec;

void die(const char *msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

void vec_init(Vec *v) {
    v->cap = 1024;
    v->len = 0;
    v->a = malloc(sizeof(int) * (size_t)v->cap);
    if (!v->a) die("out of memory vec");
}

void vec_clear(Vec *v) {
    v->len = 0;
}

void vec_push(Vec *v, int x) {
    if (v->len >= MAX_EXPANSION) die("expansion too large");

    if (v->len >= v->cap) {
        v->cap *= 2;
        if (v->cap > MAX_EXPANSION) v->cap = MAX_EXPANSION;

        int *p = realloc(v->a, sizeof(int) * (size_t)v->cap);
        if (!p) die("out of memory vec grow");
        v->a = p;
    }

    v->a[v->len++] = x;
}

void vec_free(Vec *v) {
    free(v->a);
    v->a = NULL;
    v->len = 0;
    v->cap = 0;
}

void make_tags(char level, char *header, char *wordtag, char *seqtag) {
    sprintf(header, "%cWORDS", level);
    sprintf(wordtag, "%c", level);
    sprintf(seqtag, "%c_SEQUENCE", level);
}

void free_cert(Cert *C) {
    if (!C->words) return;

    for (int i = 0; i < C->count; i++) {
        free(C->words[i].w);
    }

    free(C->words);
    C->words = NULL;
}

void parse_cert(const char *filename, Cert *C, char level) {
    FILE *f = fopen(filename, "r");
    if (!f) die("cannot open certificate");

    C->level = level;
    C->count = 0;
    C->words = NULL;

    char header[64], wordtag[16], seqtag[64];
    make_tags(level, header, wordtag, seqtag);

    char tag[64];
    int in_transitions = 0;

    while (fscanf(f, "%63s", tag) == 1) {

        if (strcmp(tag, header) == 0) {
            in_transitions = 0;

            if (fscanf(f, "%d", &C->count) != 1)
                die("bad WORDS header");

            if (C->count <= 0 || C->count > MAX_WORDS)
                die("bad word count");

            C->words = calloc((size_t)C->count, sizeof(Word));
            if (!C->words) die("out of memory words");
        }

        else if (strcmp(tag, wordtag) == 0 && !in_transitions) {
            int id, len, count;
            char count_tag[64];

            if (!C->words) die("word before header");

            if (fscanf(f, "%d %d", &id, &len) != 2)
                die("bad word line");

            if (id < 0 || id >= C->count)
                die("bad word id");

            if (len <= 0)
                die("bad word length");

            Word *W = &C->words[id];

            W->used = 1;
            W->id = id;
            W->len = len;
            W->w = malloc(sizeof(int) * (size_t)len);
            if (!W->w) die("out of memory word");

            for (int i = 0; i < len; i++) {
                if (fscanf(f, "%d", &W->w[i]) != 1)
                    die("bad word symbol");
            }

            if (fscanf(f, "%63s", count_tag) != 1)
                die("missing COUNT");

            if (strcmp(count_tag, "COUNT") != 0)
                die("expected COUNT");

            if (fscanf(f, "%d", &count) != 1)
                die("bad COUNT");

            W->count = count;
        }

        else if (strcmp(tag, seqtag) == 0) {
            in_transitions = 0;

            int n, dummy;

            if (fscanf(f, "%d", &n) != 1)
                die("bad sequence header");

            for (int i = 0; i < n; i++) {
                if (fscanf(f, "%d", &dummy) != 1)
                    die("bad sequence item");
            }
        }

        else if (strcmp(tag, "TRANSITIONS") == 0) {
            int n;

            if (fscanf(f, "%d", &n) != 1)
                die("bad TRANSITIONS header");

            in_transitions = 1;
        }

        else if (strcmp(tag, "T") == 0 && in_transitions) {
            int a, b;

            if (fscanf(f, "%d %d", &a, &b) != 2)
                die("bad transition");
        }

        else {
            fprintf(stderr, "Unknown tag %s in %s\n", tag, filename);
            die("unknown tag");
        }
    }

    fclose(f);
}

void expand_once(Vec *in, Vec *out, Cert *C) {
    vec_clear(out);

    for (int i = 0; i < in->len; i++) {
        int x = in->a[i];

        if (x < 0 || x >= C->count) {
            fprintf(stderr,
                    "ERROR: symbol %d cannot expand in level %c with count=%d\n",
                    x, C->level, C->count);
            exit(1);
        }

        if (!C->words[x].used) {
            fprintf(stderr, "ERROR: %c%d undefined\n", C->level, x);
            exit(1);
        }

        Word *W = &C->words[x];

        for (int j = 0; j < W->len; j++) {
            vec_push(out, W->w[j]);
        }
    }
}

unsigned long long fnv_hash_vec(Vec *v) {
    unsigned long long h = 1469598103934665603ULL;

    for (int i = 0; i < v->len; i++) {
        h ^= (unsigned long long)(v->a[i] + 1000003);
        h *= 1099511628211ULL;
    }

    return h;
}

int check_final_symbols(Vec *v, int limit, int verbose, int id) {
    int bad = 0;
    int max_seen = -1;

    for (int i = 0; i < v->len; i++) {
        int x = v->a[i];

        if (x > max_seen) max_seen = x;

        if (x < 0 || x >= limit) {
            bad++;

            if (verbose && bad <= 20) {
                printf("BAD: expanded V%d has final symbol %d at position %d, limit=0..%d\n",
                       id, x, i, limit - 1);
            }
        }
    }

    if (verbose) {
        printf("V%d final_len=%d max_final_symbol=%d bad=%d\n",
               id, v->len, max_seen, bad);
    }

    return bad;
}

void seed_word(Vec *v, Cert *C, int id) {
    vec_clear(v);

    if (id < 0 || id >= C->count) die("seed id out of range");
    if (!C->words[id].used) die("seed word undefined");

    Word *W = &C->words[id];

    for (int i = 0; i < W->len; i++) {
        vec_push(v, W->w[i]);
    }
}

int main(int argc, char **argv) {
    if (argc != 10 && argc != 11) {
        fprintf(stderr,
                "Usage:\n"
                "  %s s_certificate.txt S t_certificate.txt T u_certificate.txt U v_certificate.txt V K [final_limit]\n",
                argv[0]);
        return 1;
    }

    Cert S, T, U, V;

    parse_cert(argv[1], &S, argv[2][0]);
    parse_cert(argv[3], &T, argv[4][0]);
    parse_cert(argv[5], &U, argv[6][0]);
    parse_cert(argv[7], &V, argv[8][0]);

    int K = atoi(argv[9]);
    if (K <= 0 || K > V.count) die("bad K");

    int final_limit = S.count;
    if (argc == 11) {
        final_limit = atoi(argv[10]);
        if (final_limit <= 0) die("bad final limit");
    }

    printf("Parsed cycle certificates:\n");
    printf("S_count=%d\n", S.count);
    printf("T_count=%d\n", T.count);
    printf("U_count=%d\n", U.count);
    printf("V_count=%d\n", V.count);
    printf("K=%d\n", K);
    printf("final_limit=%d\n\n", final_limit);

    Vec a, b, c, d;
    vec_init(&a);
    vec_init(&b);
    vec_init(&c);
    vec_init(&d);

    int total_bad = 0;
    int ok_words = 0;

    for (int id = 0; id < K; id++) {
        seed_word(&a, &V, id);

        // V-word contains U-symbols
        expand_once(&a, &b, &U);

        // result contains T-symbols
        expand_once(&b, &c, &T);

        // result contains S-symbols
        expand_once(&c, &d, &S);

        int bad = check_final_symbols(&d, final_limit, 1, id);
        total_bad += bad;

        unsigned long long h = fnv_hash_vec(&d);

        printf("V%d composed_len=%d hash=%016llx status=%s\n",
               id,
               d.len,
               h,
               bad == 0 ? "OK" : "BAD");

        if (bad == 0) ok_words++;
    }

    printf("\nRESULT SUMMARY:\n");
    printf("checked_words=%d\n", K);
    printf("ok_words=%d\n", ok_words);
    printf("total_bad_final_symbols=%d\n", total_bad);

    if (total_bad == 0) {
        printf("\nRESULT: 4-STEP CYCLE COMPOSITION SYMBOL-CLOSED\n");
        printf("Every composed V0..V%d expansion lands inside final symbols 0..%d.\n",
               K - 1, final_limit - 1);
    } else {
        printf("\nRESULT: 4-STEP CYCLE COMPOSITION NOT CLOSED\n");
    }

    vec_free(&a);
    vec_free(&b);
    vec_free(&c);
    vec_free(&d);

    free_cert(&S);
    free_cert(&T);
    free_cert(&U);
    free_cert(&V);

    return total_bad == 0 ? 0 : 1;
}
