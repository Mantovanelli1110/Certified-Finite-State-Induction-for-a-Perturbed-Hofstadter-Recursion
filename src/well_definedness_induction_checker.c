// well_definedness_induction_checker.c
//
// Prüft den Induktions-Kern:
// Jede exportierte Rekursionsgleichung benutzt nur frühere Stellen.
//
// Erwartetes A-Record-Format:
//
//   t q_t rel_a off_a sym_a q_a rel_b off_b sym_b q_b eps
//
// Geprüft wird:
//   1. q_t = q_a + q_b + eps
//   2. rel_a < 0
//   3. rel_b < 0
//   4. t + rel_a < t
//   5. t + rel_b < t
//
// Compile:
//   gcc -O3 -std=c11 well_definedness_induction_checker.c -o wdicheck
//
// Run:
//   ./wdicheck certificate.txt

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAX_WORDS 100000
#define MAX_CONTEXTS 1000000

typedef struct {
    int used;
    int id;
    int center;
} Context;

static int word_count = 0;
static int context_count = 0;
static int radius = -1;

static Context contexts[MAX_CONTEXTS];

static long long realizations_header = 0;
static long long realizations_checked = 0;
static long long records_checked = 0;

static long long arithmetic_bad = 0;
static long long non_backward_bad = 0;
static long long malformed_bad = 0;

static int min_rel_seen = INT_MAX;
static int max_rel_seen = INT_MIN;
static int max_t_seen = INT_MIN;

