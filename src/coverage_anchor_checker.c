/*
    coverage_anchor_checker.c

    Symbolic coverage and anchoring checker for the certified
    finite-state proof.

    Expected compact certificate format:

        SWORDS 53
        S 0 7 1 1 4 5 6 1 3 COUNT 1
        S 1 1 1 COUNT 10848
        ...
        S_SEQUENCE 21535

    Analogously for T, U, V.

    This checker verifies:

        1. S/T/U/V word systems parse correctly.
        2. Header counts match parsed counts.
        3. Symbol bounds:
              S words are over S symbols,
              T words are over S symbols,
              U words are over T symbols,
              V words are over U symbols.
        4. Stable prefix:
              V_i has same raw index pattern as S_i for 0 <= i < K.
        5. Correct full composition:
              V_i
                -> U indices
                -> T indices
                -> S factor indices
                -> raw S-symbol word.
        6. The raw S-symbol word factors completely into certified S-words.

    Usage:

        coverage_anchor_checker s_certificate.txt t_certificate.txt u_certificate.txt v_certificate.txt K

    Example:

        coverage_anchor_checker s_certificate.txt t_certificate.txt u_certificate.txt v_certificate.txt 28

    Compile:

        gcc -O2 -std=c11 -Wall -Wextra -o coverage_anchor_checker coverage_anchor_checker.c

    Arithmetic correctness and strict backwardness are intentionally not
    checked here.  They should be checked separately by:

        well_definedness_induction_checker.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <limits.h>

#define LINE_BUF 65536
#define INITIAL_CAP 16

#define HASH_OFFSET 1469598103934665603ULL
#define HASH_PRIME 1099511628211ULL

typedef struct {
    int *a;
    int n;
    int cap;
} IntVec;

typedef struct {
    int id;
    long long count;
    IntVec symbols;
    int present;
} Word;

typedef struct {
    char prefix;
    Word *words;
    int count;
    int cap;
    int max_id;
    long long parsed_words;
    int header_count;
} WordSystem;

static void die(const char *msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) die("out of memory");
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q) die("out of memory");
    return q;
}

static char *ltrim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static void rtrim_inplace(char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[n - 1] = '\0';
        n--;
    }
}

static void ivec_init(IntVec *v) {
    v->a = NULL;
    v->n = 0;
    v->cap = 0;
}

static void ivec_push(IntVec *v, int x) {
    if (v->n == v->cap) {
        int new_cap = v->cap ? v->cap * 2 : INITIAL_CAP;
        v->a = (int *)xrealloc(v->a, (size_t)new_cap * sizeof(int));
        v->cap = new_cap;
    }
    v->a[v->n++] = x;
}

static void ivec_copy(IntVec *dst, const IntVec *src) {
    ivec_init(dst);
    for (int i = 0; i < src->n; i++) {
        ivec_push(dst, src->a[i]);
    }
}

static void ivec_free(IntVec *v) {
    free(v->a);
    v->a = NULL;
    v->n = 0;
    v->cap = 0;
}

static int ivec_equal(const IntVec *a, const IntVec *b) {
    if (a->n != b->n) return 0;
    for (int i = 0; i < a->n; i++) {
        if (a->a[i] != b->a[i]) return 0;
    }
    return 1;
}

static uint64_t hash_intvec(const IntVec *v) {
    uint64_t h = HASH_OFFSET;

    for (int i = 0; i < v->n; i++) {
        uint64_t x = (uint64_t)(uint32_t)v->a[i];

        h ^= x;
        h *= HASH_PRIME;

        h ^= 0x9e3779b97f4a7c15ULL;
        h *= HASH_PRIME;
    }

    return h;
}

static void wordsys_init(WordSystem *ws, char prefix) {
    ws->prefix = prefix;
    ws->words = NULL;
    ws->count = 0;
    ws->cap = 0;
    ws->max_id = -1;
    ws->parsed_words = 0;
    ws->header_count = -1;
}

static void wordsys_ensure(WordSystem *ws, int id) {
    if (id < 0) die("negative word id");

    if (id >= ws->cap) {
        int old_cap = ws->cap;
        int new_cap = ws->cap ? ws->cap : INITIAL_CAP;

        while (new_cap <= id) {
            new_cap *= 2;
        }

        ws->words = (Word *)xrealloc(ws->words, (size_t)new_cap * sizeof(Word));

        for (int i = old_cap; i < new_cap; i++) {
            ws->words[i].id = i;
            ws->words[i].count = 0;
            ws->words[i].present = 0;
            ivec_init(&ws->words[i].symbols);
        }

        ws->cap = new_cap;
    }

    if (id > ws->max_id) {
        ws->max_id = id;
    }
}

static void wordsys_set_word(WordSystem *ws, int id, long long count, const IntVec *symbols) {
    wordsys_ensure(ws, id);

    Word *w = &ws->words[id];

    if (w->present) {
        fprintf(stderr, "ERROR: duplicate %c%d\n", ws->prefix, id);
        exit(1);
    }

    w->id = id;
    w->count = count;
    w->present = 1;

    for (int i = 0; i < symbols->n; i++) {
        ivec_push(&w->symbols, symbols->a[i]);
    }

    ws->parsed_words++;
}

static void wordsys_finalize(WordSystem *ws, const char *path) {
    ws->count = ws->max_id + 1;

    if (ws->count <= 0) {
        fprintf(stderr, "ERROR: no %c-words parsed from %s\n", ws->prefix, path);
        exit(1);
    }

    if (ws->header_count > 0 && ws->count != ws->header_count) {
        fprintf(stderr,
                "ERROR: %cWORDS header mismatch in %s: header=%d parsed_count=%d\n",
                ws->prefix,
                path,
                ws->header_count,
                ws->count);
        exit(1);
    }

    for (int i = 0; i < ws->count; i++) {
        if (!ws->words[i].present) {
            fprintf(stderr, "ERROR: missing %c%d in %s\n", ws->prefix, i, path);
            exit(1);
        }
    }
}

static void wordsys_free(WordSystem *ws) {
    if (ws->words) {
        for (int i = 0; i < ws->cap; i++) {
            ivec_free(&ws->words[i].symbols);
        }
        free(ws->words);
    }

    ws->words = NULL;
    ws->count = 0;
    ws->cap = 0;
    ws->max_id = -1;
    ws->parsed_words = 0;
    ws->header_count = -1;
}

static int parse_header_line(char *line, char prefix, int *header_count_out) {
    char *s = ltrim(line);
    rtrim_inplace(s);

    char expected[32];
    snprintf(expected, sizeof(expected), "%cWORDS", prefix);

    char tag[64];
    int n = -1;

    if (sscanf(s, "%63s %d", tag, &n) == 2) {
        if (strcmp(tag, expected) == 0 && n > 0) {
            *header_count_out = n;
            return 1;
        }
    }

    return 0;
}

static int is_sequence_header(char *line, char prefix) {
    char *s = ltrim(line);
    rtrim_inplace(s);

    char expected[64];
    snprintf(expected, sizeof(expected), "%c_SEQUENCE", prefix);

    return strncmp(s, expected, strlen(expected)) == 0;
}

static int parse_compact_word_line(
    char *line,
    char prefix,
    int *id_out,
    long long *count_out,
    IntVec *symbols_out
) {
    /*
        Expected format:

            S 0 7 1 1 4 5 6 1 3 COUNT 1

        Meaning:

            prefix = S
            id     = 0
            len    = 7
            word   = 1 1 4 5 6 1 3
            count  = 1
    */

    char *s = ltrim(line);
    rtrim_inplace(s);

    if (s[0] != prefix || !isspace((unsigned char)s[1])) {
        return 0;
    }

    char *tok = strtok(s, " \t\r\n");
    if (!tok) return 0;

    if (tok[0] != prefix || tok[1] != '\0') {
        return 0;
    }

    tok = strtok(NULL, " \t\r\n");
    if (!tok) return 0;
    int id = atoi(tok);

    tok = strtok(NULL, " \t\r\n");
    if (!tok) return 0;
    int len = atoi(tok);

    if (id < 0 || len < 0) {
        return 0;
    }

    ivec_init(symbols_out);

    for (int i = 0; i < len; i++) {
        tok = strtok(NULL, " \t\r\n");
        if (!tok) {
            ivec_free(symbols_out);
            return 0;
        }

        char *end = NULL;
        long sym_long = strtol(tok, &end, 10);

        if (end == tok || *end != '\0' || sym_long < 0 || sym_long > INT_MAX) {
            ivec_free(symbols_out);
            return 0;
        }

        ivec_push(symbols_out, (int)sym_long);
    }

    tok = strtok(NULL, " \t\r\n");
    if (!tok || strcmp(tok, "COUNT") != 0) {
        ivec_free(symbols_out);
        return 0;
    }

    tok = strtok(NULL, " \t\r\n");
    if (!tok) {
        ivec_free(symbols_out);
        return 0;
    }

    char *end = NULL;
    long long count = strtoll(tok, &end, 10);

    if (end == tok || *end != '\0' || count < 0) {
        ivec_free(symbols_out);
        return 0;
    }

    *id_out = id;
    *count_out = count;

    return 1;
}

