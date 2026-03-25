#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <immintrin.h>
#include <omp.h>

const int BLOCK_SIZE = 512;
const int MAX_VARS = 26;

inline int get_precedence(char op) {
    if (op == '!') return 4;
    if (op == '&') return 3;
    if (op == '|') return 2;
    if (op == '>') return 1;
    if (op == '-') return 0;
    return -1;
}

std::string infix_to_postfix(const std::string& infix) {
    std::string postfix;
    std::vector<char> ops;
    for (char c : infix) {
        if (std::isspace(c)) continue;
        if (std::islower(c)) {
            postfix += c;
        } else if (c == '(') {
            ops.push_back(c);
        } else if (c == ')') {
            while (!ops.empty() && ops.back() != '(') {
                postfix += ops.back(); ops.pop_back();
            }
            if (!ops.empty()) ops.pop_back();
        } else {
            while (!ops.empty() && ops.back() != '(') {
                int top_p = get_precedence(ops.back());
                int cur_p = get_precedence(c);
                if (c == '!' ? top_p > cur_p : top_p >= cur_p) {
                    postfix += ops.back(); ops.pop_back();
                } else break;
            }
            ops.push_back(c);
        }
    }
    while (!ops.empty()) { postfix += ops.back(); ops.pop_back(); }
    return postfix;
}

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string input_expr;
    if (!(std::cin >> input_expr)) return 0;

    std::vector<char> vars;
    for (char c : input_expr) if (std::islower(c)) vars.push_back(c);
    std::sort(vars.begin(), vars.end());
    vars.erase(std::unique(vars.begin(), vars.end()), vars.end());

    std::string postfix = infix_to_postfix(input_expr);
    const size_t n = vars.size();
    const uint64_t total_rows = 1ULL << n;

    alignas(64) __m512i precomputed_patterns[MAX_VARS];
    const __m512i ALL_ONES = _mm512_set1_epi64(-1LL);

    for (int v = 0; v < n; ++v) {
        uint64_t k = n - 1 - v;
        if (k < 9) {
            alignas(64) uint64_t raw[8] = {0};
            for (int bit = 0; bit < 512; ++bit) {
                if ((bit >> k) & 1) raw[bit >> 6] |= (1ULL << (bit & 63));
            }
            precomputed_patterns[v] = _mm512_load_si512(raw);
        }
    }

    std::vector<uint8_t> results(total_rows);

    #pragma omp parallel for schedule(dynamic)
    for (uint64_t i = 0; i < total_rows; i += BLOCK_SIZE) {
        __m512i values[MAX_VARS];
        
        for (int v = 0; v < n; ++v) {
            uint64_t k = n - 1 - v;
            if (k >= 9) {
                values[v] = ((i >> k) & 1) ? ALL_ONES : _mm512_setzero_si512();
            } else {
                values[v] = precomputed_patterns[v];
            }
        }

        __m512i eval_stack[64]; 
        int top = -1;

        for (char c : postfix) {
            if (std::islower(c)) {
                int idx = std::lower_bound(vars.begin(), vars.end(), c) - vars.begin();
                eval_stack[++top] = values[idx];
            } else if (c == '!') {
                eval_stack[top] = _mm512_xor_si512(eval_stack[top], ALL_ONES);
            } else {
                __m512i right = eval_stack[top--];
                __m512i left = eval_stack[top];
                if (c == '&')      eval_stack[top] = _mm512_and_si512(left, right);
                else if (c == '|') eval_stack[top] = _mm512_or_si512(left, right);
                else if (c == '>') eval_stack[top] = _mm512_or_si512(_mm512_andnot_si512(left, ALL_ONES), right);
                else if (c == '-') eval_stack[top] = _mm512_xor_si512(_mm512_xor_si512(left, right), ALL_ONES);
            }
        }

        alignas(64) uint64_t out_raw[8];
        _mm512_store_si512(out_raw, eval_stack[0]);
        for (int j = 0; j < BLOCK_SIZE && (i + j) < total_rows; ++j) {
            results[i + j] = (out_raw[j >> 6] >> (j & 63)) & 1ULL;
        }
    }

    #ifndef NDEBUG
    for (uint8_t r : results) std::cout << (int)r;
    std::cout << "\n";
    #endif

    return 0;
}
