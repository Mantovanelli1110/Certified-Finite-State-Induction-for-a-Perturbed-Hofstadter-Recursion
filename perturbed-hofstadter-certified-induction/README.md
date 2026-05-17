# Perturbed Hofstadter Certified Induction

This repository contains the code, finite certificates, verification logs, and
documentation for the computer-assisted proof in the paper

**Certified Finite-State Induction for a Perturbed Hofstadter Recursion**

by Marco Mantovanelli.

The recursion studied is

```text
Q(1) = 1,
Q(2) = 1,
Q(n) = Q(n - Q(n-1)) + Q(n - Q(n-2)) + (-1)^n.
```

The purpose of this repository is to make the finite verification component of
the proof reproducible.

The main theorem proved in the accompanying paper is that this recursion is
well-defined for all `n >= 1`.

---

## Repository layout

```text
perturbed-hofstadter-certified-induction/
├── README.md
├── LICENSE
├── CITATION.cff
├── VERSION
├── HASHES.txt
├── paper/
├── src/
├── certificates/
├── logs/
├── scripts/
├── tests/
└── docs/
```

- `src/`: C source files for trace generation, certificate export, and verification checkers.
- `certificates/`: machine-readable certificate artifacts consumed by the checkers.
- `logs/`: representative checker outputs and run logs.
- `scripts/`: build and verification orchestration scripts.
- `docs/`: certificate-format and reproducibility documentation.
- `paper/`: manuscript, bibliography, and figures.
- `tests/`: regression and smoke checks for infrastructure.
- `HASHES.txt`: SHA256 hashes of the certificate files and representative logs used for the paper.

---

## Proof architecture

The verification is divided into two roles:

1. **Generation tools** produce or reconstruct finite data from the recurrence.
2. **Verification tools** check finite certificates independently.

The trace is not used as numerical evidence for the theorem. It is used to
propose and anchor a finite symbolic state space. The proof then uses finite
certificate checks for symbolic closure, factorization, faithfulness,
arithmetic correctness, parity consistency, and strict backwardness.

The verification pipeline is:

```text
verified trace
   ↓
symbolic extraction and certificate export
   ↓
finite word systems and context transition relation
   ↓
composition / factorization checks
   ↓
faithfulness and induction checks
   ↓
finite-state induction proof of well-definedness
```

---

## Main source files

The main C programs are located in `src/`.

### Generation and export

- `trace_generator.c`

  Directly computes the recurrence up to a prescribed bound, by default

  ```text
  N = 50,000,000
  ```

  and verifies at every step that both recursive arguments are positive and
  strictly backward.

  Expected successful result:

  ```text
  RESULT: VERIFIED BASE TRACE
  undefined_bad=0
  non_backward_bad=0
  ```

- `certificate_exporter.c`

  Exports the finite symbolic and arithmetic certificate used by the independent
  verification programs.

### Symbolic verification

- `cycle_composition_checker.c`

  Checks that the symbolic cycle does not leave the declared symbolic alphabet.

- `cycle_composition_pattern_checker.c`

  Diagnostic checker for direct pattern realization. This checker is not the
  main closure certificate; it helps distinguish direct equality from
  factorization closure.

- `cycle_composition_factor_checker.c`

  Verifies that every certified stable-core word expands through the cycle

  ```text
  V -> U -> T -> S
  ```

  and factors into certified `S`-words.

- `coverage_anchor_checker.c`

  Verifies stable-prefix anchoring and anchored cycle factorization.

### Arithmetic, parity, and faithfulness verification

- `well_definedness_induction_checker.c`

  Verifies the arithmetic induction certificate. It checks:

  ```text
  q_t = q_a + q_b + epsilon
  epsilon = (-1)^(L+t)
  r_a < 0
  r_b < 0
  ```

  where `L` is the global block anchor and `t` is the local target offset of the
  arithmetic record.

- `faithfulness_checker.c`

  Verifies that the exported word, context, extension, realization, and
  arithmetic-record data agree with an independent reconstruction of the
  certified symbolic extraction data.

---

## Certificate format

The main certificate is stored in

```text
certificates/certificate.txt
```

The certificate contains finite data used by the proof checkers. The main
sections are:

```text
WORDS
CONTEXTS
EXTENSIONS
REALIZATIONS
```

A short description follows. See `docs/certificate_format.md` for the full
technical specification.

### Word systems

The certificate contains the finite symbolic word systems

```text
S, T, U, V
```

used in the renormalization cycle

```text
S -> T -> U -> V -> S.
```

The relevant certified sizes are:

```text
|S| = 53
|T| = 46
|U| = 38
|V| = 28
```

### Context records

A context is a radius-`R` symbolic window. In the certificate used in the paper,

```text
R = 20
```

so each context has length `2R+1 = 41`.

### Extension records

An extension record has the form

```text
E a b c
```

where `a`, `b`, and `c` are context identifiers.

It represents a certified radius-`R` shift transition. If

```text
Gamma_a = (x_0, ..., x_{2R})
Gamma_b = (y_0, ..., y_{2R})
```

then the extension is admissible only if

```text
x_i = y_{i-1}   for 1 <= i <= 2R.
```

