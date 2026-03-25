#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <stack>
#include <vector>

const size_t SIGMA = 26;
const size_t BLOCK_SIZE = 64;
const size_t STACK_SIZE = 65536;
const std::uint64_t ALL_ONES = ~0ULL;

inline int get_precedence(const char op) {
    if (op == '!') return 4;
    if (op == '&') return 3;
    if (op == '|') return 2;
    if (op == '>') return 1;
    if (op == '-') return 0;
    return -1;
}

std::string infix_to_postfix(std::string&& infix) {
    std::string postfix, ops;

    for (const char c : infix) {
        if (std::islower(c)) {
            postfix += c;
        } else if (c == '(') {
            ops.push_back(c);
        } else if (c == ')') {
            while (!ops.empty() && ops.back() != '(') {
                postfix += ops.back();
                ops.pop_back();
            }
            if (!ops.empty()) {
                ops.pop_back();
            }
        } else {
            while (!ops.empty() && ops.back() != '(') {
                int top_p = get_precedence(ops.back());
                int cur_p = get_precedence(c);
                if (c == '!' && top_p <= cur_p) {
                    break;
                }
                if (c != '!' && top_p < cur_p) {
                    break;
                }
                postfix += ops.back();
                ops.pop_back();
            }
            ops.push_back(c);
        }
    }
    while (!ops.empty()) {
        postfix += ops.back();
        ops.pop_back();
    }
    return postfix;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string expr;
    std::cin >> expr;

    std::vector<char> vars;
    for (const char c : expr) {
        if (std::islower(c)) vars.push_back(c);
    }
    std::sort(vars.begin(), vars.end());
    vars.erase(std::unique(vars.begin(), vars.end()), vars.end());

    std::string postfix = infix_to_postfix(std::move(expr));
    const std::size_t vars_count = vars.size();
    const std::uint64_t total_combinations = 1ULL << vars_count;

    std::array<std::uint64_t, SIGMA> value{};
    std::vector<uint8_t> results(total_combinations);
    std::array<std::uint64_t, STACK_SIZE> eval_stack;

    for (std::uint64_t i = 0; i < total_combinations; i += BLOCK_SIZE) {
        value.fill(0);
        for (int j = i; j < i + BLOCK_SIZE && j < total_combinations; ++j) {
            for (int v = 0; v < vars_count; ++v) {
                size_t f = (j >> (vars_count - 1 - v)) & 1;
                value[vars[v] - 'a'] ^= f << (j - i);
            }
        }

        int top = -1;
        for (const char c : postfix) {
            if (std::islower(c)) {
                eval_stack[++top] = value[c - 'a'];
            } else if (c == '!') {
                eval_stack[top] = ~eval_stack[top];
            } else {
                std::uint64_t right = eval_stack[top--];
                std::uint64_t left = eval_stack[top];
                if (c == '&') {
                    eval_stack[top] = left & right;
                } else if (c == '|') {
                    eval_stack[top] = left | right;
                } else if (c == '>') {
                    eval_stack[top] = ~left | right;
                } else if (c == '-') {
                    eval_stack[top] = ~(left ^ right);
                }
            }
        }
        uint64_t block_res = eval_stack[0];
        for (size_t k = 0; k < BLOCK_SIZE && (i + k) < total_combinations; ++k) {
            results[i + k] = (block_res >> k) & 1ULL;
        }
    }

#ifndef NDEBUG
    for (uint8_t res : results) {
        std::cout << (int)res;
    }
    std::cout << "\n";
#endif

    return 0;
}
