// Copyright (c) 2026 Yan-Jie Wang
// SPDX-License-Identifier: Unicode-3.0
// Optional ICU 78.3 dependency patch; not a public ICU interface.

#ifndef NUMBER_PLURALOPERAND_H
#define NUMBER_PLURALOPERAND_H

#include <algorithm>
#include "number_decimalquantity.h"

#if !UCONFIG_NO_FORMATTING
U_NAMESPACE_BEGIN
namespace number::impl {

/**
 * Exact arithmetic needed by ICU's parsed integer plural rule bounds.
 *
 * For n, an exact integer floor and a nonzero fractional residue suffice:
 * all relation endpoints and modulus operands in ICU's parser are int32.
 * i, f, and t are integers. The BCD digits remain owned by DecimalQuantity.
 * No formatted value is narrowed to binary64 or truncated before modulo.
 */
struct ExactPluralOperand {
    // Values above all possible parsed bounds compare identically. This
    // sentinel is used only without modulo, after reading the exact digits.
    static constexpr uint64_t beyondRuleBounds = uint64_t(INT32_MAX) + 1;
    uint64_t integer = 0;
    bool fraction = false;

    ExactPluralOperand(const DecimalQuantity& quantity, PluralOperand operand,
                       int32_t modulus) {
        U_ASSERT(!quantity.isApproximate);
        U_ASSERT(!quantity.isNaN() && !quantity.isInfinite());
        U_ASSERT(modulus >= 0);

        // BCD position p has its original numerical magnitude at p+shift.
        // Use int64 for magnitude arithmetic, including suppressed exponents.
        const int64_t shift = int64_t(quantity.scale) + quantity.exponent;
        int64_t low = 0;
        int64_t high = int64_t(quantity.precision) - 1;
        int64_t trailingZeros = 0;

        if (operand == PLURAL_OPERAND_N || operand == PLURAL_OPERAND_I) {
            low = std::max<int64_t>(0, -shift);
            trailingZeros = std::max<int64_t>(0, shift);
            // compact() guarantees a nonzero least significant BCD digit.
            fraction = operand == PLURAL_OPERAND_N &&
                       quantity.precision != 0 && shift < 0;
        } else {
            U_ASSERT(operand == PLURAL_OPERAND_F || operand == PLURAL_OPERAND_T);
            // Discard integer digits, and ignore leading fractional zeroes.
            high = std::min<int64_t>(high, -shift - 1);
            if (operand == PLURAL_OPERAND_F) {
                const int64_t visible = std::max<int64_t>(0,
                    -std::min<int64_t>(quantity.scale, quantity.rReqPos) -
                    quantity.exponent);
                trailingZeros = std::max<int64_t>(0, shift + visible);
            }
        }

        if (high < low) {
            return;
        }
        for (int64_t position = high; position >= low; --position) {
            integer = integer * 10 +
                quantity.getDigitPos(static_cast<int32_t>(position));
            if (modulus != 0) {
                integer %= static_cast<uint32_t>(modulus);
            } else if (integer >= beyondRuleBounds) {
                integer = beyondRuleBounds;
                return;
            }
        }

        if (modulus != 0) {
            // Exponent notation can imply billions of zeroes. Evaluate their
            // contribution in logarithmic time. All products fit in uint64.
            uint64_t power = 10 % static_cast<uint32_t>(modulus);
            while (trailingZeros != 0) {
                if ((trailingZeros & 1) != 0) {
                    integer = (integer * power) % static_cast<uint32_t>(modulus);
                }
                power = (power * power) % static_cast<uint32_t>(modulus);
                trailingZeros >>= 1;
            }
        } else {
            while (trailingZeros != 0 && integer != 0 &&
                   integer < beyondRuleBounds) {
                integer = std::min<uint64_t>(integer * 10, beyondRuleBounds);
                --trailingZeros;
            }
        }
    }

    bool inRange(int32_t low, int32_t high) const {
        // A nonzero fraction lies strictly between integer and integer+1.
        return int64_t(low) <= int64_t(integer) &&
            (int64_t(integer) < int64_t(high) ||
             (int64_t(integer) == int64_t(high) && !fraction));
    }
};

} // namespace number::impl
U_NAMESPACE_END
#endif // !UCONFIG_NO_FORMATTING
#endif // NUMBER_PLURALOPERAND_H
