# Certificate Format

This document describes the machine-readable certificate files used by
the verification programs.

The large main certificate is not stored directly in Git. It is archived
on Zenodo as:

https://doi.org/10.5281/zenodo.20258943

After downloading and decompressing, the main certificate is expected at:

```text
certificates/certificate.txt
```

The compressed artifact is:

```text
certificates/certificate.txt.zst
```

Expected hashes are recorded in:

```text
HASHES.txt
certificates/certificate_manifest.txt
```

---

## File conventions

- Format: plain text
- Encoding: UTF-8
- Preferred line endings: `\n`; Windows `\r\n` is also accepted
- Integers are written in decimal notation
- Fields are separated by ASCII whitespace
- Lines are parsed by the C verification programs in `src/`

---

## Main certificate sections

The main certificate contains the following sections:

```text
WORDS
CONTEXTS
EXTENSIONS
REALIZATIONS
```

The checkers reject malformed records and report nonzero failure counters
for malformed input, header mismatches, out-of-range symbols, invalid
extensions, arithmetic mismatches, parity mismatches, and non-backward
dependencies.

---

## Word systems

The certificate uses four finite symbolic word systems:

```text
S, T, U, V
```

with certified sizes:

```text
|S| = 53
|T| = 46
|U| = 38
|V| = 28
```

The symbolic renormalization cycle is:

```text
S -> T -> U -> V -> S
```

A successful factorization check verifies that the stable `V`-core,
consisting of `V_0, ..., V_27`, expands through the cycle and factors
into certified `S`-words.

---

## Context records

The certificate contains radius-`R` symbolic contexts with

```text
R = 20
```

Thus each context has length:

```text
2R + 1 = 41
```

A context represents a symmetric symbolic window:

```text
Gamma_R(c) = (sigma_{c-R}, ..., sigma_c, ..., sigma_{c+R})
```

The faithfulness checker verifies that exported contexts agree with an
independent reconstruction from the recomputed trace.

---

## Extension records

An extension record has the form:

```text
E a b c
```

where `a`, `b`, and `c` are context identifiers.

If

```text
Gamma_a = (x_0, ..., x_{2R})
Gamma_b = (y_0, ..., y_{2R})
```

then the extension is admissible only if:

```text
x_i = y_{i-1}   for 1 <= i <= 2R
```

The successor context is:

```text
Gamma_c = (x_1, ..., x_{2R}, y_{2R})
```

The faithfulness checker verifies:

```text
extension_bad = 0
extension_missing_bad = 0
```

Here `extension_bad = 0` means that all declared extension records satisfy
the required suffix-prefix overlap and shifted-context identity.

The missing-extension check computes the required successor context
independently of the exported edge list for each reachable declared
admissible context and verifies that the corresponding extension record is
present.

---

## Realization records

A realization record stores a global block anchor:

```text
L
```

and a list of arithmetic records belonging to that realization.

The global target index of an arithmetic record is recovered as:

```text
n = L + t
```

where `t` is the local target offset.

---

## Arithmetic records

Each arithmetic record has the form:

```text
t qt ra oa sigma_a qa rb ob sigma_b qb eps
```

with fields:

- `t`: local target offset relative to the block anchor `L`
- `qt`: certified target value
- `ra`, `rb`: relative offsets of the two recursive calls
- `oa`, `ob`: auxiliary offsets recorded by the exporter
- `sigma_a`, `sigma_b`: symbolic labels of the recursive argument positions
- `qa`, `qb`: certified values at the recursive arguments
- `eps`: certified perturbation term

The induction checker verifies:

```text
qt = qa + qb + eps
eps = (-1)^(L+t)
ra < 0
rb < 0
```

A successful run reports:

```text
arithmetic_bad = 0
non_backward_bad = 0
parity_bad = 0
malformed_bad = 0
header_bad = 0
```

---

## Checker responsibilities

### `well_definedness_induction_checker_v2.c`

Checks arithmetic correctness, parity consistency, strict backwardness,
malformed records, and header consistency.

### `faithfulness_checker.c`

Recomputes the verified trace from the original recurrence, reconstructs
the symbolic extraction data, and checks that exported words, contexts,
extensions, realizations, dependency offsets, recurrence values, and
parity terms agree with the recomputed data.

### `coverage_anchor_checker.c`

Checks stable-prefix anchoring, symbol bounds, and anchored cycle
factorization.

### `cycle_composition_factor_checker.c`

Checks that all stable `V`-core words expand through the cycle and factor
into certified `S`-words.

### `cycle_composition_pattern_checker.c`

Diagnostic only. It checks direct matching to single `S`-words. This is
not the closure condition used in the proof.

---

## Expected successful main results

The core proof logs should contain:

```text
RESULT: VERIFIED BASE TRACE
RESULT: 4-STEP CYCLE COMPOSITION SYMBOL-CLOSED
RESULT: 4-STEP CYCLE COMPOSITION FACTORIZATION VERIFIED
RESULT: SYMBOLIC COVERAGE AND ANCHORING VERIFIED
RESULT: WELL-DEFINEDNESS INDUCTION CERTIFICATE VERIFIED
RESULT: FAITHFULNESS CERTIFICATE VERIFIED
```

The diagnostic pattern checker may report:

```text
RESULT: 4-STEP CYCLE COMPOSITION PATTERN FAILED
```

This is expected and is not a proof failure. It shows that direct equality
with a single `S`-word is too strong; the proof uses factorization into
certified `S`-words instead.
