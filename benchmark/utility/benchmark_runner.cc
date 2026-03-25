
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Richard Qin
//
// This file is part of the benchmark project.
//
// This program is free software : you can redistribute it and / or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation,
// either version 3 of the License, or
//  (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct RunResult {
    int exit_code = -1;
    double duration_sec = 0.0;
    bool timed_out = false;
    bool launch_error = false;
};

struct ProgressState {
    size_t max_width = 0;
};

struct RunnerArgs {
    std::string scenario_set = "quick";
    bool check_correctness = false;
    std::string cpp_arg;
    std::string build_dir;
    std::string source_dir;
    bool no_build = false;
    bool debug_build = true;
};

const int kTimeoutLimitSec = 60;
const int kReuseRate = 0;
const std::string kOutputBaseDir = "test_results";

const std::vector<std::string> kDefaultTargetOrder = {
    "checker",
    "legacy",
    "recursion",
    "simd_ord_fill",
    "simd_ord_precalc",
    "simd_avx2_fill",
    "simd_avx2_precalc",
    "simd_avx512_precalc",
    "simd_avx512_omp_parallel",
    "simd_avx2_omp_parallel",
};

const std::map<std::string, std::string> kKnownTargets = {
    {"checker", "checker"},
    {"legacy", "legacy"},
    {"recursion", "recursion"},
    {"simd_ord_fill", "simd_ord_fill"},
    {"simd_ord_precalc", "simd_ord_precalc"},
    {"simd_avx2_fill", "simd_avx2_fill"},
    {"simd_avx2_precalc", "simd_avx2_precalc"},
    {"simd_avx512_precalc", "simd_avx512_precalc"},
    {"simd_avx512_omp_parallel", "simd_avx512_omp_parallel"},
    {"simd_avx2_omp_parallel", "simd_avx2_omp_parallel"},
};

std::string shell_escape(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out += "'";
    return out;
}

std::string now_run_id() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

std::vector<std::pair<int, int>> build_quick_cases() {
    return {
        {21, 5000},
        {22, 2000},
        {22, 120000},
        {22, 500000},
        {22, 1000000},
    };
}

void append_cases(std::vector<std::pair<int, int>>& out, int n, const std::vector<int>& lengths) {
    for (int len : lengths) {
        out.emplace_back(n, len);
    }
}

std::vector<std::pair<int, int>> build_full_cases() {
    std::vector<std::pair<int, int>> out;

    for (int n = 2; n <= 10; ++n) {
        out.emplace_back(n, 20);
    }
    for (int n = 2; n <= 10; ++n) {
        out.emplace_back(n, 200);
    }
    for (int n = 11; n <= 19; ++n) {
        out.emplace_back(n, 200);
    }

    append_cases(out, 20, {
        200, 400, 600, 800, 1000, 1200, 1400, 1600, 1800, 2000,
        2500, 3000, 3500, 4000, 4500, 5000, 5500, 6000, 6500, 7000,
        7500, 8000, 8500, 9000, 9500, 10000
    });

    append_cases(out, 21, {
        200, 400, 600, 800, 1000, 1200, 1400, 1600, 1800, 2000,
        2500, 3000, 3500, 4000, 4500, 5000, 5500, 6000, 6500, 7000,
        7500, 8000, 8500, 9000, 9500, 10000,
        12000, 14000, 16000, 18000, 20000, 24000, 28000, 32000,
        36000, 40000, 44000, 48000, 52000, 56000, 60000, 64000,
        70000, 75000, 80000, 90000, 100000, 110000, 120000, 130000,
        140000, 150000, 160000, 170000, 180000, 190000, 200000
    });

    append_cases(out, 22, {
        1000, 4000, 6000, 8000, 10000, 12000, 15000, 18000, 20000,
        24000, 28000, 32000, 36000, 40000, 44000, 48000, 52000, 56000,
        60000, 64000, 70000, 75000, 80000, 90000, 100000, 110000,
        120000, 130000, 140000, 150000, 160000, 170000, 180000,
        190000, 200000, 300000, 400000, 500000, 600000, 700000,
        800000, 900000, 1000000
    });

    return out;
}

