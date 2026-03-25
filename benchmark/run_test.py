#!/usr/bin/env python3

# self custom test runner for benchmarking the various implementations
# implemented by Gemini
# modified by Richard Qin
# do not trust ai, they may cheat you, always verify the results

import argparse
import csv
import subprocess
import time
import os
import sys
import re

SOURCE_DIR = "source"
UTILITY_DIR = "utility"

CPP_FILES = {
    "checker": os.path.join(UTILITY_DIR, "checker.cpp"),
    "legacy": os.path.join(SOURCE_DIR, "legacy.cpp"),
    "recursion": os.path.join(SOURCE_DIR, "recursion.cpp"),
    "simd_ord_fill": os.path.join(SOURCE_DIR, "simd_ord_fill.cpp"),
    "simd_ord_precalc": os.path.join(SOURCE_DIR, "simd_ord_precalc.cpp"),
    "simd_avx2_fill": os.path.join(SOURCE_DIR, "simd_avx2_fill.cpp"),
    "simd_avx2_precalc": os.path.join(SOURCE_DIR, "simd_avx2_precalc.cpp"),
    "simd_avx512_precalc": os.path.join(SOURCE_DIR, "simd_avx512_precalc.cpp"),
    "simd_avx512_omp_parallel": os.path.join(SOURCE_DIR, "simd_avx512_omp_parallel.cpp"),
    "simd_avx2_omp_parallel": os.path.join(SOURCE_DIR, "simd_avx2_omp_parallel.cpp")
}
GENERATOR_SCRIPT = os.path.join(UTILITY_DIR, "generator.py")
REUSE_RATE = 0

TEST_CASES_FULL = [
    (2, 20), (3, 20), (4, 20), (5, 20), (6, 20), (7, 20), (8, 20), (9, 20), (10, 20),
    (2, 200), (3, 200), (4, 200), (5, 200), (6, 200), (7, 200), (8, 200), (9, 200), (10, 200),
    (11, 200), (12, 200), (13, 200), (14, 200), (15, 200), (16, 200), (17, 200), (18, 200), (19, 200),
    (20, 200), (20, 400), (20, 600), (20, 800), (20, 1000), (20, 1200), (20, 1400), (20, 1600), (20, 1800), (20, 2000), (20, 2500), (20, 3000), (20, 3500), (20, 4000), (20, 4500), (20, 5000), (20, 5500), (20, 6000), (20, 6500), (20, 7000), (20, 7500), (20, 8000), (20, 8500), (20, 9000), (20, 9500), (20, 10000),
    (21, 200), (21, 400), (21, 600), (21, 800), (21, 1000), (21, 1200), (21, 1400), (21, 1600), (21, 1800), (21, 2000), (21, 2500), (21, 3000), (21, 3500), (21, 4000), (21, 4500), (21, 5000), (21, 5500), (21, 6000), (21, 6500), (21, 7000), (21, 7500), (21, 8000), (21, 8500), (21, 9000), (21, 9500), (21, 10000),
    (21, 12000), (21, 14000), (21, 16000), (21, 18000), (21, 20000), (21, 24000), (21, 28000), (21, 32000), (21, 36000), (21, 40000), (21, 44000), (21, 48000), (21, 52000), (21, 56000), (21, 60000), (21, 64000), (21, 70000), (21, 75000), (21, 80000), (21, 90000), (21, 100000), (21, 110000), (21, 120000), (21, 130000), (21, 140000), (21, 150000), (21, 160000), (21, 170000), (21, 180000), (21, 190000), (21, 200000),
    (22, 1000), (22, 4000), (22, 6000), (22, 8000), (22, 10000), (22, 12000), (22, 15000), (22, 18000), (22, 20000), (22, 24000), (22, 28000), (22, 32000), (22, 36000), (22, 40000), (22, 44000), (22, 48000), (22, 52000), (22, 56000), (22, 60000), (22, 64000), (22, 70000), (22, 75000), (22, 80000), (22, 90000), (22, 100000), (22, 110000), (22, 120000), (22, 130000), (22, 140000), (22, 150000), (22, 160000), (22, 170000), (22, 180000), (22, 190000), (22, 200000), (22, 300000), (22, 400000), (22, 500000), (22, 600000), (22, 700000), (22, 800000), (22, 900000), (22, 1000000)
]