The successor context is

```text
Gamma_c = (x_1, ..., x_{2R}, y_{2R}).
```

### Realization records

A realization record stores a global block anchor `L` and a list of arithmetic
records belonging to that realization.

### Arithmetic records

Each arithmetic record has the form

```text
t qt ra oa sigma_a qa rb ob sigma_b qb eps
```

where:

- `t` is the local target offset relative to the block anchor `L`;
- `qt` is the certified target value;
- `ra`, `rb` are the relative offsets of the two recursive calls;
- `oa`, `ob` are auxiliary offsets recorded by the exporter;
- `sigma_a`, `sigma_b` are symbolic labels of the argument positions;
- `qa`, `qb` are the certified argument values;
- `eps` is the perturbation term.

The corresponding global target index is

```text
n = L + t.
```

The induction checker verifies:

```text
qt = qa + qb + eps
eps = (-1)^(L+t)
ra < 0
rb < 0
```

---

## Reproducibility workflow

The scripts below do not alter the checker algorithms. They only build and
orchestrate runs.

### 1. Build checker binaries

#### Linux/macOS

```bash
bash scripts/build_unix.sh
```

#### Windows PowerShell

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_windows.ps1
```

The resulting binaries are placed in `bin/`.

---

### 2. Run all verification checks

#### Linux/macOS

```bash
bash scripts/run_all_checks_unix.sh
```

#### Windows PowerShell

```powershell
powershell -ExecutionPolicy Bypass -File scripts/run_all_checks_windows.ps1
```

Generated run logs are placed in `logs/`.

---

## Expected verification outputs

A successful run should report zero failures in all core checkers.

### Coverage and anchoring checker

Expected key output:

```text
symbol_bounds=OK
stable_prefix=OK
composition_factorization=OK
symbol_bad=0
factor_bad=0
factor_count_mismatch=0
```

### Cycle composition pattern checker

Expected diagnostic output:

```text
matched_S_words=1
unmatched=27
symbol_bad_total=0

RESULT: 4-STEP CYCLE COMPOSITION PATTERN FAILED
The expansion lands inside the S alphabet, but not always on an existing S-word.
```

### Cycle composition factor checker

Expected key output:

```text
checked_words=28
factored=28
failed=0
symbol_bad_total=0
```

### Well-definedness induction checker

Expected key output:

```text
realizations_checked=4735941
arithmetic_records_checked=49999571
min_rel_seen=-2441147
max_rel_seen=-10
arithmetic_bad=0
non_backward_bad=0
parity_bad=0
malformed_bad=0
header_bad=0

RESULT: WELL-DEFINEDNESS INDUCTION CERTIFICATE VERIFIED
```

### Faithfulness checker

Expected key output:

```text
words_checked=13
contexts_checked=2923
extensions_checked=3026
realizations_checked=4735941
arithmetic_records_checked=49999571
malformed_bad=0
word_bad=0
context_bad=0
extension_bad=0
extension_missing_bad=0
realization_bad=0
record_bad=0
header_bad=0

RESULT: FAITHFULNESS CERTIFICATE VERIFIED
```

---

## Base trace verification

The program

```text
src/trace_generator.c
```

directly recomputes the recurrence up to `N = 5 * 10^7`, unless a different
bound is provided.

Example:

```bash
./bin/trace_generator 50000000
```

Optionally, it can write a binary trace file:

```bash
./bin/trace_generator 50000000 certificates/trace.bin
```

A successful run verifies that all recursive arguments in the base range are
positive and strictly backward.

Expected key output:

```text
undefined_bad=0
non_backward_bad=0
RESULT: VERIFIED BASE TRACE
```

---

## Hash verification

The file

```text
HASHES.txt
```

contains SHA256 hashes of the certificate files and representative output logs
used for the paper.

On Linux/macOS:

```bash
sha256sum -c HASHES.txt
```

On Windows PowerShell, hashes can be checked manually using:

```powershell
Get-FileHash certificates\certificate.txt -Algorithm SHA256
```

The hashes in `HASHES.txt` should correspond to the exact version of the
certificate and logs cited in the paper.

---

## Notes on trusted code

The proof is computer-assisted. The verification programs act as finite proof
checkers for explicit machine-readable certificates.

The trusted components are:

1. the mathematical proof in the paper;
2. the C verification programs;
3. the certificate files;
4. the reproducibility of the recorded outputs.

The generation pipeline and the verification pipeline are intentionally
separated. Once the certificate is exported, the checkers operate on the finite
certificate data.

A future formalization in a proof assistant such as Lean or Coq could further
reduce the trusted code base.

---

## Development notes

- The checker programs should be compiled with a C11-compatible compiler.
- Recommended compiler flags are:

  ```bash
  -O2 -std=c11 -Wall -Wextra
  ```

- The repository version used in the paper should be tagged, for example:

  ```bash
  git tag v1.0.0
  ```

- For a citable archival version, create a release and archive it with Zenodo
  or another DOI-issuing repository.

---

## Citation

See `CITATION.cff` for how to cite this artifact.

If you use this repository, please cite both the paper and the archived
repository release.

