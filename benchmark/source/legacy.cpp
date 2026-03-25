#include <algorithm>
#include <iostream>
#include <numeric>
#include <stack>
#include <vector>
#include <cstdint>

const size_t SIGMA = 26;

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

    size_t value = 0;
    std::vector<bool> results;
    results.reserve(total_combinations);
    for (std::uint64_t i = 0; i < total_combinations; ++i) {
        value = 0;
        for (int v = 0; v < vars_count; ++v) {
            size_t f = (i >> (vars_count - 1 - v)) & 1;
            value ^= f << (vars[v] - 'a');
        }
        std::stack<bool> eval_stack;
        for (const char c : postfix) {
            if (std::islower(c)) {
                eval_stack.push(value >> (c - 'a') & 1);
            } else if (c == '!') {
                bool operand = eval_stack.top();
                eval_stack.pop();
                eval_stack.push(!operand);
            } else {
                bool right = eval_stack.top();
                eval_stack.pop();
                bool left = eval_stack.top();
                eval_stack.pop();
                if (c == '&') {
                    eval_stack.push(left && right);
                } else if (c == '|') {
                    eval_stack.push(left || right);
                } else if (c == '>') {
                    eval_stack.push(!left || right);
                } else if (c == '-') {
                    eval_stack.push(left == right);
                }
            }
        }
        bool result = eval_stack.top();
        results.push_back(result);
    }

    #ifndef NDEBUG
    for (int i = 0; i < total_combinations; ++i) {
        std::cout << results[i];
    }
    std::cout << "\n";
    #endif

    return 0;
}