static void parse_word_certificate(const char *path, char prefix, WordSystem *ws) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: cannot open word certificate: %s\n", path);
        exit(1);
    }

    wordsys_init(ws, prefix);

    char line[LINE_BUF];
    long long line_no = 0;

    while (fgets(line, sizeof(line), f)) {
        line_no++;

        char line_for_header[LINE_BUF];
        strncpy(line_for_header, line, sizeof(line_for_header) - 1);
        line_for_header[sizeof(line_for_header) - 1] = '\0';

        int header_count = -1;
        if (parse_header_line(line_for_header, prefix, &header_count)) {
            ws->header_count = header_count;
            continue;
        }

        char line_for_seq[LINE_BUF];
        strncpy(line_for_seq, line, sizeof(line_for_seq) - 1);
        line_for_seq[sizeof(line_for_seq) - 1] = '\0';

        if (is_sequence_header(line_for_seq, prefix)) {
            break;
        }

        char copy[LINE_BUF];
        strncpy(copy, line, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';

        int id = -1;
        long long count = 0;
        IntVec syms;

        if (parse_compact_word_line(copy, prefix, &id, &count, &syms)) {
            if (syms.n <= 0) {
                fprintf(stderr,
                        "ERROR: empty %c-word in %s at line %lld\n",
                        prefix,
                        path,
                        line_no);
                exit(1);
            }

            wordsys_set_word(ws, id, count, &syms);
            ivec_free(&syms);
        }
    }

    fclose(f);

    wordsys_finalize(ws, path);
}

