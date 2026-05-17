// cycle_composition_factor_checker.c
//
// Prüft:
//
//   expandiere V_i über U,T,S
//   und faktorisiere die Endfolge als Konkatenation von S-Wörtern.
//
// Compile:
//   gcc -O3 -std=c11 cycle_composition_factor_checker.c -o cfactor
//
// Run:
//   ./cfactor s_certificate.txt S t_certificate.txt T u_certificate.txt U v_certificate.txt V 28
//
// Optional:
//   ./cfactor ... 28 53

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_WORDS 100000
#define MAX_EXPANSION 50000000
#define MAX_FACTORS 10000000

typedef struct {
    int used;
    int id;
    int len;
    int *w;
    int count;
    uint64_t hash;
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

uint64_t fnv_hash_ints(const int *a, int len) {
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < len; i++) {
        h ^= (uint64_t)(a[i] + 1000003);
        h *= 1099511628211ULL;
    }
    return h;
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
            W->hash = fnv_hash_ints(W->w, W->len);
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

int word_matches_at(Vec *v, int pos, Word *W) {
    if (!W->used) return 0;
    if (pos + W->len > v->len) return 0;

    for (int i = 0; i < W->len; i++) {
        if (v->a[pos + i] != W->w[i]) return 0;
    }

    return 1;
}

int find_match_at(Vec *v, int pos, Cert *S, int limit) {
    for (int i = 0; i < limit; i++) {
        if (word_matches_at(v, pos, &S->words[i])) {
            return i;
        }
    }

    return -1;
}

int factor_greedy_longest(Vec *v, Cert *S, int limit, int *factors, int *factor_count) {
    int pos = 0;
    *factor_count = 0;

    while (pos < v->len) {
        int best = -1;
        int best_len = -1;

        for (int i = 0; i < limit; i++) {
            Word *W = &S->words[i];

            if (!W->used) continue;
            if (W->len <= best_len) continue;
            if (pos + W->len > v->len) continue;

            if (word_matches_at(v, pos, W)) {
                best = i;
                best_len = W->len;
            }
        }

        if (best < 0) {
            return pos;
        }

        if (*factor_count >= MAX_FACTORS)
            die("too many factors");

        factors[(*factor_count)++] = best;
        pos += best_len;
    }

    return -1;
}

int factor_dp(Vec *v, Cert *S, int limit, int *factors, int *factor_count) {
    int n = v->len;

    unsigned char *reachable = calloc((size_t)n + 1, 1);
    int *prev_pos = malloc(sizeof(int) * ((size_t)n + 1));
    int *prev_word = malloc(sizeof(int) * ((size_t)n + 1));

    if (!reachable || !prev_pos || !prev_word)
        die("out of memory DP");

    for (int i = 0; i <= n; i++) {
        prev_pos[i] = -1;
        prev_word[i] = -1;
    }

    reachable[0] = 1;

    for (int pos = 0; pos < n; pos++) {
        if (!reachable[pos]) continue;

        for (int w = 0; w < limit; w++) {
            Word *W = &S->words[w];

            if (!W->used) continue;
            int np = pos + W->len;
            if (np > n) continue;

            if (!reachable[np] && word_matches_at(v, pos, W)) {
                reachable[np] = 1;
                prev_pos[np] = pos;
                prev_word[np] = w;

                if (np == n) break;
            }
        }
    }

    if (!reachable[n]) {
        int far = 0;
        for (int i = 0; i <= n; i++) {
            if (reachable[i]) far = i;
        }

        free(reachable);
        free(prev_pos);
        free(prev_word);

        return far;
    }

    int tmp_count = 0;
    int p = n;

    while (p > 0) {
        if (tmp_count >= MAX_FACTORS)
            die("too many factors traceback");

        factors[tmp_count++] = prev_word[p];
        p = prev_pos[p];
    }

    *factor_count = 0;

    for (int i = tmp_count - 1; i >= 0; i--) {
        factors[(*factor_count)++] = factors[i];
    }

    free(reachable);
    free(prev_pos);
    free(prev_word);

    return -1;
}

int check_symbols(Vec *v, int limit) {
    int bad = 0;

    for (int i = 0; i < v->len; i++) {
        if (v->a[i] < 0 || v->a[i] >= limit)
            bad++;
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

void print_factor_prefix(int *factors, int factor_count, int maxn) {
    int n = factor_count < maxn ? factor_count : maxn;

    for (int i = 0; i < n; i++) {
        printf(" S%d", factors[i]);
    }

    if (factor_count > maxn) printf(" ...");
    printf("\n");
}

void print_vec_window(Vec *v, int pos, int radius) {
    int a = pos - radius;
    int b = pos + radius;

    if (a < 0) a = 0;
    if (b > v->len) b = v->len;

    printf("window at pos=%d:", pos);

    for (int i = a; i < b; i++) {
        if (i == pos) printf(" |");
        printf(" %d", v->a[i]);
    }

    printf("\n");
}

int main(int argc, char **argv) {
    if (argc != 10 && argc != 11) {
        fprintf(stderr,
                "Usage:\n"
                "  %s s_certificate.txt S t_certificate.txt T u_certificate.txt U v_certificate.txt V K [S_limit]\n",
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

    int S_limit = S.count;
    if (argc == 11) {
        S_limit = atoi(argv[10]);
        if (S_limit <= 0 || S_limit > S.count) die("bad S_limit");
    }

    printf("Parsed cycle certificates:\n");
    printf("S_count=%d\n", S.count);
    printf("T_count=%d\n", T.count);
    printf("U_count=%d\n", U.count);
    printf("V_count=%d\n", V.count);
    printf("K=%d\n", K);
    printf("S_limit=%d\n\n", S_limit);

    Vec a, b, c, d;
    vec_init(&a);
    vec_init(&b);
    vec_init(&c);
    vec_init(&d);

    int *factors = malloc(sizeof(int) * MAX_FACTORS);
    if (!factors) die("out of memory factors");

    int ok = 0;
    int fail = 0;
    int symbol_bad_total = 0;

    for (int id = 0; id < K; id++) {
        seed_word(&a, &V, id);

        expand_once(&a, &b, &U);
        expand_once(&b, &c, &T);
        expand_once(&c, &d, &S);

        int symbol_bad = check_symbols(&d, S_limit);
        symbol_bad_total += symbol_bad;

        printf("V%d composed_len=%d max_symbol=%d ",
               id, d.len, max_symbol(&d));

        if (symbol_bad > 0) {
            printf("SYMBOL_BAD=%d\n", symbol_bad);
            fail++;
            continue;
        }

        int factor_count = 0;

        int badpos = factor_greedy_longest(&d, &S, S_limit, factors, &factor_count);

        if (badpos >= 0) {
            printf("greedy_failed_at=%d; trying DP... ", badpos);

            badpos = factor_dp(&d, &S, S_limit, factors, &factor_count);
        }

        if (badpos >= 0) {
            printf("FACTOR_FAILED farthest=%d\n", badpos);
            print_vec_window(&d, badpos, 30);
            fail++;
        } else {
            printf("FACTORED factor_count=%d prefix:", factor_count);
            print_factor_prefix(factors, factor_count, 30);
            ok++;
        }
    }

    printf("\nRESULT SUMMARY:\n");
    printf("checked_words=%d\n", K);
    printf("factored=%d\n", ok);
    printf("failed=%d\n", fail);
    printf("symbol_bad_total=%d\n", symbol_bad_total);

    if (fail == 0 && symbol_bad_total == 0) {
        printf("\nRESULT: 4-STEP CYCLE COMPOSITION FACTORIZATION VERIFIED\n");
        printf("Every composed V0..V%d expansion factors into certified S0..S%d words.\n",
               K - 1, S_limit - 1);
    } else {
        printf("\nRESULT: 4-STEP CYCLE COMPOSITION FACTORIZATION FAILED\n");
    }

    free(factors);

    vec_free(&a);
    vec_free(&b);
    vec_free(&c);
    vec_free(&d);

    free_cert(&S);
    free_cert(&T);
    free_cert(&U);
    free_cert(&V);

    return (fail == 0 && symbol_bad_total == 0) ? 0 : 1;
}
