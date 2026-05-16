// faithfulness_checker.c
//
// Structural / faithfulness checker for certificate.txt.
//
// This checker verifies that the exported certificate is internally
// faithful to the symbolic format:
//   - WORDS section is well-formed.
//   - CONTEXTS section is well-formed.
//   - Context symbols are valid word ids.
//   - CENTER equals ctx[R].
//   - EXTENSIONS are correct radius-R shift transitions.
//   - EXTENSIONS are exhaustive for all matching context pairs.
//   - REALIZATIONS use the new format:
//       A rid context_id center L valid_count ...
//   - Each realization refers to a valid context.
//   - Each arithmetic target t lies inside the central return word.
//   - Each dependency symbol is a valid word id.
//   - Each dependency offset lies inside its dependency word.
//   - Each A-line contains exactly valid_count arithmetic records.
//
// This does not replace the arithmetic/backwardness/parity checker.
// Use both checkers.
//
// Compile:
//   gcc -O2 -std=c11 faithfulness_checker.c -o faithcheck
//
// Run:
//   ./faithcheck certificate.txt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#define R 20
#define CTX_LEN (2 * R + 1)
#define MAX_BAD_PRINT 20

typedef long long i64;

typedef struct {
    int len;
    int *w;
} Word;

typedef struct {
    int id;
    int ctx[CTX_LEN];
    int center_symbol;
    int count;
} Context;

static char *read_line_dynamic(FILE *f) {
    size_t cap = 4096;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    int c;
    while ((c = fgetc(f)) != EOF) {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nbuf = (char *)realloc(buf, cap);
            if (!nbuf) {
                free(buf);
                fprintf(stderr, "Out of memory\n");
                exit(1);
            }
            buf = nbuf;
        }

        buf[len++] = (char)c;

        if (c == '\n') break;
    }

    if (len == 0 && c == EOF) {
        free(buf);
        return NULL;
    }

    buf[len] = '\0';
    return buf;
}

static int parse_i64_token(char **saveptr, i64 *out) {
    char *tok = strtok_r(NULL, " \t\r\n", saveptr);
    if (!tok) return 0;

    errno = 0;
    char *end = NULL;
    long long v = strtoll(tok, &end, 10);

    if (errno != 0 || end == tok || *end != '\0') return 0;

    *out = (i64)v;
    return 1;
}

static int parse_int_token(char **saveptr, int *out) {
    i64 v;
    if (!parse_i64_token(saveptr, &v)) return 0;
    if (v < INT_MIN || v > INT_MAX) return 0;
    *out = (int)v;
    return 1;
}

static int suffix_prefix_match(Context *A, Context *B) {
    for (int i = 1; i < CTX_LEN; i++) {
        if (A->ctx[i] != B->ctx[i - 1]) return 0;
    }
    return 1;
}

static int shifted_context_id(Context *contexts, int context_count,
                              Context *A, Context *B) {
    int y[CTX_LEN];

    for (int i = 0; i < CTX_LEN - 1; i++) {
        y[i] = A->ctx[i + 1];
    }

    y[CTX_LEN - 1] = B->ctx[CTX_LEN - 1];

    for (int c = 0; c < context_count; c++) {
        int ok = 1;

        for (int i = 0; i < CTX_LEN; i++) {
            if (contexts[c].ctx[i] != y[i]) {
                ok = 0;
                break;
            }
        }

        if (ok) return c;
    }

    return -1;
}

