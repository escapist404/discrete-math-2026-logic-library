
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

#include "generator.h"

#include <iostream>
#include <stdexcept>

WffGenerator::WffGenerator(int var_count, std::mt19937 rng) : rng_(rng) {
    var_count_ = std::clamp(var_count, 1, 26);
    std::string variables_str = "abcdefghijklmnopqrstuvwxyz";
    variables_ = variables_str.substr(0, static_cast<size_t>(var_count_));
}

std::string WffGenerator::Generate(int length, double reuse_rate) {
    sub_wff_pool_.clear();
    if (length <= 1) {
        length = 1;
    } else if (length % 2 == 0) {
        length -= 1;
    }
    return Build(length, reuse_rate);
}

std::string WffGenerator::Build(int length, double reuse_rate) {
    if (length > 3 && sub_wff_pool_.size() > 5 && unit_dist_(rng_) < reuse_rate) {
        std::uniform_int_distribution<size_t> pool_dist(0, sub_wff_pool_.size() - 1);
        return sub_wff_pool_[pool_dist(rng_)];
    }

    std::string res;
    if (length <= 1) {
        std::uniform_int_distribution<size_t> var_dist(0, variables_.size() - 1);
        res.push_back(variables_[var_dist(rng_)]);
    } else if (length == 2) {
        std::uniform_int_distribution<size_t> var_dist(0, variables_.size() - 1);
        res.push_back('!');
        res.push_back(variables_[var_dist(rng_)]);
    } else if (unit_dist_(rng_) < 0.15) {
        res = "!" + Build(length - 1, reuse_rate);
    } else {
        std::uniform_int_distribution<size_t> op_dist(0, binary_ops_.size() - 1);
        char op = binary_ops_[op_dist(rng_)];

        std::uniform_int_distribution<int> k_dist(1, length - 2);
        int k = k_dist(rng_);
        if (k % 2 == 0) {
            k += 1;
        }
        if (k >= length - 1) {
            k = length - 2;
        }

        std::string left = Build(k, reuse_rate);
        std::string right = Build(length - 1 - k, reuse_rate);
        res = "(" + left + op + right + ")";
    }

    if (res.size() < 50) {
        sub_wff_pool_.push_back(res);
    }

    return res;
}

bool ParseArgs(int argc, char** argv, GeneratorOptions& options) {
    if (argc < 3 || argc > 4) {
        return false;
    }

    try {
        options.length = std::stoi(argv[1]);
        options.var_count = std::stoi(argv[2]);
        options.reuse_rate = (argc == 4) ? std::stod(argv[3]) : 0.0;
    } catch (const std::exception&) {
        return false;
    }

    if (options.reuse_rate < 0.0 || options.reuse_rate > 1.0) {
        return false;
    }

    return true;
}

std::string BuildFormula(const GeneratorOptions& options) {
    std::mt19937 rng(options.seed.has_value() ? *options.seed : std::random_device{}());
    WffGenerator generator(options.var_count, rng);
    return generator.Generate(options.length, options.reuse_rate);
}

int main(int argc, char** argv) {
    GeneratorOptions options;
    if (!ParseArgs(argc, argv, options)) {
        std::cerr << "Usage: generator <length> <vars> [reuse_rate]\n";
        return 1;
    }

    std::cout << BuildFormula(options) << '\n';
    return 0;
    
}