void die(const char *msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

void update_rel(int r) {
    if (r < min_rel_seen) min_rel_seen = r;
    if (r > max_rel_seen) max_rel_seen = r;
}

void skip_ints(FILE *f, int n) {
    int x;
    for (int i = 0; i < n; i++) {
        if (fscanf(f, "%d", &x) != 1) {
            die("unexpected EOF while skipping ints");
        }
    }
}

void skip_rest_of_line(FILE *f) {
    int c;
    while ((c = fgetc(f)) != '\n' && c != EOF) {
    }
}

void parse_word(FILE *f) {
    int id, len;

    if (fscanf(f, "%d %d", &id, &len) != 2)
        die("bad W line");

    if (id < 0 || id >= MAX_WORDS)
        die("bad W id");

    if (len <= 0)
        die("bad W length");

    skip_ints(f, len);
}

void parse_context(FILE *f) {
    int id;

    if (fscanf(f, "%d", &id) != 1)
        die("bad C id");

    if (id < 0 || id >= MAX_CONTEXTS)
        die("context id too large");

    int ctx_len = 2 * radius + 1;
    if (ctx_len <= 0 || ctx_len > 10000)
        die("bad radius / context length");

    int center = 0;

    for (int i = 0; i < ctx_len; i++) {
        int x;
        if (fscanf(f, "%d", &x) != 1)
            die("bad C context symbol");

        if (i == radius)
            center = x;
    }

    char tag[64];
    int cval, count;

    if (fscanf(f, "%63s", tag) != 1 || strcmp(tag, "CENTER") != 0)
        die("expected CENTER");

    if (fscanf(f, "%d", &cval) != 1)
        die("bad CENTER value");

    if (fscanf(f, "%63s", tag) != 1 || strcmp(tag, "COUNT") != 0)
        die("expected COUNT");

    if (fscanf(f, "%d", &count) != 1)
        die("bad COUNT value");

    contexts[id].used = 1;
    contexts[id].id = id;
    contexts[id].center = center;

    if (id + 1 > context_count)
        context_count = id + 1;
}

void parse_A_block(FILE *f) {
    int rid, cid, len;

    if (fscanf(f, "%d %d %d", &rid, &cid, &len) != 3) {
        die("bad A header");
    }

    if (cid < 0 || cid >= MAX_CONTEXTS || !contexts[cid].used) {
        die("bad or unknown context id in A");
    }

    if (len < 0) {
        die("bad A length");
    }

    realizations_checked++;

    for (int i = 0; i < len; i++) {
        int t;
        int q_t;
        int rel_a;
        int off_a;
        int sym_a;
        int q_a;
        int rel_b;
        int off_b;
        int sym_b;
        int q_b;
        int eps;

        int got = fscanf(f, "%d %d %d %d %d %d %d %d %d %d %d",
                         &t,
                         &q_t,
                         &rel_a,
                         &off_a,
                         &sym_a,
                         &q_a,
                         &rel_b,
                         &off_b,
                         &sym_b,
                         &q_b,
                         &eps);

        if (got != 11) {
            malformed_bad++;
            die("bad A arithmetic record");
        }

        records_checked++;

        if (t > max_t_seen)
            max_t_seen = t;

        update_rel(rel_a);
        update_rel(rel_b);

        if (q_t != q_a + q_b + eps) {
            arithmetic_bad++;

            if (arithmetic_bad <= 10) {
                printf("\nARITHMETIC FAILURE\n");
                printf("rid=%d cid=%d record=%d\n", rid, cid, i);
                printf("t=%d q_t=%d q_a=%d q_b=%d eps=%d\n",
                       t, q_t, q_a, q_b, eps);
            }
        }

        if (rel_a >= 0 || rel_b >= 0) {
            non_backward_bad++;

            if (non_backward_bad <= 20) {
                printf("\nNON-BACKWARD RECURSION FAILURE\n");
                printf("rid=%d cid=%d record=%d\n", rid, cid, i);
                printf("t=%d rel_a=%d rel_b=%d\n", t, rel_a, rel_b);
                printf("target_a=%d target_b=%d\n", t + rel_a, t + rel_b);
            }
        }

        if (t + rel_a >= t || t + rel_b >= t) {
            non_backward_bad++;

            if (non_backward_bad <= 20) {
                printf("\nNON-DECREASING TARGET FAILURE\n");
                printf("rid=%d cid=%d record=%d\n", rid, cid, i);
                printf("t=%d target_a=%d target_b=%d\n",
                       t, t + rel_a, t + rel_b);
            }
        }
    }
}

void parse_certificate(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) die("cannot open certificate");

    char tag[128];

    while (fscanf(f, "%127s", tag) == 1) {

        if (strcmp(tag, "RADIUS") == 0 || strcmp(tag, "radius") == 0) {
            if (fscanf(f, "%d", &radius) != 1)
                die("bad radius");
        }

        else if (strcmp(tag, "WORDS") == 0) {
            if (fscanf(f, "%d", &word_count) != 1)
                die("bad WORDS header");

            if (word_count <= 0 || word_count > MAX_WORDS)
                die("bad word count");
        }

        else if (strcmp(tag, "W") == 0) {
            parse_word(f);
        }

        else if (strcmp(tag, "CONTEXTS") == 0) {
            int declared;

            if (fscanf(f, "%d", &declared) != 1)
                die("bad CONTEXTS header");

            if (declared <= 0 || declared > MAX_CONTEXTS)
                die("bad context count");

            if (radius < 0) {
                // Dein Exporter benutzt bisher implizit R=20.
                radius = 20;
            }
        }

        else if (strcmp(tag, "C") == 0) {
            parse_context(f);
        }

        else if (strcmp(tag, "EXTENSIONS") == 0) {
            int n;
            if (fscanf(f, "%d", &n) != 1)
                die("bad EXTENSIONS header");
        }

        else if (strcmp(tag, "E") == 0) {
            skip_rest_of_line(f);
        }

        else if (strcmp(tag, "REALIZATIONS") == 0) {
            if (fscanf(f, "%lld", &realizations_header) != 1)
                die("bad REALIZATIONS header");
        }

        else if (strcmp(tag, "A") == 0) {
            parse_A_block(f);

            if ((realizations_checked % 500000) == 0) {
                printf("checked realizations: %lld\n", realizations_checked);
                fflush(stdout);
            }
        }

        else {
            fprintf(stderr, "Unknown tag: %s\n", tag);
            die("unknown tag");
        }
    }

    fclose(f);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s certificate.txt\n", argv[0]);
        return 1;
    }

    parse_certificate(argv[1]);

    printf("\nParsed certificate:\n");
    printf("radius=%d\n", radius);
    printf("words=%d\n", word_count);
    printf("contexts_seen=%d\n", context_count);
    printf("realizations_header=%lld\n", realizations_header);
    printf("realizations_checked=%lld\n", realizations_checked);
    printf("arithmetic_records_checked=%lld\n", records_checked);

    printf("\nInduction statistics:\n");
    printf("min_rel_seen=%d\n", min_rel_seen);
    printf("max_rel_seen=%d\n", max_rel_seen);
    printf("max_t_seen=%d\n", max_t_seen);
    printf("arithmetic_bad=%lld\n", arithmetic_bad);
    printf("non_backward_bad=%lld\n", non_backward_bad);
    printf("malformed_bad=%lld\n", malformed_bad);

    if (records_checked == 0) {
        die("no arithmetic records checked");
    }

    if (realizations_header != 0 &&
        realizations_header != realizations_checked) {
        printf("\nERROR: realization count mismatch\n");
        return 1;
    }

    if (arithmetic_bad == 0 &&
        non_backward_bad == 0 &&
        malformed_bad == 0) {
        printf("\nRESULT: WELL-DEFINEDNESS INDUCTION CHECK VERIFIED\n");
        printf("Every certified recursive call is strictly backward, and every arithmetic equation is valid.\n");
        return 0;
    }

    printf("\nRESULT: WELL-DEFINEDNESS INDUCTION CHECK FAILED\n");
    return 1;
}