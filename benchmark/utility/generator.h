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

#ifndef GENERATOR_H
#define GENERATOR_H

#include <optional>
#include <string>
#include <random>
#include <cstdint>
#include <vector>
#include <algorithm>

struct GeneratorOptions
{
    int length = 1;
    int var_count = 1;
    double reuse_rate = 0.0;
    std::optional<std::uint32_t> seed = std::nullopt;
};

class WffGenerator
{
private:
    std::string Build(int length, double reuse_rate);

    int var_count_ = 1;
    std::string variables_;
    std::vector<char> binary_ops_ {'&', '|', '>', '-'};
    std::vector<std::string> sub_wff_pool_;

    std::mt19937 rng_;
    std::uniform_real_distribution<double> unit_dist_ {0.0, 1.0};

public:
    explicit WffGenerator(int var_count, std::mt19937 rng);
    ~WffGenerator() = default;
    std::string Generate(int length, double reuse_rate = 0.0);
};

bool ParseArgs(int argc, char** argv, GeneratorOptions& options);

std::string BuildFormula(const GeneratorOptions& options);

#endif