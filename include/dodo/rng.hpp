#ifndef DODO_RNG_HPP
#define DODO_RNG_HPP

#include <cstdint>

namespace dodo {

// xoshiro256** seeded through SplitMix64. Chosen over std::mt19937 because it is
// noticeably faster, has a tiny state, and is trivial to make reproducible from
// a single 64-bit seed, which is exactly what the hot generation loop needs.
class Rng {
  public:
    explicit Rng(std::uint64_t seed) noexcept { reseed(seed); }

    void reseed(std::uint64_t seed) noexcept {
        SplitMix64 sm{seed};
        state_[0] = sm.next();
        state_[1] = sm.next();
        state_[2] = sm.next();
        state_[3] = sm.next();
    }

    std::uint64_t next_u64() noexcept {
        const std::uint64_t result = rotl(state_[1] * 5, 7) * 9;
        const std::uint64_t t = state_[1] << 17;
        state_[2] ^= state_[0];
        state_[3] ^= state_[1];
        state_[1] ^= state_[2];
        state_[0] ^= state_[3];
        state_[2] ^= t;
        state_[3] = rotl(state_[3], 45);
        return result;
    }

    // Uniform double in [0, 1). Uses the top 53 bits, the full mantissa width.
    double next_double() noexcept {
        return static_cast<double>(next_u64() >> 11) * (1.0 / 9007199254740992.0);
    }

    // Uniform integer in the inclusive range [min, max]. Uses Lemire's
    // multiply-shift to avoid modulo bias while staying branch-light.
    std::int64_t next_int(std::int64_t min, std::int64_t max) noexcept {
        if (max <= min) {
            return min;
        }
        const std::uint64_t span = static_cast<std::uint64_t>(max - min) + 1;
        return min + static_cast<std::int64_t>(bounded(span));
    }

    bool next_bool(double probability_true) noexcept {
        return next_double() < probability_true;
    }

    // Index into a container of the given size; size must be non-zero.
    std::size_t next_index(std::size_t size) noexcept {
        return static_cast<std::size_t>(bounded(static_cast<std::uint64_t>(size)));
    }

  private:
    struct SplitMix64 {
        std::uint64_t state;
        std::uint64_t next() noexcept {
            std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        }
    };

    static std::uint64_t rotl(std::uint64_t x, int k) noexcept {
        return (x << k) | (x >> (64 - k));
    }

    // Unbiased bounded integer in [0, range) via rejection sampling. Portable
    // across compilers (no 128-bit multiply) and free of modulo bias.
    std::uint64_t bounded(std::uint64_t range) noexcept {
        const std::uint64_t threshold = (0u - range) % range;
        std::uint64_t value;
        do {
            value = next_u64();
        } while (value < threshold);
        return value % range;
    }

    std::uint64_t state_[4];
};

} // namespace dodo

#endif // DODO_RNG_HPP