static void print_word_system_summary(const WordSystem *ws, const char *path) {
    printf("%c certificate: file=%s\n", ws->prefix, path);
    printf("  header_count=%d\n", ws->header_count);
    printf("  parsed_count=%d\n", ws->count);
    printf("  parsed_words=%lld\n", ws->parsed_words);
}

static int check_symbol_bounds(const WordSystem *from, const WordSystem *to) {
    int bad = 0;

    for (int i = 0; i < from->count; i++) {
        const Word *w = &from->words[i];

        for (int j = 0; j < w->symbols.n; j++) {
            int sym = w->symbols.a[j];

            if (sym < 0 || sym >= to->count) {
                bad++;

                if (bad <= 20) {
                    fprintf(stderr,
                            "BAD symbol bound: %c%d uses %d at pos=%d, but %c alphabet is 0..%d\n",
                            from->prefix,
                            i,
                            sym,
                            j,
                            to->prefix,
                            to->count - 1);
                }
            }
        }
    }

    return bad;
}

static int check_stable_prefix(const WordSystem *S, const WordSystem *V, int K) {
    int bad = 0;

    if (K > S->count) K = S->count;
    if (K > V->count) K = V->count;

    for (int i = 0; i < K; i++) {
        if (!ivec_equal(&S->words[i].symbols, &V->words[i].symbols)) {
            bad++;

            if (bad <= 20) {
                fprintf(stderr,
                        "BAD stable prefix at id=%d: S_len=%d V_len=%d\n",
                        i,
                        S->words[i].symbols.n,
                        V->words[i].symbols.n);

                fprintf(stderr, "  S%d:", i);
                for (int j = 0; j < S->words[i].symbols.n && j < 80; j++) {
                    fprintf(stderr, " %d", S->words[i].symbols.a[j]);
                }
                if (S->words[i].symbols.n > 80) fprintf(stderr, " ...");
                fprintf(stderr, "\n");

                fprintf(stderr, "  V%d:", i);
                for (int j = 0; j < V->words[i].symbols.n && j < 80; j++) {
                    fprintf(stderr, " %d", V->words[i].symbols.a[j]);
                }
                if (V->words[i].symbols.n > 80) fprintf(stderr, " ...");
                fprintf(stderr, "\n");
            }
        }
    }

    return bad;
}