std::string read_file_as_string(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string extract_truth_table(const std::string& text) {
    static const std::regex bitline("^[01]+$");
    std::istringstream iss(text);
    std::string line;
    std::string last;
    while (std::getline(iss, line)) {
        if (std::regex_match(line, bitline)) {
            last = line;
        }
    }
    return last;
}

RunResult run_binary_with_input(
    const std::filesystem::path& exe,
    const std::string& input,
    int timeout_sec,
    const std::filesystem::path& stdout_file,
    const std::filesystem::path& stderr_file) {

    RunResult result;
    const auto t0 = std::chrono::steady_clock::now();

    std::filesystem::path input_file = stdout_file;
    input_file += ".input.tmp";
    {
        std::ofstream out(input_file);
        out << input;
    }

    std::ostringstream cmd;
    cmd << "timeout " << timeout_sec << "s " << shell_escape(exe.string())
        << " < " << shell_escape(input_file.string())
        << " > " << shell_escape(stdout_file.string())
        << " 2> " << shell_escape(stderr_file.string());

    int raw = std::system(cmd.str().c_str());

    const auto t1 = std::chrono::steady_clock::now();
    result.duration_sec = std::chrono::duration<double>(t1 - t0).count();

    std::error_code ec;
    std::filesystem::remove(input_file, ec);

    if (raw == -1) {
        result.launch_error = true;
        return result;
    }

    if (WIFEXITED(raw)) {
        result.exit_code = WEXITSTATUS(raw);
        if (result.exit_code == 124) {
            result.timed_out = true;
        }
    } else {
        result.launch_error = true;
    }

    return result;
}

bool run_generator(
    const std::filesystem::path& generator_exe,
    int length,
    int vars,
    int reuse_rate,
    std::string& out_formula) {

    std::ostringstream cmd;
    cmd << shell_escape(generator_exe.string()) << " " << length << " " << vars << " " << reuse_rate;
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        return false;
    }

    char buf[4096];
    std::string output;
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        output += buf;
    }
    const int rc = pclose(pipe);
    if (rc != 0) {
        return false;
    }

    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    out_formula = output;
    return !out_formula.empty();
}

bool should_skip_due_to_prior_timeout(
    const std::string& name,
    int n,
    int len,
    const std::unordered_map<std::string, std::vector<std::pair<int, int>>>& timeout_history) {

    auto it = timeout_history.find(name);
    if (it == timeout_history.end()) {
        return false;
    }
    for (const auto& prev : it->second) {
        if (prev.first <= n && prev.second <= len) {
            return true;
        }
    }
    return false;
}

std::string fmt_sec(double sec) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << sec << "s";
    return oss.str();
}

std::string fmt_eta(int seconds) {
    if (seconds < 0) {
        seconds = 0;
    }
    const int h = seconds / 3600;
    const int m = (seconds % 3600) / 60;
    const int s = seconds % 60;
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << h << ":"
        << std::setw(2) << m << ":"
        << std::setw(2) << s;
    return oss.str();
}

void print_status(const std::string& msg, ProgressState& state) {
    state.max_width = std::max(state.max_width, msg.size());
    std::string padded = msg;
    padded.append(state.max_width - msg.size(), ' ');
    std::cout << "\r" << padded << std::flush;
}

std::string csv_escape(const std::string& s) {
    bool need_quotes = s.find(',') != std::string::npos || s.find('"') != std::string::npos || s.find('\n') != std::string::npos || s.find('\r') != std::string::npos;
    if (!need_quotes) {
        return s;
    }
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') {
            out += "\"\"";
        } else {
            out.push_back(c);
        }
    }
    out += "\"";
    return out;
}