int main(int argc, char **argv) {
    const char *filename = "certificate.txt";

    if (argc >= 2) {
        filename = argv[1];
    }

    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", filename);
        return 1;
    }

    printf("Faithfulness checker\n");
    printf("certificate=%s\n\n", filename);

    Word *words = NULL;
    Context *contexts = NULL;
    int **ext = NULL;

    int words_header = -1;
    int contexts_header = -1;
    int extensions_header = -1;
    int realizations_header = -1;

    int words_checked = 0;
    int contexts_checked = 0;
    int extensions_checked = 0;
    int realizations_checked = 0;
    i64 arithmetic_records_checked = 0;

    i64 malformed_bad = 0;
    i64 word_bad = 0;
    i64 context_bad = 0;
    i64 extension_bad = 0;
    i64 extension_missing_bad = 0;
    i64 realization_bad = 0;
    i64 record_bad = 0;
    i64 header_bad = 0;

    int bad_printed = 0;

    char *line = NULL;
    i64 line_no = 0;

    while ((line = read_line_dynamic(f)) != NULL) {
        line_no++;

        char *saveptr = NULL;
        char *tok = strtok_r(line, " \t\r\n", &saveptr);

        if (!tok || tok[0] == '#') {
            free(line);
            continue;
        }

        if (strcmp(tok, "WORDS") == 0) {
            if (!parse_int_token(&saveptr, &words_header) ||
                words_header <= 0) {

                malformed_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD malformed at line %lld: invalid WORDS header\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            words = (Word *)calloc((size_t)words_header, sizeof(Word));
            if (!words) {
                fprintf(stderr, "Out of memory\n");
                exit(1);
            }

            free(line);
            continue;
        }

        if (strcmp(tok, "W") == 0) {
            int id = 0;
            int len = 0;

            if (!parse_int_token(&saveptr, &id) ||
                !parse_int_token(&saveptr, &len) ||
                id < 0 ||
                id >= words_header ||
                len <= 0) {

                word_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD word at line %lld: invalid W header\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            if (words[id].len != 0) {
                word_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD word at line %lld: duplicate word id %d\n",
                           line_no, id);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            words[id].len = len;
            words[id].w = (int *)malloc((size_t)len * sizeof(int));
            if (!words[id].w) {
                fprintf(stderr, "Out of memory\n");
                exit(1);
            }

            for (int i = 0; i < len; i++) {
                if (!parse_int_token(&saveptr, &words[id].w[i])) {
                    word_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD word at line %lld: too few symbols in W %d\n",
                               line_no, id);
                        bad_printed++;
                    }

                    break;
                }
            }

            char *extra = strtok_r(NULL, " \t\r\n", &saveptr);
            if (extra != NULL) {
                word_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD word at line %lld: extra token in W %d: %s\n",
                           line_no, id, extra);
                    bad_printed++;
                }
            }

            words_checked++;

            free(line);
            continue;
        }

        if (strcmp(tok, "CONTEXTS") == 0) {
            if (!parse_int_token(&saveptr, &contexts_header) ||
                contexts_header <= 0) {

                malformed_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD malformed at line %lld: invalid CONTEXTS header\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            contexts = (Context *)calloc((size_t)contexts_header,
                                         sizeof(Context));
            if (!contexts) {
                fprintf(stderr, "Out of memory\n");
                exit(1);
            }

            for (int i = 0; i < contexts_header; i++) {
                contexts[i].id = -1;
            }

            free(line);
            continue;
        }

        if (strcmp(tok, "C") == 0) {
            int id = 0;

            if (!parse_int_token(&saveptr, &id) ||
                id < 0 ||
                id >= contexts_header) {

                context_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD context at line %lld: invalid context id\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            if (contexts[id].id != -1) {
                context_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD context at line %lld: duplicate context id %d\n",
                           line_no, id);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            contexts[id].id = id;

            for (int i = 0; i < CTX_LEN; i++) {
                if (!parse_int_token(&saveptr, &contexts[id].ctx[i])) {
                    context_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD context at line %lld: too few ctx entries for C %d\n",
                               line_no, id);
                        bad_printed++;
                    }

                    break;
                }

                if (contexts[id].ctx[i] < 0 ||
                    contexts[id].ctx[i] >= words_header) {

                    context_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD context at line %lld: ctx[%d]=%d out of word bounds\n",
                               line_no, i, contexts[id].ctx[i]);
                        bad_printed++;
                    }
                }
            }

            char *kw_center = strtok_r(NULL, " \t\r\n", &saveptr);
            if (!kw_center || strcmp(kw_center, "CENTER") != 0) {
                context_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD context at line %lld: missing CENTER keyword\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            if (!parse_int_token(&saveptr, &contexts[id].center_symbol)) {
                context_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD context at line %lld: missing CENTER value\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            char *kw_count = strtok_r(NULL, " \t\r\n", &saveptr);
            if (!kw_count || strcmp(kw_count, "COUNT") != 0) {
                context_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD context at line %lld: missing COUNT keyword\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            if (!parse_int_token(&saveptr, &contexts[id].count)) {
                context_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD context at line %lld: missing COUNT value\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            if (contexts[id].center_symbol != contexts[id].ctx[R]) {
                context_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD context at line %lld: CENTER=%d but ctx[R]=%d\n",
                           line_no,
                           contexts[id].center_symbol,
                           contexts[id].ctx[R]);
                    bad_printed++;
                }
            }

            if (contexts[id].count <= 0) {
                context_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD context at line %lld: non-positive COUNT=%d\n",
                           line_no,
                           contexts[id].count);
                    bad_printed++;
                }
            }

            char *extra = strtok_r(NULL, " \t\r\n", &saveptr);
            if (extra != NULL) {
                context_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD context at line %lld: extra token in C %d: %s\n",
                           line_no, id, extra);
                    bad_printed++;
                }
            }

            contexts_checked++;

            free(line);
            continue;
        }

        if (strcmp(tok, "EXTENSIONS") == 0) {
            if (!parse_int_token(&saveptr, &extensions_header) ||
                extensions_header < 0) {

                malformed_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD malformed at line %lld: invalid EXTENSIONS header\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            if (contexts_header <= 0) {
                malformed_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD malformed at line %lld: EXTENSIONS before CONTEXTS\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            ext = (int **)malloc((size_t)contexts_header * sizeof(int *));
            if (!ext) {
                fprintf(stderr, "Out of memory\n");
                exit(1);
            }

            for (int i = 0; i < contexts_header; i++) {
                ext[i] = (int *)malloc((size_t)contexts_header * sizeof(int));
                if (!ext[i]) {
                    fprintf(stderr, "Out of memory\n");
                    exit(1);
                }

                for (int j = 0; j < contexts_header; j++) {
                    ext[i][j] = -1;
                }
            }

            free(line);
            continue;
        }

        if (strcmp(tok, "E") == 0) {
            int a = 0;
            int b = 0;
            int c = 0;

            if (!parse_int_token(&saveptr, &a) ||
                !parse_int_token(&saveptr, &b) ||
                !parse_int_token(&saveptr, &c) ||
                a < 0 ||
                b < 0 ||
                c < 0 ||
                a >= contexts_header ||
                b >= contexts_header ||
                c >= contexts_header) {

                extension_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD extension at line %lld: invalid E fields\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            if (!suffix_prefix_match(&contexts[a], &contexts[b])) {
                extension_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD extension at line %lld: C%d and C%d do not suffix-prefix match\n",
                           line_no, a, b);
                    bad_printed++;
                }
            }

            int expected_c = shifted_context_id(contexts, contexts_header,
                                                &contexts[a], &contexts[b]);

            if (expected_c < 0 || expected_c != c) {
                extension_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD extension at line %lld: E %d %d %d expected %d\n",
                           line_no, a, b, c, expected_c);
                    bad_printed++;
                }
            }

            if (ext && ext[a][b] != -1) {
                extension_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD extension at line %lld: duplicate E %d %d\n",
                           line_no, a, b);
                    bad_printed++;
                }
            }

            if (ext) ext[a][b] = c;

            char *extra = strtok_r(NULL, " \t\r\n", &saveptr);
            if (extra != NULL) {
                extension_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD extension at line %lld: extra token: %s\n",
                           line_no, extra);
                    bad_printed++;
                }
            }

            extensions_checked++;

            free(line);
            continue;
        }

        if (strcmp(tok, "REALIZATIONS") == 0) {
            if (!parse_int_token(&saveptr, &realizations_header) ||
                realizations_header < 0) {

                malformed_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD malformed at line %lld: invalid REALIZATIONS header\n",
                           line_no);
                    bad_printed++;
                }
            }

            free(line);
            continue;
        }

        if (strcmp(tok, "A") == 0) {
            int rid = 0;
            int context_id = 0;
            int center = 0;
            i64 L = 0;
            int valid_count = 0;

            if (!parse_int_token(&saveptr, &rid) ||
                !parse_int_token(&saveptr, &context_id) ||
                !parse_int_token(&saveptr, &center) ||
                !parse_i64_token(&saveptr, &L) ||
                !parse_int_token(&saveptr, &valid_count)) {

                realization_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD realization at line %lld: invalid A header\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            if (rid != realizations_checked) {
                realization_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD realization at line %lld: rid=%d expected=%d\n",
                           line_no, rid, realizations_checked);
                    bad_printed++;
                }
            }

            if (context_id < 0 || context_id >= contexts_header) {
                realization_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD realization at line %lld: context_id=%d out of range\n",
                           line_no, context_id);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            if (center < 0) {
                realization_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD realization at line %lld: negative center=%d\n",
                           line_no, center);
                    bad_printed++;
                }
            }

            if (L <= 0) {
                realization_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD realization at line %lld: non-positive L=%lld\n",
                           line_no, L);
                    bad_printed++;
                }
            }

            if (valid_count < 0) {
                realization_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD realization at line %lld: negative valid_count=%d\n",
                           line_no, valid_count);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            int center_symbol = contexts[context_id].ctx[R];

            if (center_symbol < 0 || center_symbol >= words_header) {
                realization_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD realization at line %lld: central symbol out of bounds\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            int center_word_len = words[center_symbol].len;

            int records_seen = 0;

            for (int k = 0; k < valid_count; k++) {
                int t = 0;
                i64 qt = 0;
                int ra = 0;
                int offa = 0;
                int sigma_a = 0;
                i64 qa = 0;
                int rb = 0;
                int offb = 0;
                int sigma_b = 0;
                i64 qb = 0;
                int eps = 0;

                if (!parse_int_token(&saveptr, &t) ||
                    !parse_i64_token(&saveptr, &qt) ||
                    !parse_int_token(&saveptr, &ra) ||
                    !parse_int_token(&saveptr, &offa) ||
                    !parse_int_token(&saveptr, &sigma_a) ||
                    !parse_i64_token(&saveptr, &qa) ||
                    !parse_int_token(&saveptr, &rb) ||
                    !parse_int_token(&saveptr, &offb) ||
                    !parse_int_token(&saveptr, &sigma_b) ||
                    !parse_i64_token(&saveptr, &qb) ||
                    !parse_int_token(&saveptr, &eps)) {

                    record_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD record at line %lld: too few fields for record %d of %d\n",
                               line_no, k + 1, valid_count);
                        bad_printed++;
                    }

                    break;
                }

                records_seen++;
                arithmetic_records_checked++;

                if (t < 0 || t >= center_word_len) {
                    record_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD record at line %lld: t=%d outside central word length %d\n",
                               line_no, t, center_word_len);
                        bad_printed++;
                    }
                }

                if (sigma_a < 0 || sigma_a >= words_header) {
                    record_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD record at line %lld: sigma_a=%d out of range\n",
                               line_no, sigma_a);
                        bad_printed++;
                    }
                } else if (offa < 0 || offa >= words[sigma_a].len) {
                    record_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD record at line %lld: offa=%d outside word S%d length %d\n",
                               line_no, offa, sigma_a, words[sigma_a].len);
                        bad_printed++;
                    }
                }

                if (sigma_b < 0 || sigma_b >= words_header) {
                    record_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD record at line %lld: sigma_b=%d out of range\n",
                               line_no, sigma_b);
                        bad_printed++;
                    }
                } else if (offb < 0 || offb >= words[sigma_b].len) {
                    record_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD record at line %lld: offb=%d outside word S%d length %d\n",
                               line_no, offb, sigma_b, words[sigma_b].len);
                        bad_printed++;
                    }
                }

                if (eps != 1 && eps != -1) {
                    record_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD record at line %lld: eps=%d not in {-1,+1}\n",
                               line_no, eps);
                        bad_printed++;
                    }
                }

                (void)qt;
                (void)qa;
                (void)qb;
                (void)ra;
                (void)rb;
            }

            if (records_seen != valid_count) {
                record_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD realization at line %lld: records_seen=%d valid_count=%d\n",
                           line_no, records_seen, valid_count);
                    bad_printed++;
                }
            }

            char *extra = strtok_r(NULL, " \t\r\n", &saveptr);
            if (extra != NULL) {
                record_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD realization at line %lld: extra token after records: %s\n",
                           line_no, extra);
                    bad_printed++;
                }
            }

            realizations_checked++;

            free(line);
            continue;
        }

        malformed_bad++;

        if (bad_printed < MAX_BAD_PRINT) {
            printf("BAD malformed at line %lld: unknown record type '%s'\n",
                   line_no, tok);
            bad_printed++;
        }

        free(line);
    }

    fclose(f);

    if (words_header >= 0 && words_checked != words_header) header_bad++;
    if (contexts_header >= 0 && contexts_checked != contexts_header) header_bad++;
    if (extensions_header >= 0 && extensions_checked != extensions_header) header_bad++;
    if (realizations_header >= 0 && realizations_checked != realizations_header) header_bad++;

    if (contexts && ext) {
        int computed_extensions = 0;

        for (int a = 0; a < contexts_header; a++) {
            for (int b = 0; b < contexts_header; b++) {
                if (!suffix_prefix_match(&contexts[a], &contexts[b])) {
                    continue;
                }

                int c = shifted_context_id(contexts, contexts_header,
                                           &contexts[a], &contexts[b]);

                if (c < 0) {
                    extension_missing_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD extension completeness: no shifted context for pair %d,%d\n",
                               a, b);
                        bad_printed++;
                    }

                    continue;
                }

                computed_extensions++;

                if (ext[a][b] != c) {
                    extension_missing_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD extension completeness: missing E %d %d %d, stored=%d\n",
                               a, b, c, ext[a][b]);
                        bad_printed++;
                    }
                }
            }
        }

        if (computed_extensions != extensions_checked) {
            extension_missing_bad++;

            if (bad_printed < MAX_BAD_PRINT) {
                printf("BAD extension count: computed=%d exported=%d\n",
                       computed_extensions, extensions_checked);
                bad_printed++;
            }
        }
    }

    printf("Faithfulness certificate:\n");
    printf("words_header=%d\n", words_header);
    printf("words_checked=%d\n", words_checked);
    printf("contexts_header=%d\n", contexts_header);
    printf("contexts_checked=%d\n", contexts_checked);
    printf("extensions_header=%d\n", extensions_header);
    printf("extensions_checked=%d\n", extensions_checked);
    printf("realizations_header=%d\n", realizations_header);
    printf("realizations_checked=%d\n", realizations_checked);
    printf("arithmetic_records_checked=%lld\n", arithmetic_records_checked);
    printf("malformed_bad=%lld\n", malformed_bad);
    printf("word_bad=%lld\n", word_bad);
    printf("context_bad=%lld\n", context_bad);
    printf("extension_bad=%lld\n", extension_bad);
    printf("extension_missing_bad=%lld\n", extension_missing_bad);
    printf("realization_bad=%lld\n", realization_bad);
    printf("record_bad=%lld\n", record_bad);
    printf("header_bad=%lld\n", header_bad);

    int ok =
        malformed_bad == 0 &&
        word_bad == 0 &&
        context_bad == 0 &&
        extension_bad == 0 &&
        extension_missing_bad == 0 &&
        realization_bad == 0 &&
        record_bad == 0 &&
        header_bad == 0;

    if (words) {
        for (int i = 0; i < words_header; i++) {
            free(words[i].w);
        }
        free(words);
    }

    free(contexts);

    if (ext) {
        for (int i = 0; i < contexts_header; i++) {
            free(ext[i]);
        }
        free(ext);
    }

    if (ok) {
        printf("\nRESULT: FAITHFULNESS CERTIFICATE VERIFIED\n");
        return 0;
    }

    printf("\nRESULT: FAITHFULNESS CERTIFICATE FAILED\n");
    return 1;
}