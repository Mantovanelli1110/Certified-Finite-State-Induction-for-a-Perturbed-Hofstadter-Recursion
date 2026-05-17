// well_definedness_induction_checker.c
//
// Checks the arithmetic / backwardness / parity part of certificate.txt.
//
// Expected certificate format for realization lines:
//
//   A rid context_id center L valid_count
//       t qt ra offa sigma_a qa rb offb sigma_b qb eps
//       ...
//
// Each arithmetic record has 11 fields:
//
//   t qt ra offa sigma_a qa rb offb sigma_b qb eps
//
// Checks:
//   qt = qa + qb + eps
//   ra < 0 and rb < 0
//   eps = (-1)^(L+t)
//   eps in {-1,+1}
//   record count matches valid_count
//
// Compile:
//   gcc -O2 -std=c11 well_definedness_induction_checker.c -o wdcheck
//
// Run:
//   ./wdcheck certificate.txt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#define MAX_BAD_PRINT 10

typedef long long i64;

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

static int expected_parity(i64 n) {
    return ((n & 1LL) == 0) ? 1 : -1;
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

    printf("Well-definedness induction checker\n");
    printf("certificate=%s\n\n", filename);

    i64 words_header = -1;
    i64 contexts_header = -1;
    i64 extensions_header = -1;
    i64 realizations_header = -1;

    i64 words_checked = 0;
    i64 contexts_checked = 0;
    i64 extensions_checked = 0;
    i64 realizations_checked = 0;
    i64 arithmetic_records_checked = 0;

    i64 arithmetic_bad = 0;
    i64 non_backward_bad = 0;
    i64 parity_bad = 0;
    i64 malformed_bad = 0;
    i64 header_bad = 0;

    int have_rel = 0;
    int min_rel_seen = 0;
    int max_rel_seen = 0;

    int have_target = 0;
    int min_target_seen = 0;
    int max_target_seen = 0;

    int have_global_target = 0;
    i64 min_global_target_seen = 0;
    i64 max_global_target_seen = 0;

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
            if (!parse_i64_token(&saveptr, &words_header)) {
                malformed_bad++;
            }
            free(line);
            continue;
        }

        if (strcmp(tok, "W") == 0) {
            words_checked++;
            free(line);
            continue;
        }

        if (strcmp(tok, "CONTEXTS") == 0) {
            if (!parse_i64_token(&saveptr, &contexts_header)) {
                malformed_bad++;
            }
            free(line);
            continue;
        }

        if (strcmp(tok, "C") == 0) {
            contexts_checked++;
            free(line);
            continue;
        }

        if (strcmp(tok, "EXTENSIONS") == 0) {
            if (!parse_i64_token(&saveptr, &extensions_header)) {
                malformed_bad++;
            }
            free(line);
            continue;
        }

        if (strcmp(tok, "E") == 0) {
            extensions_checked++;
            free(line);
            continue;
        }

        if (strcmp(tok, "REALIZATIONS") == 0) {
            if (!parse_i64_token(&saveptr, &realizations_header)) {
                malformed_bad++;
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

                malformed_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD malformed at line %lld: cannot parse A header\n",
                           line_no);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            if (valid_count < 0) {
                malformed_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD malformed at line %lld: negative valid_count=%d\n",
                           line_no, valid_count);
                    bad_printed++;
                }

                free(line);
                continue;
            }

            int records_seen_in_line = 0;

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

                    malformed_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD malformed at line %lld: too few fields for arithmetic record %d of %d\n",
                               line_no, k + 1, valid_count);
                        bad_printed++;
                    }

                    break;
                }

                records_seen_in_line++;
                arithmetic_records_checked++;

                if (!have_target) {
                    min_target_seen = max_target_seen = t;
                    have_target = 1;
                } else {
                    if (t < min_target_seen) min_target_seen = t;
                    if (t > max_target_seen) max_target_seen = t;
                }

                i64 global_target = L + (i64)t;

                if (!have_global_target) {
                    min_global_target_seen = max_global_target_seen = global_target;
                    have_global_target = 1;
                } else {
                    if (global_target < min_global_target_seen) {
                        min_global_target_seen = global_target;
                    }
                    if (global_target > max_global_target_seen) {
                        max_global_target_seen = global_target;
                    }
                }

                if (!have_rel) {
                    min_rel_seen = max_rel_seen = ra;
                    have_rel = 1;
                }

                if (ra < min_rel_seen) min_rel_seen = ra;
                if (rb < min_rel_seen) min_rel_seen = rb;
                if (ra > max_rel_seen) max_rel_seen = ra;
                if (rb > max_rel_seen) max_rel_seen = rb;

                i64 sum = qa + qb + (i64)eps;

                if (qt != sum) {
                    arithmetic_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD arithmetic at line %lld: rid=%d qt=%lld qa=%lld qb=%lld eps=%d sum=%lld\n",
                               line_no, rid, qt, qa, qb, eps, sum);
                        bad_printed++;
                    }
                }

                if (ra >= 0 || rb >= 0) {
                    non_backward_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD backwardness at line %lld: rid=%d ra=%d rb=%d\n",
                               line_no, rid, ra, rb);
                        bad_printed++;
                    }
                }

                int expected = expected_parity(global_target);

                if (eps != expected || (eps != 1 && eps != -1)) {
                    parity_bad++;

                    if (bad_printed < MAX_BAD_PRINT) {
                        printf("BAD parity at line %lld: rid=%d L=%lld t=%d n=%lld eps=%d expected=%d\n",
                               line_no, rid, L, t, global_target, eps, expected);
                        bad_printed++;
                    }
                }

                (void)context_id;
                (void)center;
                (void)offa;
                (void)offb;
                (void)sigma_a;
                (void)sigma_b;
            }

            char *extra = strtok_r(NULL, " \t\r\n", &saveptr);
            if (extra != NULL) {
                malformed_bad++;

                if (bad_printed < MAX_BAD_PRINT) {
                    printf("BAD malformed at line %lld: extra token after %d records: %s\n",
                           line_no, records_seen_in_line, extra);
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

    printf("Arithmetic / backwardness / parity certificate:\n");
    printf("words_header=%lld\n", words_header);
    printf("words_checked=%lld\n", words_checked);
    printf("contexts_header=%lld\n", contexts_header);
    printf("contexts_checked=%lld\n", contexts_checked);
    printf("extensions_header=%lld\n", extensions_header);
    printf("extensions_checked=%lld\n", extensions_checked);
    printf("realizations_header=%lld\n", realizations_header);
    printf("realizations_checked=%lld\n", realizations_checked);
    printf("arithmetic_records_checked=%lld\n", arithmetic_records_checked);

    if (have_rel) {
        printf("min_rel_seen=%d\n", min_rel_seen);
        printf("max_rel_seen=%d\n", max_rel_seen);
    } else {
        printf("min_rel_seen=NA\n");
        printf("max_rel_seen=NA\n");
    }

    if (have_target) {
        printf("min_target_seen=%d\n", min_target_seen);
        printf("max_target_seen=%d\n", max_target_seen);
    } else {
        printf("min_target_seen=NA\n");
        printf("max_target_seen=NA\n");
    }

    if (have_global_target) {
        printf("min_global_target_seen=%lld\n", min_global_target_seen);
        printf("max_global_target_seen=%lld\n", max_global_target_seen);
    } else {
        printf("min_global_target_seen=NA\n");
        printf("max_global_target_seen=NA\n");
    }

    printf("arithmetic_bad=%lld\n", arithmetic_bad);
    printf("non_backward_bad=%lld\n", non_backward_bad);
    printf("parity_bad=%lld\n", parity_bad);
    printf("malformed_bad=%lld\n", malformed_bad);
    printf("header_bad=%lld\n", header_bad);

    if (arithmetic_bad == 0 &&
        non_backward_bad == 0 &&
        parity_bad == 0 &&
        malformed_bad == 0 &&
        header_bad == 0) {

        printf("\nRESULT: WELL-DEFINEDNESS INDUCTION CERTIFICATE VERIFIED\n");
        return 0;
    }

    printf("\nRESULT: WELL-DEFINEDNESS INDUCTION CERTIFICATE FAILED\n");
    return 1;
}