void print_final_report(const std::vector<std::pair<std::string, std::map<std::string, std::string>>>& results,
                        const std::vector<std::string>& cpp_names) {
    const int col_width_scenario = 32;
    const int col_width_data = 18;
    const int total_width = col_width_scenario + static_cast<int>(col_width_data * cpp_names.size());

    std::cout << "\n" << std::string(total_width, '=') << "\n";
    std::cout << std::left << std::setw(col_width_scenario) << "Scenario";
    for (const auto& name : cpp_names) {
        std::cout << std::right << std::setw(col_width_data) << name;
    }
    std::cout << "\n" << std::string(total_width, '-') << "\n\n";

    for (const auto& kv : results) {
        std::cout << std::left << std::setw(col_width_scenario) << kv.first;
        for (const auto& name : cpp_names) {
            auto it = kv.second.find(name);
            const std::string val = (it == kv.second.end()) ? "N/A" : it->second;
            std::cout << std::right << std::setw(col_width_data) << val;
        }
        std::cout << "\n";
    }
    std::cout << std::string(total_width, '=') << "\n\n";
}

void save_csv(const std::filesystem::path& output_dir,
              const std::vector<std::pair<std::string, std::map<std::string, std::string>>>& results,
              const std::vector<std::string>& cpp_names) {
    std::filesystem::path csv = output_dir / "benchmark_results.csv";
    std::ofstream out(csv);
    out << "Scenario";
    for (const auto& name : cpp_names) {
        out << "," << name;
    }
    out << "\n";

    for (const auto& row : results) {
        out << csv_escape(row.first);
        for (const auto& name : cpp_names) {
            auto it = row.second.find(name);
            out << "," << csv_escape(it == row.second.end() ? "N/A" : it->second);
        }
        out << "\n";
    }

    std::cout << "\n\033[1;32m[3/3] Success! CSV report saved to: " << csv.string() << "\033[0m\n";
}

std::vector<std::string> split_csv_list(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream iss(s);
    while (std::getline(iss, cur, ',')) {
        size_t l = cur.find_first_not_of(" \t");
        size_t r = cur.find_last_not_of(" \t");
        if (l == std::string::npos) {
            continue;
        }
        out.push_back(cur.substr(l, r - l + 1));
    }
    return out;
}

bool parse_bool_flag(const std::string& s, bool& out) {
    if (s == "on" || s == "ON" || s == "true" || s == "1") {
        out = true;
        return true;
    }
    if (s == "off" || s == "OFF" || s == "false" || s == "0") {
        out = false;
        return true;
    }
    return false;
}

bool run_shell_cmd(const std::string& cmd) {
    const int rc = std::system(cmd.c_str());
    return rc == 0;
}

bool configure_and_build(const std::filesystem::path& source_dir,
                        const std::filesystem::path& build_dir,
                        bool debug_build) {
    std::filesystem::create_directories(build_dir);

    std::ostringstream configure_cmd;
    configure_cmd << "cmake -S " << shell_escape(source_dir.string())
                  << " -B " << shell_escape(build_dir.string())
                  << " -DBENCHMARK_DEBUG=" << (debug_build ? "ON" : "OFF");
    if (!run_shell_cmd(configure_cmd.str())) {
        return false;
    }

    std::ostringstream build_cmd;
    build_cmd << "cmake --build " << shell_escape(build_dir.string()) << " -j";
    return run_shell_cmd(build_cmd.str());
}