TEST_CASES_QUICK = [
    (21, 5000), (22, 2000), (22, 120000), (22, 500000), (22, 1000000)
]

SCENARIO_SETS = {
    "quick": TEST_CASES_QUICK,
    "full": TEST_CASES_FULL,
}

DEFAULT_SCENARIO_SET = "quick"

DEBUG = True
TIMEOUT_LIMIT = 60
OUTPUT_BASE_DIR = "test_results"


def get_cpu_flags():
    try:
        with open("/proc/cpuinfo", "r", encoding="utf-8") as f:
            for line in f:
                if line.lower().startswith("flags"):
                    _, value = line.split(":", 1)
                    return set(value.strip().lower().split())
    except Exception:
        pass

    try:
        proc = subprocess.run(["lscpu"], capture_output=True, text=True, check=True)
        for line in proc.stdout.splitlines():
            if line.lower().startswith("flags:"):
                _, value = line.split(":", 1)
                return set(value.strip().lower().split())
    except Exception:
        pass
    return set()


def get_required_features(name):
    if "avx512" in name:
        return ["avx512f", "avx512bw"]
    if "avx2" in name:
        return ["avx2"]
    return []


def get_compile_cmd(name, source, binary_name, cpu_flags):
    base_cmd = ["g++", "-O3", source, "-o", binary_name, "-std=c++17", "-Wno-ignored-attributes", "-fopenmp"]
    required = get_required_features(name)
    missing = [feat for feat in required if feat not in cpu_flags]
    if missing:
        return None, missing

    for feat in required:
        base_cmd.append(f"-m{feat}")
    if DEBUG is False:
        base_cmd.append("-DNDEBUG")
    return base_cmd, []


def extract_truth_table(stdout_text):
    # Prefer the last pure bitstring line so checksum/prefix logs do not affect comparison.
    bit_lines = [line.strip() for line in stdout_text.splitlines() if re.fullmatch(r"[01]+", line.strip())]
    if bit_lines:
        return bit_lines[-1]
    return None


def run_one_case(exe, wff, timeout_limit):
    start_time = time.perf_counter()
    proc = subprocess.run(
        [exe],
        input=wff,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout_limit
    )
    end_time = time.perf_counter()
    return proc, end_time - start_time

def print_status(msg, state):
    # Keep track of longest message so we can overwrite prior content fully
    state["max_width"] = max(state.get("max_width", 0), len(msg))
    padded = msg.ljust(state["max_width"])
    print(f"\r{padded}", end="", flush=True)

def format_eta(seconds):
    seconds = int(max(0, seconds))
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    return f"{h:02d}:{m:02d}:{s:02d}"

def should_skip_due_to_prior_timeout(name, n, length, timeout_history):
    # Skip if this case is harder (>= in both dimensions) than a prior timeout for the same impl
    for prev_n, prev_len in timeout_history.get(name, []):
        if prev_n <= n and prev_len <= length:
            return True
    return False

