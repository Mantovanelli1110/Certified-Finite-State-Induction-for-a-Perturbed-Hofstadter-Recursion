// cycle_composition_pattern_checker.c
//
// Prüft die 4-Schritt-Komposition:
//
//   V --U--> T --T--> S --S--> End-Symbole
//
// und vergleicht das Ergebnis mit den S-Wörtern.
// Für jedes V_i wird geprüft, ob die 4-fach expandierte Endfolge
// exakt einem S_j entspricht.
//
// Compile:
//   gcc -O3 -std=c11 cycle_composition_pattern_checker.c -o cpattern
//
// Run:
//   ./cpattern s_certificate.txt S t_certificate.txt T u_certificate.txt U v_certificate.txt V 28

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_WORDS 100000
#define MAX_EXPANSION 50000000

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

void seed_word(Vec *v, Cert *C, int id) {
    vec_clear(v);

    if (id < 0 || id >= C->count)
        die("seed id out of range");

    if (!C->words[id].used)
        die("seed word undefined");

    Word *W = &C->words[id];

    for (int i = 0; i < W->len; i++) {
        vec_push(v, W->w[i]);
    }
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

uint64_t fnv_hash_ints(const int *a, int len) {
    uint64_t h = 1469598103934665603ULL;

    for (int i = 0; i < len; i++) {
        h ^= (uint64_t)(a[i] + 1000003);
        h *= 1099511628211ULL;
    }

    return h;
}

int same_vec_word(Vec *v, Word *W) {
    if (!W->used) return 0;
    if (v->len != W->len) return 0;

    for (int i = 0; i < v->len; i++) {
        if (v->a[i] != W->w[i]) return 0;
    }

    return 1;
}

int find_matching_word(Vec *v, Cert *C) {
    uint64_t hv = fnv_hash_ints(v->a, v->len);

    for (int i = 0; i < C->count; i++) {
        Word *W = &C->words[i];

        if (!W->used) continue;
        if (W->len != v->len) continue;

        uint64_t hw = fnv_hash_ints(W->w, W->len);
        if (hw != hv) continue;

        if (same_vec_word(v, W)) return i;
    }

    return -1;
}

int check_symbols(Vec *v, int limit) {
    int bad = 0;

    for (int i = 0; i < v->len; i++) {
        if (v->a[i] < 0 || v->a[i] >= limit) {
            bad++;
        }
    }

    return bad;
}

int max_symbol(Vec *v) {
    int m = -1;

    for (int i = 0; i < v->len; i++) {
        if (v->a[i] > m) m = v->a[i];
    }

    return m;
}

void print_vec_prefix(Vec *v, int maxn) {
    int n = v->len < maxn ? v->len : maxn;

    for (int i = 0; i < n; i++) {
        printf(" %d", v->a[i]);
    }

    if (v->len > maxn) printf(" ...");
    printf("\n");
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
    if (K <= 0 || K > V.count)
        die("bad K");

    int final_limit = S.count;

    if (argc == 11) {
        final_limit = atoi(argv[10]);
        if (final_limit <= 0 || final_limit > S.count)
            die("bad final limit");
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

    int total_symbol_bad = 0;
    int matched = 0;
    int unmatched = 0;

    for (int id = 0; id < K; id++) {
        seed_word(&a, &V, id);

        expand_once(&a, &b, &U);
        expand_once(&b, &c, &T);
        expand_once(&c, &d, &S);

        int symbol_bad = check_symbols(&d, final_limit);
        total_symbol_bad += symbol_bad;

        int match = -1;

        if (symbol_bad == 0) {
            match = find_matching_word(&d, &S);
        }

        uint64_t h = fnv_hash_ints(d.a, d.len);

        printf("V%d composed_len=%d max_symbol=%d hash=%016llx ",
               id,
               d.len,
               max_symbol(&d),
               (unsigned long long)h);

        if (symbol_bad != 0) {
            printf("SYMBOL_BAD=%d\n", symbol_bad);
            printf("prefix:");
            print_vec_prefix(&d, 60);
            unmatched++;
        } else if (match >= 0) {
            printf("MATCHES S%d\n", match);
            matched++;
        } else {
            printf("NO_S_WORD_MATCH\n");
            printf("prefix:");
            print_vec_prefix(&d, 60);
            unmatched++;
        }
    }

    printf("\nRESULT SUMMARY:\n");
    printf("checked_words=%d\n", K);
    printf("symbol_bad_total=%d\n", total_symbol_bad);
    printf("matched_S_words=%d\n", matched);
    printf("unmatched=%d\n", unmatched);

    if (total_symbol_bad == 0 && unmatched == 0) {
        printf("\nRESULT: 4-STEP CYCLE COMPOSITION PATTERN VERIFIED\n");
        printf("Every composed V0..V%d expansion is exactly an S-word.\n", K - 1);
    } else {
        printf("\nRESULT: 4-STEP CYCLE COMPOSITION PATTERN FAILED\n");
        printf("The expansion lands inside the S alphabet, but not always on an existing S-word.\n");
    }

    vec_free(&a);
    vec_free(&b);
    vec_free(&c);
    vec_free(&d);

    free_cert(&S);
    free_cert(&T);
    free_cert(&U);
    free_cert(&V);

    return (total_symbol_bad == 0 && unmatched == 0) ? 0 : 1;
}