bool parse_args(int argc, char** argv, RunnerArgs& args) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--scenario-set" && i + 1 < argc) {
            args.scenario_set = argv[++i];
        } else if (arg == "--cpp" && i + 1 < argc) {
            args.cpp_arg = argv[++i];
        } else if (arg == "--check-correctness") {
            args.check_correctness = true;
        } else if (arg == "--build-dir" && i + 1 < argc) {
            args.build_dir = argv[++i];
        } else if (arg == "--source-dir" && i + 1 < argc) {
            args.source_dir = argv[++i];
        } else if (arg == "--no-build") {
            args.no_build = true;
        } else if (arg == "--debug-build" && i + 1 < argc) {
            bool parsed = false;
            if (!parse_bool_flag(argv[++i], parsed)) {
                return false;
            }
            args.debug_build = parsed;
        } else {
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    RunnerArgs args;
    if (!parse_args(argc, argv, args)) {
        std::cerr
            << "Usage: benchmark_runner [--scenario-set quick|full] [--cpp a,b,c] [--check-correctness]"
            << " [--build-dir path] [--source-dir path] [--no-build] [--debug-build on|off]\n";
        return 1;
    }

    std::vector<std::string> cpp_names;
    if (args.cpp_arg.empty()) {
        for (const auto& name : kDefaultTargetOrder) {
            cpp_names.push_back(name);
        }
    } else {
        cpp_names = split_csv_list(args.cpp_arg);
        for (const auto& name : cpp_names) {
            if (kKnownTargets.find(name) == kKnownTargets.end()) {
                std::cerr << "Unknown implementation key: " << name << "\n";
                return 1;
            }
        }
    }

    std::vector<std::pair<int, int>> test_cases;
    if (args.scenario_set == "quick") {
        test_cases = build_quick_cases();
    } else if (args.scenario_set == "full") {
        test_cases = build_full_cases();
    } else {
        std::cerr << "Unknown scenario set: " << args.scenario_set << "\n";
        return 1;
    }

    const std::filesystem::path self_dir = std::filesystem::absolute(argv[0]).parent_path();
    const std::filesystem::path build_dir = args.build_dir.empty() ? self_dir : std::filesystem::absolute(args.build_dir);
    const std::filesystem::path source_dir = args.source_dir.empty() ? build_dir.parent_path() : std::filesystem::absolute(args.source_dir);

    if (!args.no_build) {
        std::cout << "\033[1;34m[1/3] Building with CMake...\033[0m\n";
        if (!configure_and_build(source_dir, build_dir, args.debug_build)) {
            std::cerr << "\033[1;31mCMake configure/build failed.\033[0m\n";
            return 1;
        }
    } else {
        std::cout << "\033[1;34m[1/3] Skipping build (using existing outputs).\033[0m\n";
    }

    const std::filesystem::path exe_dir = build_dir;
    const std::filesystem::path generator_exe = exe_dir / "generator";
    if (!std::filesystem::exists(generator_exe)) {
        std::cerr << "Missing generator executable: " << generator_exe.string() << "\n";
        return 1;
    }

    std::map<std::string, std::filesystem::path> binaries;
    for (const auto& name : cpp_names) {
        const auto it = kKnownTargets.find(name);
        const std::filesystem::path path = exe_dir / it->second;
        if (std::filesystem::exists(path)) {
            binaries[name] = path;
        }
    }

    std::filesystem::path legacy_exe;
    if (args.check_correctness) {
        auto it = kKnownTargets.find("legacy");
        const std::filesystem::path path = exe_dir / it->second;
        if (std::filesystem::exists(path)) {
            legacy_exe = path;
        } else {
            std::cout << "\033[1;33m[warn] Correctness mode requested, but legacy binary is unavailable.\033[0m\n";
            args.check_correctness = false;
        }
    }

    if (binaries.empty()) {
        std::cerr << "No selected binaries found in build directory.\n";
        return 1;
    }

    const std::string run_id = now_run_id();
    const std::filesystem::path run_dir = std::filesystem::path(kOutputBaseDir) / ("run_" + run_id);
    std::filesystem::create_directories(run_dir);

    std::cout << "\n\033[1;34m[2/3] Benchmarking... Data saved to: " << run_dir.string() << "\033[0m\n";

    std::vector<std::pair<std::string, std::map<std::string, std::string>>> results;
    std::unordered_map<std::string, std::vector<std::pair<int, int>>> timeout_history;
    ProgressState progress_state;

    const int total_tests = static_cast<int>(test_cases.size() * cpp_names.size());
    int completed = 0;

    for (const auto& tc : test_cases) {
        const int n = tc.first;
        const int len = tc.second;
        const std::string case_name = "n" + std::to_string(n) + "_l" + std::to_string(len);
        const std::string case_label = "var_count = " + std::to_string(n) + ", wff_len = " + std::to_string(len);
        const std::filesystem::path case_dir = run_dir / case_name;
        std::filesystem::create_directories(case_dir);

        std::string wff;
        if (!run_generator(generator_exe, len, n, kReuseRate, wff)) {
            std::cerr << "Error generating formula for " << case_name << "\n";
            continue;
        }

        results.emplace_back(case_label, std::map<std::string, std::string>{});
        auto& case_row = results.back().second;

        {
            std::ofstream in(case_dir / "input.txt");
            in << wff;
        }

        std::string reference_bits;
        bool reference_timed_out = false;
        bool reference_launch_error = false;
        bool reference_has_run = false;
        double reference_duration = 0.0;
        int reference_exit_code = -1;
        std::string reference_stdout;

        if (args.check_correctness) {
            const std::filesystem::path ref_out = case_dir / "legacy_reference_output.txt";
            const std::filesystem::path ref_err = case_dir / "legacy_reference_error.txt";
            RunResult rr = run_binary_with_input(legacy_exe, wff, kTimeoutLimitSec, ref_out, ref_err);
            reference_timed_out = rr.timed_out;
            reference_launch_error = rr.launch_error;
            if (!rr.timed_out && !rr.launch_error) {
                reference_has_run = true;
                reference_stdout = read_file_as_string(ref_out);
                reference_bits = extract_truth_table(reference_stdout);
                reference_duration = rr.duration_sec;
                reference_exit_code = rr.exit_code;
            }
        }

        for (const auto& name : cpp_names) {
            auto& slot = case_row[name];
            auto bit = binaries.find(name);
            if (bit == binaries.end()) {
                slot = "NOT_COMPILED";
                ++completed;
                const int remaining = total_tests - completed;
                print_status("[" + std::to_string(completed) + "/" + std::to_string(total_tests) + "] left:" + std::to_string(remaining) + " ETA<=" + fmt_eta(remaining * kTimeoutLimitSec), progress_state);
                continue;
            }

            if (should_skip_due_to_prior_timeout(name, n, len, timeout_history)) {
                slot = "TIMEOUT";
                ++completed;
                const int remaining = total_tests - completed;
                print_status("[" + std::to_string(completed) + "/" + std::to_string(total_tests) + "] left:" + std::to_string(remaining) + " ETA<=" + fmt_eta(remaining * kTimeoutLimitSec), progress_state);
                continue;
            }

            print_status("[" + std::to_string(completed + 1) + "/" + std::to_string(total_tests) + "] " + name + " | n=" + std::to_string(n) + " l=" + std::to_string(len) + " | ETA<=" + fmt_eta((total_tests - completed - 1) * kTimeoutLimitSec), progress_state);

            if (args.check_correctness && name == "legacy") {
                if (reference_timed_out) {
                    slot = "TIMEOUT";
                    timeout_history[name].emplace_back(n, len);
                } else if (reference_launch_error || !reference_has_run) {
                    slot = "ERROR";
                } else if (reference_exit_code != 0) {
                    slot = "RUNTIME_ERR";
                } else {
                    slot = fmt_sec(reference_duration);
                    std::ofstream out(case_dir / (name + "_output.txt"));
                    out << reference_stdout;
                }
            } else {
                const std::filesystem::path out_path = case_dir / (name + "_output.txt");
                const std::filesystem::path err_path = case_dir / (name + "_error.txt");
                RunResult rr = run_binary_with_input(bit->second, wff, kTimeoutLimitSec, out_path, err_path);

                if (rr.timed_out) {
                    slot = "TIMEOUT";
                    timeout_history[name].emplace_back(n, len);
                } else if (rr.launch_error) {
                    slot = "ERROR";
                } else if (rr.exit_code != 0) {
                    slot = "RUNTIME_ERR";
                } else if (args.check_correctness && name != "legacy") {
                    const std::string cand_stdout = read_file_as_string(out_path);
                    const std::string cand_bits = extract_truth_table(cand_stdout);
                    if (reference_bits.empty() || cand_bits.empty()) {
                        slot = "NO_OUTPUT";
                    } else if (cand_bits != reference_bits) {
                        slot = "WRONG_ANS";
                    } else {
                        slot = fmt_sec(rr.duration_sec);
                    }
                } else {
                    slot = fmt_sec(rr.duration_sec);
                }
            }

            ++completed;
            const int remaining = total_tests - completed;
            print_status("[" + std::to_string(completed) + "/" + std::to_string(total_tests) + "] left:" + std::to_string(remaining) + " ETA<=" + fmt_eta(remaining * kTimeoutLimitSec), progress_state);
            std::cout << "\n";
        }
    }

    std::cout << "\n";
    print_final_report(results, cpp_names);
    save_csv(run_dir, results, cpp_names);
    return 0;
}
