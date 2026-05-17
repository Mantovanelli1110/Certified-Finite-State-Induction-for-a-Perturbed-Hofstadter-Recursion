# Perturbed Hofstadter Certified Induction

This repository is organized for **reproducible computer-assisted verification** of a perturbed Hofstadter recursion proof.

## Repository layout

```
perturbed-hofstadter-certified-induction/
├── README.md
├── LICENSE
├── CITATION.cff
├── VERSION
├── paper/
├── src/
├── certificates/
├── logs/
├── scripts/
├── tests/
└── docs/
```

- `src/`: C source files for checkers and proof tooling.
- `certificates/`: certificate artifacts consumed by checkers.
- `logs/`: representative checker outputs and run logs.
- `scripts/`: build and verification orchestration scripts.
- `docs/`: format and process documentation.
- `paper/`: manuscript and supplementary material.
- `tests/`: regression and smoke checks for infrastructure.

## Reproducibility workflow

> The scripts below do **not** alter checker algorithms; they only build and orchestrate runs.

### 1) Build checker binaries

#### Linux/macOS

```bash
bash scripts/build_unix.sh
```

#### Windows (PowerShell)

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_windows.ps1
```

### 2) Run all checks

#### Linux/macOS

```bash
bash scripts/run_all_checks_unix.sh
```

#### Windows (PowerShell)

```powershell
powershell -ExecutionPolicy Bypass -File scripts/run_all_checks_windows.ps1
```

### 3) Inspect outputs

Generated run logs are placed in `logs/` with timestamps, together with any representative checker outputs committed to the repository.

## Notes

- If no C source files are present yet in `src/`, build scripts exit with a clear message.
- If no certificate files are present in `certificates/`, run scripts skip certificate-driven checks gracefully.

## Citation

See `CITATION.cff` for how to cite this artifact.