static void expand_indices_through_system(
    const WordSystem *from,
    const IntVec *input_indices,
    IntVec *output_indices
) {
    /*
        input_indices is a word over 'from' symbols.
        For each symbol x, append from->words[x].symbols.
    */

    ivec_init(output_indices);

    for (int i = 0; i < input_indices->n; i++) {
        int idx = input_indices->a[i];

        if (idx < 0 || idx >= from->count || !from->words[idx].present) {
            fprintf(stderr,
                    "ERROR: invalid %c-symbol %d at input position %d\n",
                    from->prefix,
                    idx,
                    i);
            exit(1);
        }

        const Word *w = &from->words[idx];

        for (int j = 0; j < w->symbols.n; j++) {
            ivec_push(output_indices, w->symbols.a[j]);
        }
    }
}

static void compose_V_to_raw_S_symbols(
    const WordSystem *S,
    const WordSystem *T,
    const WordSystem *U,
    const WordSystem *V,
    int vid,
    IntVec *out_raw_S_symbols,
    IntVec *out_natural_S_factors
) {
    /*
        Correct full expansion:

            V_i
              -> U indices
              -> T indices
              -> S factor indices
              -> raw S-symbol word

        The intermediate S factor index list is already a natural
        factorization.  We still expand it to a raw S-symbol word and
        independently factor that raw word back into certified S-words.
    */

    if (vid < 0 || vid >= V->count || !V->words[vid].present) {
        die("invalid V word id in compose_V_to_raw_S_symbols");
    }

    IntVec t_symbols;
    IntVec s_factor_indices;
    IntVec raw_S_symbols;

    expand_indices_through_system(U, &V->words[vid].symbols, &t_symbols);
    expand_indices_through_system(T, &t_symbols, &s_factor_indices);
    expand_indices_through_system(S, &s_factor_indices, &raw_S_symbols);

    ivec_free(&t_symbols);

    *out_raw_S_symbols = raw_S_symbols;
    *out_natural_S_factors = s_factor_indices;
}

static int word_matches_at(const IntVec *text, int pos, const IntVec *pat) {
    if (pos < 0) return 0;
    if (pos + pat->n > text->n) return 0;

    for (int i = 0; i < pat->n; i++) {
        if (text->a[pos + i] != pat->a[i]) {
            return 0;
        }
    }

    return 1;
}

static int factor_into_S_words(const WordSystem *S, const IntVec *text, IntVec *factor_ids) {
    int n = text->n;

    int *prev = (int *)xmalloc((size_t)(n + 1) * sizeof(int));
    int *which = (int *)xmalloc((size_t)(n + 1) * sizeof(int));

    for (int i = 0; i <= n; i++) {
        prev[i] = -1;
        which[i] = -1;
    }

    prev[0] = 0;

    /*
        Dynamic programming factorization.

        dp[pos] = reachable.
        If reachable, try every certified S-word at pos.
    */

    for (int pos = 0; pos < n; pos++) {
        if (prev[pos] < 0) continue;

        for (int sid = 0; sid < S->count; sid++) {
            const IntVec *pat = &S->words[sid].symbols;
            if (pat->n <= 0) continue;

            int next = pos + pat->n;

            if (next <= n && prev[next] < 0 && word_matches_at(text, pos, pat)) {
                prev[next] = pos;
                which[next] = sid;
            }
        }
    }

    if (prev[n] < 0) {
        free(prev);
        free(which);
        return 0;
    }

    ivec_init(factor_ids);

    int cur = n;

    while (cur > 0) {
        int sid = which[cur];

        if (sid < 0) {
            ivec_free(factor_ids);
            free(prev);
            free(which);
            return 0;
        }

        ivec_push(factor_ids, sid);
        cur = prev[cur];
    }

    /*
        Reverse factor list.
    */

    for (int i = 0, j = factor_ids->n - 1; i < j; i++, j--) {
        int tmp = factor_ids->a[i];
        factor_ids->a[i] = factor_ids->a[j];
        factor_ids->a[j] = tmp;
    }

    free(prev);
    free(which);

    return 1;
}

static void print_prefix_factors(const IntVec *factors, int max_print) {
    int n = factors->n < max_print ? factors->n : max_print;

    for (int i = 0; i < n; i++) {
        printf(" S%d", factors->a[i]);
    }

    if (factors->n > max_print) {
        printf(" ...");
    }
}

