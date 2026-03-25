#!/usr/bin/env bash

# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Richard Qin
#
# This file is part of the benchmark project.
#
# This program is free software : you can redistribute it and / or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation,
# either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

DEFAULT_BUILD_DIR="build"
DEFAULT_SCENARIO="quick"
DEFAULT_DEBUG_BUILD="on"

INTERACTIVE_MODE="yes"
BUILD_DIR="$DEFAULT_BUILD_DIR"
DO_BUILD="yes"
DEBUG_BUILD="$DEFAULT_DEBUG_BUILD"
SCENARIO="$DEFAULT_SCENARIO"
CPP_LIST=""
CHECK_CORRECTNESS="no"

print_usage() {
  cat <<'EOF'
Usage:
  ./run_benchmark.sh
  ./run_benchmark.sh --non-interactive [options]

Options:
  --non-interactive                 Run without prompts.
  --build-dir <path>               Build directory (default: build).
  --build <yes|no>                 Whether to run CMake build step (default: yes).
  --debug-build <on|off>           BENCHMARK_DEBUG value passed to CMake (default: on).
  --scenario-set <quick|full>      Benchmark scenario set (default: quick).
  --cpp <csv>                      Comma-separated implementation keys.
  --check-correctness              Enable correctness check against legacy.
  --no-check-correctness           Disable correctness check.
  -h, --help                       Show this help.

Examples:
  ./run_benchmark_interactive.sh
  ./run_benchmark_interactive.sh --non-interactive --build-dir build --build yes --scenario-set quick --cpp recursion
  ./run_benchmark_interactive.sh --non-interactive --build no --scenario-set full --check-correctness
EOF
}

normalize_yes_no() {
  local v="${1,,}"
  case "$v" in
    y|yes|true|1)
      echo "yes"
      ;;
    n|no|false|0)
      echo "no"
      ;;
    *)
      return 1
      ;;
  esac
}

normalize_on_off() {
  local v="${1,,}"
  case "$v" in
    on|true|1)
      echo "on"
      ;;
    off|false|0)
      echo "off"
      ;;
    *)
      return 1
      ;;
  esac
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --non-interactive)
        INTERACTIVE_MODE="no"
        shift
        ;;
      --build-dir)
        [[ $# -ge 2 ]] || { echo "Missing value for --build-dir"; exit 1; }
        BUILD_DIR="$2"
        shift 2
        ;;
      --build)
        [[ $# -ge 2 ]] || { echo "Missing value for --build"; exit 1; }
        DO_BUILD="$(normalize_yes_no "$2")" || { echo "Invalid value for --build: $2"; exit 1; }
        shift 2
        ;;
      --debug-build)
        [[ $# -ge 2 ]] || { echo "Missing value for --debug-build"; exit 1; }
        DEBUG_BUILD="$(normalize_on_off "$2")" || { echo "Invalid value for --debug-build: $2"; exit 1; }
        shift 2
        ;;
      --scenario-set)
        [[ $# -ge 2 ]] || { echo "Missing value for --scenario-set"; exit 1; }
        case "${2,,}" in
          quick|full)
            SCENARIO="${2,,}"
            ;;
          *)
            echo "Invalid value for --scenario-set: $2"
            exit 1
            ;;
        esac
        shift 2
        ;;
      --cpp)
        [[ $# -ge 2 ]] || { echo "Missing value for --cpp"; exit 1; }
        CPP_LIST="$2"
        shift 2
        ;;
      --check-correctness)
        CHECK_CORRECTNESS="yes"
        shift
        ;;
      --no-check-correctness)
        CHECK_CORRECTNESS="no"
        shift
        ;;
      -h|--help)
        print_usage
        exit 0
        ;;
      *)
        echo "Unknown argument: $1"
        print_usage
        exit 1
        ;;
    esac
  done
}

print_header() {
  echo "==============================================="
  echo " Logic Benchmark Interactive Runner"
  echo "==============================================="
  echo
}

ask_build_dir() {
  local answer
  read -r -p "Build directory [${DEFAULT_BUILD_DIR}]: " answer
  if [[ -z "$answer" ]]; then
    BUILD_DIR="$DEFAULT_BUILD_DIR"
  else
    BUILD_DIR="$answer"
  fi
}

