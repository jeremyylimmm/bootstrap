#pragma once

#include "core.h"

typedef struct {
    uint64_t low;
    uint64_t high;
} int128_t;

inline int128_t int128_from_uint64(uint64_t value) {
    return (int128_t) {
        .low = value,
        .high = 0
    };
}

inline int128_t int128_from_int64(int64_t value) {
    return (int128_t) {
        .low = value,
        .high = (value >> 63) ? 0xffffffffffffffff : 0
    };
}

inline int128_t int128_zero() {
    return (int128_t) {0};
}

inline bool int128_equal(int128_t left, int128_t right) {
    return left.low == right.low && left.high == right.high;
}

inline int128_t int128_add(int128_t left, int128_t right) {
    uint64_t low = left.low + right.low;
    int carry = low < left.low;
    uint64_t high = left.high + right.high + carry;
    return (int128_t) {
        .low = low,
        .high = high
    };
}

inline int128_t int128_bitwise_not(int128_t x) {
    return (int128_t) {
        .low = ~x.low,
        .high = ~x.high,
    };
}

inline int128_t int128_bitwise_or(int128_t left, int128_t right) {
    return (int128_t) {
        .low = left.low | right.low,
        .high = left.high | right.high,
    };
}

inline int128_t int128_bitwise_and(int128_t left, int128_t right) {
    return (int128_t) {
        .low = left.low & right.low,
        .high = left.high & right.high,
    };
}

inline int128_t int128_negate(int128_t x) {
    return int128_add(int128_bitwise_not(x), int128_from_uint64(1));
}

inline int128_t int128_sub(int128_t left, int128_t right) {
    return int128_add(left, int128_negate(right));
}

inline bool int128_negative(int128_t x) {
    return x.high >> 63;
}

inline bool int128_positive(int128_t x) { // Includes zero
    return !int128_negative(x);
}

inline bool int128_greater(int128_t left, int128_t right) {
    return int128_negative(int128_sub(right, left));
}

inline bool int128_greater_equal(int128_t left, int128_t right) {
    return int128_positive(int128_sub(left, right));
}

inline bool int128_less(int128_t left, int128_t right) {
    return int128_negative(int128_sub(left, right));
}

inline bool int128_less_equal(int128_t left, int128_t right) {
    return int128_positive(int128_sub(right, left));
}

inline uint64_t uint64_safe_shr(uint64_t x, int amount) {
    if (amount < 0) {
        amount = -amount;
        return amount > 63 ? 0 : x << amount;
    }
    else {
        return amount > 63 ? 0 : x >> amount;
    }
}

inline uint64_t uint64_safe_shl(uint64_t x, int amount) {
    return uint64_safe_shr(x, -amount);
}

inline int128_t int128_shl(int128_t x, int amount);
inline int128_t int128_shr(int128_t x, int amount);

inline int128_t int128_shl(int128_t x, int amount) {
    if (amount < 0) {
        return int128_shr(x, -amount);
    }

    uint64_t low = uint64_safe_shl(x.low, amount);
    uint64_t carry = uint64_safe_shr(x.low, 64-amount);
    uint64_t high = uint64_safe_shl(x.high, amount) | carry;

    return (int128_t) {
        .low = low,
        .high = high
    };
}

inline int128_t int128_shr(int128_t x, int amount) {
    if (amount < 0) {
        return int128_shl(x, -amount);
    }

    uint64_t high = uint64_safe_shr(x.high, amount);
    uint64_t carry = uint64_safe_shl(x.high, 64-amount);
    uint64_t low = uint64_safe_shr(x.low, amount) | carry;

    return (int128_t) {
        .low = low,
        .high = high
    };
}

inline int128_t int128_mul(int128_t left, int128_t right) {
    int128_t product = int128_zero();

    for (int i = 0; i < 128; ++i) {
        if (left.low & 1) {
            product = int128_add(product, right);
        }

        left = int128_shr(left, 1);
        right = int128_shl(right, 1);
    }

    return product;
}

typedef struct {
    int128_t quotient;
    int128_t remainder;
} Int128DivResult;

inline Int128DivResult int128_div(int128_t dividend, int128_t divisor) {
    int128_t quotient = int128_zero();

    for (int i = 0; i < 128; ++i) {
        int128_t cur = int128_shr(dividend, 127-i);

        if(!int128_greater_equal(cur, divisor)) {
            continue;
        }

        int128_t bottom = int128_shl(divisor, 127-i);
        dividend = int128_sub(dividend, bottom);

        int128_t set = int128_shl(int128_from_uint64(1), 127-i);
        quotient = int128_bitwise_or(quotient, set);
    }

    return (Int128DivResult) {
        .quotient = quotient,
        .remainder = dividend
    };
}