def compile_binaries(cpp_files):
    print("\033[1;34m[1/3] Compiling implementations...\033[0m")
    status_state = {"max_width": 0}
    binaries = {}
    cpu_flags = get_cpu_flags()

    for name, source in cpp_files.items():
        if not os.path.exists(source):
            continue
        binary_name = f"./{source.replace('.cpp', '.out')}"
        compile_cmd, missing = get_compile_cmd(name, source, binary_name, cpu_flags)

        print_status(f"Compiling {name}...", status_state)
        if compile_cmd is None:
            print(f"\nSkipping {name}: missing CPU feature(s): {', '.join(missing)}")
            continue
        try:
            subprocess.run(compile_cmd, check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError:
            print(f"Error compiling {name}")
            continue
        binaries[name] = binary_name

    print()
    return binaries

def save_to_csv(results, output_dir, cpp_names):
    csv_path = os.path.join(output_dir, "benchmark_results.csv")
    headers = ["Scenario"] + cpp_names
    
    try:
        with open(csv_path, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow(headers)
            for scenario, data in results.items():
                row = [scenario] + [data.get(name, "N/A") for name in cpp_names]
                writer.writerow(row)
        print(f"\n\033[1;32m[3/3] Success! CSV report saved to: {csv_path}\033[0m")
    except Exception as e:
        print(f"\n\033[1;31mError saving CSV: {e}\033[0m")

def run_benchmark(binaries, test_cases, cpp_names, check_correctness=False):
    run_id = time.strftime("%Y%m%d_%H%M%S")
    current_run_dir = os.path.join(OUTPUT_BASE_DIR, f"run_{run_id}")
    os.makedirs(current_run_dir, exist_ok=True)
    
    print(f"\n\033[1;34m[2/3] Benchmarking... Data saved to: {current_run_dir}\033[0m")
    results = {}
    timeout_history = {name: [] for name in cpp_names}
    total_tests = len(test_cases) * len(cpp_names)
    completed = 0
    status_state = {"max_width": 0}

    reference_exe = binaries.get("legacy") if check_correctness else None
    if check_correctness and reference_exe is None:
        print("\033[1;33m[warn] Correctness mode requested, but legacy binary is unavailable.\033[0m")
        check_correctness = False
    
    for n, length in test_cases:
        case_name = f"n{n}_l{length}"
        case_dir = os.path.join(current_run_dir, case_name)
        os.makedirs(case_dir, exist_ok=True)
        
        case_label = f"var_count = {n}, wff_len = {length}"
        results[case_label] = {}
        
        try:
            wff = subprocess.check_output(
                [sys.executable, GENERATOR_SCRIPT, str(length), str(n), str(REUSE_RATE)], 
                universal_newlines=True
            ).strip()
            with open(os.path.join(case_dir, "input.txt"), "w") as f:
                f.write(wff)
        except Exception as e:
            print(f"Error generating formula: {e}")
            continue

        reference_stdout = None
        reference_bits = None
        reference_duration = None
        reference_returncode = None
        if check_correctness:
            try:
                ref_proc, reference_duration = run_one_case(reference_exe, wff, TIMEOUT_LIMIT)
                reference_stdout = ref_proc.stdout
                reference_returncode = ref_proc.returncode
                with open(os.path.join(case_dir, "legacy_reference_output.txt"), "w", encoding="utf-8") as ref_f:
                    ref_f.write(reference_stdout)
                reference_bits = extract_truth_table(reference_stdout) if ref_proc.returncode == 0 else None
            except subprocess.TimeoutExpired:
                reference_returncode = None
                reference_bits = None

        for name in cpp_names:
            exe = binaries.get(name)
            if exe is None:
                results[case_label][name] = "NOT_COMPILED"
                completed += 1
                remaining = total_tests - completed
                eta_max = remaining * TIMEOUT_LIMIT
                print_status(f"[{completed}/{total_tests}] left:{remaining:3d} ETA≤{format_eta(eta_max)}", status_state)
                continue
            if should_skip_due_to_prior_timeout(name, n, length, timeout_history):
                results[case_label][name] = "TIMEOUT"
                completed += 1
                remaining = total_tests - completed
                eta_max = remaining * TIMEOUT_LIMIT
                print_status(f"[{completed}/{total_tests}] left:{remaining:3d} ETA≤{format_eta(eta_max)}", status_state)
                continue
            print_status(f"[{completed + 1}/{total_tests}] {name:12} | n={n:02d} l={length:<6} | ETA≤{format_eta((total_tests - completed - 1) * TIMEOUT_LIMIT)}", status_state)
            
            output_file = os.path.join(case_dir, f"{name}_output.txt")
            
            try:
                if check_correctness and name == "legacy":
                    if reference_returncode is None:
                        results[case_label][name] = "TIMEOUT"
                        timeout_history[name].append((n, length))
                        completed += 1
                        remaining = total_tests - completed
                        eta_max = remaining * TIMEOUT_LIMIT
                        print_status(f"[{completed}/{total_tests}] left:{remaining:3d} ETA≤{format_eta(eta_max)}", status_state)
                        print()
                        continue

                    proc = subprocess.CompletedProcess([exe], reference_returncode, reference_stdout, "")
                    duration = reference_duration if reference_duration is not None else 0.0
                    with open(output_file, "w", encoding="utf-8") as out_f:
                        out_f.write(reference_stdout)
                else:
                    proc, duration = run_one_case(exe, wff, TIMEOUT_LIMIT)
                    with open(output_file, "w", encoding="utf-8") as out_f:
                        out_f.write(proc.stdout)
                
                if proc.returncode != 0:
                    results[case_label][name] = "RUNTIME_ERR"
                else:
                    if check_correctness and name != "legacy":
                        candidate_bits = extract_truth_table(proc.stdout)
                        if reference_bits is None or candidate_bits is None:
                            results[case_label][name] = "NO_OUTPUT"
                        elif candidate_bits != reference_bits:
                            results[case_label][name] = "WRONG_ANS"
                        else:
                            results[case_label][name] = f"{duration:.6f}s"
                    else:
                        results[case_label][name] = f"{duration:.6f}s"

            except subprocess.TimeoutExpired:
                results[case_label][name] = "TIMEOUT"
                timeout_history[name].append((n, length))
            except Exception:
                results[case_label][name] = "ERROR"

            completed += 1
            remaining = total_tests - completed
            eta_max = remaining * TIMEOUT_LIMIT
            print_status(f"[{completed}/{total_tests}] left:{remaining:3d} ETA≤{format_eta(eta_max)}", status_state)

            print()  # move cursor to new line after progress
    return results, current_run_dir

def print_final_report(results):
    col_width_scenario = 32
    col_width_data = 18
    cpp_names = list(next(iter(results.values())).keys()) if results else []
    total_width = col_width_scenario + (col_width_data * len(cpp_names))
    print("\n" + "=" * total_width)
    header = f"{'Scenario':<{col_width_scenario}}"
    for name in cpp_names:
        header += f"{name:>{col_width_data}}"
    print(header)
    print("-" * total_width + "\n")
    for case, data in results.items():
        row = f"{case:<{col_width_scenario}}"
        for name in cpp_names:
            row += f"{data.get(name, 'N/A'):>{col_width_data}}"
        print(row)
    print("=" * total_width + "\n")

def cleanup_binaries(binaries):
    for path in binaries.values():
        try:
            os.remove(path)
        except FileNotFoundError:
            continue
        except OSError as exc:
            print(f"Warning: could not remove {path}: {exc}")

def parse_args():
    parser = argparse.ArgumentParser(description="Benchmark logical formula solvers.")
    parser.add_argument(
        "--cpp",
        dest="cpp_list",
        help="Comma-separated list of implementation keys to run (default: all).",
    )
    parser.add_argument(
        "--scenario-set",
        choices=SCENARIO_SETS.keys(),
        default=DEFAULT_SCENARIO_SET,
        help=f"Select which scenario set to run (default: {DEFAULT_SCENARIO_SET}).",
    )
    parser.add_argument(
        "--check-correctness",
        action="store_true",
        help="Compare each implementation output against legacy output for each case.",
    )
    return parser.parse_args()

def resolve_cpp_selection(cpp_list_arg):
    if not cpp_list_arg:
        return CPP_FILES.copy()
    requested = [item.strip() for item in cpp_list_arg.split(',') if item.strip()]
    invalid = [name for name in requested if name not in CPP_FILES]
    if invalid:
        raise ValueError(f"Unknown implementation keys: {', '.join(invalid)}")
    return {name: CPP_FILES[name] for name in requested}

if __name__ == "__main__":
    args = parse_args()
    try:
        selected_cpp_files = resolve_cpp_selection(args.cpp_list)
    except ValueError as exc:
        print(f"\033[1;31m{exc}\033[0m")
        sys.exit(1)

    cpp_names = list(selected_cpp_files.keys())
    if not cpp_names:
        print("\033[1;31mNo implementations selected. Exiting.\033[0m")
        sys.exit(1)

    compile_targets = selected_cpp_files.copy()
    if args.check_correctness and "legacy" not in compile_targets:
        compile_targets["legacy"] = CPP_FILES["legacy"]

    selected_test_cases = SCENARIO_SETS.get(args.scenario_set, TEST_CASES_QUICK)

    bin_paths = {}
    try:
        bin_paths = compile_binaries(compile_targets)
        if not bin_paths:
            print("\033[1;31mNo binaries compiled. Exiting.\033[0m")
            sys.exit(1)

        benchmark_data, run_dir = run_benchmark(bin_paths, selected_test_cases, cpp_names, check_correctness=args.check_correctness)
        print_final_report(benchmark_data)
        save_to_csv(benchmark_data, run_dir, cpp_names)
    finally:
        cleanup_binaries(bin_paths)