ask_rebuild() {
  local answer
  read -r -p "Run CMake configure+build before benchmark? (Y/n): " answer
  answer="${answer:-Y}"
  case "${answer,,}" in
    y|yes)
      DO_BUILD="yes"
      ;;
    n|no)
      DO_BUILD="no"
      ;;
    *)
      echo "Invalid input. Using default: yes"
      DO_BUILD="yes"
      ;;
  esac
}

ask_debug_build() {
  local answer
  read -r -p "Build in debug-compatible mode (BENCHMARK_DEBUG=ON)? (Y/n): " answer
  answer="${answer:-Y}"
  case "${answer,,}" in
    y|yes)
      DEBUG_BUILD="on"
      ;;
    n|no)
      DEBUG_BUILD="off"
      ;;
    *)
      echo "Invalid input. Using default: on"
      DEBUG_BUILD="$DEFAULT_DEBUG_BUILD"
      ;;
  esac
}

ask_scenario() {
  local answer
  read -r -p "Scenario set (quick/full) [${DEFAULT_SCENARIO}]: " answer
  answer="${answer:-$DEFAULT_SCENARIO}"
  case "${answer,,}" in
    quick|full)
      SCENARIO="${answer,,}"
      ;;
    *)
      echo "Invalid input. Using default: ${DEFAULT_SCENARIO}"
      SCENARIO="$DEFAULT_SCENARIO"
      ;;
  esac
}

ask_cpp_targets() {
  echo
  echo "Enter implementation keys as comma-separated list, or leave empty for defaults."
  echo "Examples: recursion"
  echo "          legacy,simd_avx2_omp_parallel"
  local answer
  read -r -p "--cpp value: " answer
  CPP_LIST="$answer"
}

ask_correctness() {
  local answer
  read -r -p "Enable correctness check against legacy? (y/N): " answer
  answer="${answer:-N}"
  case "${answer,,}" in
    y|yes)
      CHECK_CORRECTNESS="yes"
      ;;
    n|no)
      CHECK_CORRECTNESS="no"
      ;;
    *)
      echo "Invalid input. Using default: no"
      CHECK_CORRECTNESS="no"
      ;;
  esac
}

build_runner_if_needed() {
  if [[ "$DO_BUILD" == "yes" ]]; then
    echo
    echo "[Step 1/2] Configuring and building with CMake..."
    cmake -S . -B "$BUILD_DIR" -DBENCHMARK_DEBUG="${DEBUG_BUILD^^}"
    cmake --build "$BUILD_DIR" -j
  fi
}

run_benchmark() {
  local runner_path="$BUILD_DIR/benchmark_runner"
  if [[ ! -x "$runner_path" ]]; then
    echo
    echo "Error: benchmark runner not found or not executable at: $runner_path"
    echo "Tip: choose build step = yes, or verify your build directory."
    exit 1
  fi

  local cmd=("$runner_path" "--build-dir" "$BUILD_DIR" "--source-dir" ".")

  if [[ "$DO_BUILD" == "no" ]]; then
    cmd+=("--no-build")
  fi

  cmd+=("--debug-build" "$DEBUG_BUILD")
  cmd+=("--scenario-set" "$SCENARIO")

  if [[ -n "$CPP_LIST" ]]; then
    cmd+=("--cpp" "$CPP_LIST")
  fi

  if [[ "$CHECK_CORRECTNESS" == "yes" ]]; then
    cmd+=("--check-correctness")
  fi

  echo
  echo "[Step 2/2] Running benchmark..."
  echo "Command: ${cmd[*]}"
  echo
  "${cmd[@]}"
}

main() {
  parse_args "$@"
  print_header
  if [[ "$INTERACTIVE_MODE" == "yes" ]]; then
    ask_build_dir
    ask_rebuild
    ask_debug_build
    ask_scenario
    ask_cpp_targets
    ask_correctness
  else
    echo "Running in non-interactive mode"
    echo "Build directory: $BUILD_DIR"
    echo "Run build step: $DO_BUILD"
    echo "Debug build: $DEBUG_BUILD"
    echo "Scenario: $SCENARIO"
    if [[ -n "$CPP_LIST" ]]; then
      echo "CPP targets: $CPP_LIST"
    else
      echo "CPP targets: default"
    fi
    echo "Correctness check: $CHECK_CORRECTNESS"
  fi
  build_runner_if_needed
  run_benchmark
}

main "$@"
