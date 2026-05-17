/*
  trace_generator.c

  Directly computes the perturbed Hofstadter recursion

      Q(1) = Q(2) = 1,
      Q(n) = Q(n - Q(n-1)) + Q(n - Q(n-2)) + (-1)^n

  up to a prescribed bound N.

  It verifies at every step that both recursive arguments are positive
  and strictly smaller than n.

  Usage:
      ./trace_generator
      ./trace_generator 50000000
      ./trace_generator 50000000 trace.bin

  If a third argument is given, the program writes a binary trace file:
      uint64_t N
      int32_t  Q[1], Q[2], ..., Q[N]

  This file is intended as a reproducible base trace for the certificate
  exporter.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <time.h>

#define DEFAULT_N 50000000ULL

static void die(const char *msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(EXIT_FAILURE);
}

static uint64_t parse_u64(const char *s) {
    char *end = NULL;
    unsigned long long v = strtoull(s, &end, 10);
    if (end == s || *end != '\0') {
        die("invalid integer argument");
    }
    return (uint64_t)v;
}

static int parity_term(uint64_t n) {
    return (n % 2 == 0) ? 1 : -1;
}

static void write_binary_trace(const char *path, const int32_t *Q, uint64_t N) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        die("could not open output trace file");
    }

    if (fwrite(&N, sizeof(uint64_t), 1, f) != 1) {
        die("failed to write trace header");
    }

    /*
      Q is stored 1-based.  We write Q[1] through Q[N].
    */
    if (fwrite(Q + 1, sizeof(int32_t), (size_t)N, f) != (size_t)N) {
        die("failed to write trace values");
    }

    if (fclose(f) != 0) {
        die("failed to close output trace file");
    }
}

int main(int argc, char **argv) {
    uint64_t N = DEFAULT_N;
    const char *out_path = NULL;

    if (argc >= 2) {
        N = parse_u64(argv[1]);
    }
    if (argc >= 3) {
        out_path = argv[2];
    }
    if (argc > 3) {
        fprintf(stderr, "Usage: %s [N] [trace.bin]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (N < 2) {
        die("N must be at least 2");
    }

    printf("Trace generator\n");
    printf("N=%" PRIu64 "\n", N);

    size_t count = (size_t)(N + 3);
    int32_t *Q = (int32_t *)calloc(count, sizeof(int32_t));
    if (!Q) {
        die("memory allocation failed");
    }

    clock_t start = clock();

    Q[1] = 1;
    Q[2] = 1;

    uint64_t min_arg_seen = UINT64_MAX;
    uint64_t max_arg_seen = 0;
    uint64_t min_gap_seen = UINT64_MAX;
    uint64_t max_q_seen = 1;

    for (uint64_t n = 3; n <= N; n++) {
        int64_t a = (int64_t)n - (int64_t)Q[n - 1];
        int64_t b = (int64_t)n - (int64_t)Q[n - 2];

        if (a <= 0 || b <= 0) {
            fprintf(stderr,
                    "UNDEFINED at n=%" PRIu64 ": a=%" PRId64 ", b=%" PRId64 "\n",
                    n, a, b);
            free(Q);
            return EXIT_FAILURE;
        }

        if (a >= (int64_t)n || b >= (int64_t)n) {
            fprintf(stderr,
                    "NON-BACKWARD at n=%" PRIu64 ": a=%" PRId64 ", b=%" PRId64 "\n",
                    n, a, b);
            free(Q);
            return EXIT_FAILURE;
        }

        int eps = parity_term(n);

        int64_t value =
            (int64_t)Q[a] +
            (int64_t)Q[b] +
            (int64_t)eps;

        if (value < INT32_MIN || value > INT32_MAX) {
            fprintf(stderr,
                    "VALUE OUT OF int32 RANGE at n=%" PRIu64 ": value=%" PRId64 "\n",
                    n, value);
            free(Q);
            return EXIT_FAILURE;
        }

        Q[n] = (int32_t)value;

        if ((uint64_t)a < min_arg_seen) min_arg_seen = (uint64_t)a;
        if ((uint64_t)b < min_arg_seen) min_arg_seen = (uint64_t)b;
        if ((uint64_t)a > max_arg_seen) max_arg_seen = (uint64_t)a;
        if ((uint64_t)b > max_arg_seen) max_arg_seen = (uint64_t)b;

        uint64_t gap_a = n - (uint64_t)a;
        uint64_t gap_b = n - (uint64_t)b;
        if (gap_a < min_gap_seen) min_gap_seen = gap_a;
        if (gap_b < min_gap_seen) min_gap_seen = gap_b;

        if ((uint64_t)Q[n] > max_q_seen) {
            max_q_seen = (uint64_t)Q[n];
        }

        if (n % 10000000ULL == 0) {
            printf("computed n=%" PRIu64 "\n", n);
            fflush(stdout);
        }
    }

    clock_t end = clock();
    double seconds = (double)(end - start) / (double)CLOCKS_PER_SEC;

    printf("\nDirect trace verification:\n");
    printf("N=%" PRIu64 "\n", N);
    printf("Q(1)=%" PRId32 "\n", Q[1]);
    printf("Q(2)=%" PRId32 "\n", Q[2]);
    printf("Q(N)=%" PRId32 "\n", Q[N]);
    printf("min_arg_seen=%" PRIu64 "\n", min_arg_seen);
    printf("max_arg_seen=%" PRIu64 "\n", max_arg_seen);
    printf("min_backward_gap_seen=%" PRIu64 "\n", min_gap_seen);
    printf("max_q_seen=%" PRIu64 "\n", max_q_seen);
    printf("undefined_bad=0\n");
    printf("non_backward_bad=0\n");
    printf("elapsed_seconds=%.2f\n", seconds);

    if (out_path) {
        write_binary_trace(out_path, Q, N);
        printf("binary_trace_written=%s\n", out_path);
    }

    printf("\nRESULT: VERIFIED BASE TRACE\n");

    free(Q);
    return EXIT_SUCCESS;
}