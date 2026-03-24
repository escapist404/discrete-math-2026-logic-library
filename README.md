# Logic Formula Toolkit and Benchmark Suite

This repository contains two related parts:

1. A Python logic toolkit for propositional formulas:
	 - truth table generation
	 - suffix (postfix) conversion and evaluation
	 - PDNF and PCNF helpers
2. A C++ benchmark suite with multiple implementations of formula evaluation, including SIMD variants.

## Features

- Supports unary and binary logical operators.
- Extracts propositional variables from a formula.
- Generates full truth tables for a formula.
- Converts formulas to postfix notation.
- Generates perfect normal-form terms (PDNF and PCNF helpers).
- Benchmarks multiple C++ engines and exports CSV results.

## Repository Layout

- logic_lib/
	- constants.py: operator and parenthesis symbols
	- truth_table.py: parsing, postfix evaluation, truth-table generation
	- primary_normal_form.py: PDNF/PCNF-related utilities
	- exception.py: custom exception type
- test.py: simple command-line truth-table runner
- benchmark/
	- run_test.py: compile + run benchmark scenarios
	- source/: C++ implementations (legacy, recursion, SIMD AVX2/AVX512, OMP)
	- utility/generator.py: random well-formed formula generator
	- utility/checker.cpp: formula legality checker
	- test_results/: benchmark output directories and CSV files

## Formula Syntax

### Operators

- ! : negation
- & : conjunction
- | : disjunction
- > : implication
- - : biconditional

### Parentheses

- ( and )

### Variable names

- The C++ benchmark path assumes lowercase letters a-z.
- Python variable extraction treats non-operator symbols as variables.

### Precedence (highest to lowest)

1. !
2. &
3. |
4. >
5. -

## Python Usage

### Minimal interactive runner

Run:

```bash
python3 test.py
```

Then provide a formula on stdin, for example:

```text
(a>b)
```

Output format:

- First line: variable names followed by the original formula
- Remaining lines: truth-table rows in 0/1 format

### Using library functions directly

Main APIs in logic_lib:

- get_prop_var(wff)
- get_truth_table(wff)
- convert_wff_to_suffix_expr(wff)
- calc_suffix_expr_value(suffix_expr)
- convert_wff_to_pdnf(wff)
- convert_wff_to_pcnf(wff)
- test_wff_is_always_true(wff)

Note:
Current module imports in logic_lib reference a package name "logic". If you run this repository directly as-is, you may need to align package naming (for example, rename the package directory or adjust imports) so imports resolve consistently.

## Benchmark Suite

The benchmark runner compiles selected C++ implementations and executes scenario sets, generating:

- per-case output files
- an aggregated benchmark_results.csv

### Prerequisites

- Python 3
- g++ with C++17 support
- OpenMP support
- CPU/compiler support for AVX2 and AVX512 flags used by the runner

Compile flags used by benchmark/run_test.py include:

- -O3
- -std=c++17
- -fopenmp
- -mavx2
- -mavx512f
- -mavx512bw

If your machine does not support AVX512, remove or conditionally disable those implementations/flags before running.

### Run benchmarks

From benchmark/:

```bash
python3 run_test.py
```

Options:

- --scenario-set quick|full
- --cpp name1,name2,...

Examples:

```bash
# quick default set
python3 run_test.py

# full scenario set
python3 run_test.py --scenario-set full

# run only specific implementations
python3 run_test.py --cpp legacy,simd_avx2_precalc
```

### Output location

Each run writes to:

benchmark/test_results/run_YYYYMMDD_HHMMSS/

Including:

- benchmark_results.csv
- per-scenario folders like n21_l5000/ with captured outputs

## Implementation Keys

The benchmark runner recognizes these keys:

- checker
- legacy
- recursion
- simd_ord_fill
- simd_ord_precalc
- simd_avx2_fill
- simd_avx2_precalc
- simd_avx512_precalc
- simd_avx512_omp_parallel

## Notes

- The benchmark timeout per run case is 60 seconds (TIMEOUT_LIMIT in benchmark/run_test.py).
- The formula generator currently uses lowercase variables and can generate deep/large expressions.
- Existing benchmark artifacts in test_results can be large and are historical outputs.