static void print_prefix_raw(const IntVec *raw, int max_print) {
    int n = raw->n < max_print ? raw->n : max_print;

    for (int i = 0; i < n; i++) {
        printf(" %d", raw->a[i]);
    }

    if (raw->n > max_print) {
        printf(" ...");
    }
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s s_certificate.txt t_certificate.txt u_certificate.txt v_certificate.txt K\n\n"
            "Example:\n"
            "  %s s_certificate.txt t_certificate.txt u_certificate.txt v_certificate.txt 28\n",
            prog,
            prog);
}

int main(int argc, char **argv) {
    if (argc != 6) {
        usage(argv[0]);
        return 1;
    }

    const char *s_path = argv[1];
    const char *t_path = argv[2];
    const char *u_path = argv[3];
    const char *v_path = argv[4];
    int K = atoi(argv[5]);

    if (K <= 0) {
        die("K must be positive");
    }

    printf("Symbolic coverage-anchor checker\n");
    printf("S=%s\n", s_path);
    printf("T=%s\n", t_path);
    printf("U=%s\n", u_path);
    printf("V=%s\n", v_path);
    printf("K=%d\n\n", K);

    WordSystem S, T, U, V;

    parse_word_certificate(s_path, 'S', &S);
    parse_word_certificate(t_path, 'T', &T);
    parse_word_certificate(u_path, 'U', &U);
    parse_word_certificate(v_path, 'V', &V);

    printf("Parsed word certificates:\n");
    print_word_system_summary(&S, s_path);
    print_word_system_summary(&T, t_path);
    print_word_system_summary(&U, u_path);
    print_word_system_summary(&V, v_path);
    printf("\n");

    if (K > S.count || K > V.count) {
        fprintf(stderr,
                "ERROR: K=%d exceeds available stable prefix range: S_count=%d V_count=%d\n",
                K,
                S.count,
                V.count);
        return 1;
    }

    /*
        Basic symbol-bound checks for the hierarchy.

        S-words are over S-symbol indices.
        T-words are over S-symbol indices.
        U-words are over T-symbol indices.
        V-words are over U-symbol indices.

        For stable-prefix equality, V_i and S_i are compared as raw index
        patterns.  This is intentional: the stable prefix equality is
        level-renaming equality.
    */

    int bad_S_bounds = check_symbol_bounds(&S, &S);
    int bad_T_bounds = check_symbol_bounds(&T, &S);
    int bad_U_bounds = check_symbol_bounds(&U, &T);
    int bad_V_bounds = check_symbol_bounds(&V, &U);

    printf("Symbol bound checks:\n");
    printf("S over S bad=%d\n", bad_S_bounds);
    printf("T over S bad=%d\n", bad_T_bounds);
    printf("U over T bad=%d\n", bad_U_bounds);
    printf("V over U bad=%d\n", bad_V_bounds);
    printf("\n");

    int prefix_bad = check_stable_prefix(&S, &V, K);

    printf("Stable prefix check:\n");
    printf("K=%d\n", K);
    printf("prefix_bad=%d\n", prefix_bad);
    printf("stable_prefix=%s\n\n", prefix_bad == 0 ? "OK" : "BAD");

    printf("Full composition and factorization:\n");

    int symbol_bad = 0;
    int factor_bad = 0;
    int factor_count_mismatch = 0;

    long long total_raw_symbols = 0;
    long long total_factors = 0;
    long long total_natural_factors = 0;

    int max_raw_len = 0;
    int max_factor_count = 0;
    int max_natural_factor_count = 0;
    int max_symbol_seen = -1;

    for (int i = 0; i < K; i++) {
        IntVec raw;
        IntVec natural_factors;

        compose_V_to_raw_S_symbols(&S, &T, &U, &V, i, &raw, &natural_factors);

        if (raw.n > max_raw_len) {
            max_raw_len = raw.n;
        }

        if (natural_factors.n > max_natural_factor_count) {
            max_natural_factor_count = natural_factors.n;
        }

        total_raw_symbols += raw.n;
        total_natural_factors += natural_factors.n;

        for (int j = 0; j < raw.n; j++) {
            int sym = raw.a[j];

            if (sym > max_symbol_seen) {
                max_symbol_seen = sym;
            }

            if (sym < 0 || sym >= S.count) {
                symbol_bad++;

                if (symbol_bad <= 20) {
                    fprintf(stderr,
                            "BAD final raw S-symbol: V%d raw position %d has symbol %d, but S alphabet is 0..%d\n",
                            i,
                            j,
                            sym,
                            S.count - 1);
                }
            }
        }

        IntVec factors;

        if (!factor_into_S_words(&S, &raw, &factors)) {
            factor_bad++;

            fprintf(stderr,
                    "BAD factorization: V%d raw_len=%d natural_factor_count=%d hash=%016llx\n",
                    i,
                    raw.n,
                    natural_factors.n,
                    (unsigned long long)hash_intvec(&raw));

            fprintf(stderr, "  raw prefix:");
            for (int j = 0; j < raw.n && j < 80; j++) {
                fprintf(stderr, " %d", raw.a[j]);
            }
            if (raw.n > 80) {
                fprintf(stderr, " ...");
            }
            fprintf(stderr, "\n");
        } else {
            total_factors += factors.n;

            if (factors.n > max_factor_count) {
                max_factor_count = factors.n;
            }

            if (factors.n != natural_factors.n) {
                factor_count_mismatch++;

                if (factor_count_mismatch <= 20) {
                    fprintf(stderr,
                            "WARNING: V%d DP factor_count=%d differs from natural_factor_count=%d\n",
                            i,
                            factors.n,
                            natural_factors.n);
                }
            }

            printf("V%d raw_len=%d factor_count=%d natural_factor_count=%d hash=%016llx status=OK prefix:",
                   i,
                   raw.n,
                   factors.n,
                   natural_factors.n,
                   (unsigned long long)hash_intvec(&raw));

            print_prefix_factors(&factors, 30);
            printf("\n");

            ivec_free(&factors);
        }

        ivec_free(&raw);
        ivec_free(&natural_factors);
    }

    printf("\nCoverage / factorization summary:\n");
    printf("checked_core_words=%d\n", K);
    printf("symbol_bad=%d\n", symbol_bad);
    printf("factor_bad=%d\n", factor_bad);
    printf("factor_count_mismatch=%d\n", factor_count_mismatch);
    printf("total_raw_symbols=%lld\n", total_raw_symbols);
    printf("total_factors=%lld\n", total_factors);
    printf("total_natural_factors=%lld\n", total_natural_factors);
    printf("max_raw_len=%d\n", max_raw_len);
    printf("max_factor_count=%d\n", max_factor_count);
    printf("max_natural_factor_count=%d\n", max_natural_factor_count);
    printf("max_symbol_seen=%d\n", max_symbol_seen);
    printf("\n");

    int ok_bounds =
        bad_S_bounds == 0 &&
        bad_T_bounds == 0 &&
        bad_U_bounds == 0 &&
        bad_V_bounds == 0;

    int ok_prefix = prefix_bad == 0;
    int ok_coverage = symbol_bad == 0 && factor_bad == 0;

    printf("RESULT SUMMARY:\n");
    printf("symbol_bounds=%s\n", ok_bounds ? "OK" : "BAD");
    printf("stable_prefix=%s\n", ok_prefix ? "OK" : "BAD");
    printf("composition_factorization=%s\n", ok_coverage ? "OK" : "BAD");

    if (factor_count_mismatch > 0) {
        printf("factor_count_mismatch_warning=%d\n", factor_count_mismatch);
    }

    if (ok_bounds && ok_prefix && ok_coverage) {
        printf("\nRESULT: SYMBOLIC COVERAGE AND ANCHORING VERIFIED\n");
        printf("The stable core V0..V%d matches the corresponding S-patterns.\n", K - 1);
        printf("Every checked V-word expands through V->U->T->S into raw S-symbols only.\n");
        printf("Every checked raw composed expansion factors into certified S-words.\n");
    } else {
        printf("\nRESULT: SYMBOLIC COVERAGE AND ANCHORING FAILED\n");

        if (!ok_bounds) {
            printf("Reason: symbol-bound violation in one of S/T/U/V systems.\n");
        }

        if (!ok_prefix) {
            printf("Reason: stable prefix mismatch between S and V.\n");
        }

        if (!ok_coverage) {
            printf("Reason: final raw S-symbol closure or S-word factorization failed.\n");
        }
    }

    wordsys_free(&S);
    wordsys_free(&T);
    wordsys_free(&U);
    wordsys_free(&V);

    return (ok_bounds && ok_prefix && ok_coverage) ? 0 : 1;
}