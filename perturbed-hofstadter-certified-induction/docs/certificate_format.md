# Certificate Format

This document defines the expected format for checker certificate inputs stored in `certificates/`.

## File conventions

- Preferred extension: `.txt` or `.cert`
- Text encoding: UTF-8
- Line endings: `\n` (Unix) or `\r\n` (Windows)

## Generic structure

A certificate is parsed line-by-line by checker programs in `src/`.

Typical sections include:

1. **Header metadata** (optional): version or parameter identifiers.
2. **Finite-state objects**: states, transitions, labels.
3. **Induction witness blocks**: local obligations and closure conditions.
4. **Summary/trailer** (optional): counts or checksums.

Because checker implementations may differ, use the exact line grammar expected by the corresponding checker source file.

## Validation expectations

- The checker should reject malformed lines.
- The checker should reject certificates violating invariants.
- Successful verification should produce deterministic output suitable for `logs/`.

## Recommended practice

- Keep one canonical certificate file per theorem statement.
- Preserve historical certificates by versioning filenames.
- Store representative successful and failing run outputs in `logs/`.
