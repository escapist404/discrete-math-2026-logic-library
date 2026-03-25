# Logic Benchmark (C++/CMake)

This benchmark suite is C++-based.

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

Optional CMake flags:

- `-DBENCHMARK_DEBUG=ON|OFF`
- `-DBENCHMARK_FORCE_ISA=ON|OFF`

## Run Benchmarks

### Direct runner (recommended)

```bash
./build/benchmark_runner --scenario-set quick --cpp recursion
```

Common options:

- `--scenario-set quick|full`
- `--cpp legacy,recursion,...`
- `--check-correctness`
- `--build-dir <path>`
- `--source-dir <path>`
- `--debug-build on|off`
- `--no-build`

### Interactive shell script

```bash
./run_benchmark.sh
```

### Non-interactive shell script

```bash
./run_benchmark.sh --non-interactive --build yes --scenario-set quick --cpp simd_avx2_omp_parallel
```

## Outputs

Benchmark outputs are written under:

- `test_results/run_<timestamp>/`

Each run directory includes:

- `input.txt` per case
- `<impl>_output.txt` per implementation
- `benchmark_results.csv`

## License

This repository currently uses file-level license headers.

- `utility/generator.cc`, `utility/generator.h`, `benchmark_runner.cc` and `run_benchmark.sh`  are marked `GPL-3.0-only`.
- Other source files keep their own existing license status unless explicitly stated in-file.

If you distribute binaries or source that include GPL-covered files, ensure compliance with GPLv3 obligations (for example: preserving notices and providing corresponding source as required).
