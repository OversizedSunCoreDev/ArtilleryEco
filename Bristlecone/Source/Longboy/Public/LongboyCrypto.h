#pragma once

#include "CoreMinimal.h"

// Wanted to use OpenSSL RC5 BUT:
// - longboy server uses 16-bit and 32-bit word sizes, but OpenSSL's RC5 implementation only supports 32-bit and 64-bit word sizes.
// - longboy server rounds are not what is supported by OpenSSL's RC5 implementation (longboy server uses 20)

// quite literaly a rewrite of the RC5 rust code because I was tired - https://github.com/RustCrypto/block-ciphers/blob/master/rc5/src/lib.rs
#include <array>
#include <cstdint>
#include <type_traits>
#include <algorithm>
#include <cstring>

// Helper: Rotate left/right for unsigned types
template<typename T>
constexpr T rotl(T x, unsigned n) {
    constexpr unsigned mask = sizeof(T) * 8 - 1;
    n &= mask;
    return (x << n) | (x >> ((sizeof(T) * 8) - n));
}
template<typename T>
constexpr T rotr(T x, unsigned n) {
    constexpr unsigned mask = sizeof(T) * 8 - 1;
    n &= mask;
    return (x >> n) | (x << ((sizeof(T) * 8) - n));
}

// RC5 core template
template<typename Word, size_t Rounds, size_t KeyBytes>
class RC5
{
public:
    static_assert(std::is_integral_v<Word>, "RC5 Word must be an integral type");
    static_assert(std::is_unsigned_v<Word>, "RC5 Word must be unsigned");
    static_assert((sizeof(Word) == 2) || (sizeof(Word) == 4), "RC5 Word must be 16-bit or 32-bit");

    static constexpr size_t WordBytes = sizeof(Word);
    static constexpr size_t BlockBytes = 2 * WordBytes;
    static constexpr size_t ExpandedKeyTableSize = 2 * (Rounds + 1);

    using Block = std::array<uint8_t, BlockBytes>;
    using Key = std::array<uint8_t, KeyBytes>;
    using ExpandedKeyTable = std::array<Word, ExpandedKeyTableSize>;

    RC5(const Key& key) {
        key_table_ = substitute_key(key);
    }

    void encrypt(Block& block) const {
        using U = std::make_unsigned_t<Word>;
        Word a, b;
        words_from_block(block, a, b);

        a = static_cast<Word>(static_cast<U>(a) + static_cast<U>(key_table_[0]));
        b = static_cast<Word>(static_cast<U>(b) + static_cast<U>(key_table_[1]));

        for (size_t i = 1; i <= Rounds; ++i) {
            a = static_cast<Word>(static_cast<U>(rotl<Word>(a ^ b, b)) + static_cast<U>(key_table_[2 * i]));
            b = static_cast<Word>(static_cast<U>(rotl<Word>(b ^ a, a)) + static_cast<U>(key_table_[2 * i + 1]));
        }

        block_from_words(a, b, block);
    }

    void decrypt(Block& block) const {
        using U = std::make_unsigned_t<Word>;
        Word a, b;
        words_from_block(block, a, b);

        for (size_t i = Rounds; i >= 1; --i) {
            b = static_cast<Word>(static_cast<U>(rotr<Word>(static_cast<Word>(static_cast<U>(b) - static_cast<U>(key_table_[2 * i + 1])), a)) ^ static_cast<U>(a));
            a = static_cast<Word>(static_cast<U>(rotr<Word>(static_cast<Word>(static_cast<U>(a) - static_cast<U>(key_table_[2 * i])), b)) ^ static_cast<U>(b));
        }

        b = static_cast<Word>(static_cast<U>(b) - static_cast<U>(key_table_[1]));
        a = static_cast<Word>(static_cast<U>(a) - static_cast<U>(key_table_[0]));

        block_from_words(a, b, block);
    }

private:
    ExpandedKeyTable key_table_;

    static ExpandedKeyTable substitute_key(const Key& key) {
        auto key_as_words = key_into_words(key);
        auto expanded_key_table = initialize_expanded_key_table();

        mix_in(expanded_key_table, key_as_words);
        return expanded_key_table;
    }

    static std::array<Word, (KeyBytes + WordBytes - 1) / WordBytes> key_into_words(const Key& key) {
        constexpr size_t KeyWords = (KeyBytes + WordBytes - 1) / WordBytes;
        std::array<Word, KeyWords> key_as_words{};
        for (int i = KeyBytes - 1; i >= 0; --i) {
            key_as_words[i / WordBytes] = rotl<Word>(key_as_words[i / WordBytes], 8);
            key_as_words[i / WordBytes] += key[i];
        }
        return key_as_words;
    }

    static ExpandedKeyTable initialize_expanded_key_table() {
        ExpandedKeyTable table{};
        constexpr Word P = sizeof(Word) == 4 ? 0xB7E15163 : 0xB7E1; // 32/16-bit
        constexpr Word Q = sizeof(Word) == 4 ? 0x9E3779B9 : 0x9E37; // 32/16-bit
        table[0] = P;
        for (size_t i = 1; i < ExpandedKeyTableSize; ++i) {
            table[i] = table[i - 1] + Q;
        }
        return table;
    }

    static void mix_in(ExpandedKeyTable& key_table, std::array<Word, (KeyBytes + WordBytes - 1) / WordBytes>& key_as_words) {
        size_t n = std::max(key_table.size(), key_as_words.size());
        Word a = 0, b = 0;
        size_t i = 0, j = 0;
        for (size_t k = 0; k < 3 * n; ++k) {
            key_table[i] = rotl<Word>(key_table[i] + a + b, 3);
            a = key_table[i];
            key_as_words[j] = rotl<Word>(key_as_words[j] + a + b, a + b);
            b = key_as_words[j];
            i = (i + 1) % key_table.size();
            j = (j + 1) % key_as_words.size();
        }
    }

    // Explicit little-endian load — guarantees parity with Rust's from_le_bytes
    static void words_from_block(const Block& block, Word& a, Word& b) {
        a = 0;
        b = 0;
        for (size_t i = 0; i < WordBytes; ++i) {
            a |= static_cast<Word>(block[i]) << (8 * i);
            b |= static_cast<Word>(block[WordBytes + i]) << (8 * i);
        }
    }

    // Explicit little-endian store — guarantees parity with Rust's to_le_bytes
    static void block_from_words(Word a, Word b, Block& block) {
        for (size_t i = 0; i < WordBytes; ++i) {
            block[i] = static_cast<uint8_t>((a >> (8 * i)) & 0xFFu);
            block[WordBytes + i] = static_cast<uint8_t>((b >> (8 * i)) & 0xFFu);
        }
    }
};

/**
* Generically named so implementaiton doesn't matter, as long as it can encrypt and decrypt data using a key.
**/
struct FLongboyCrypto
{
    LONGBOY_API FLongboyCrypto(const uint64& Key);
    LONGBOY_API void EncryptHeader(const uint32* Header, uint32* OutHeader);
    LONGBOY_API void DecryptHeader(const uint32* Header, uint32* OutHeader);
    LONGBOY_API void EncryptBody(const TArray<uint8>& Plaintext, TArray<uint8>& OutCiphertext);
    LONGBOY_API void DecryptBody(const TArray<uint8>& Ciphertext, TArray<uint8>& OutPlaintext);
    LONGBOY_API void EncryptBody(const uint8* Plaintext, uint8* OutCiphertext, SIZE_T NumBytes);
    LONGBOY_API void DecryptBody(const uint8* Ciphertext, uint8* OutPlaintext, SIZE_T NumBytes);

private:
    //RC5 16/20/8
    RC5<uint16_t, 20, 8> HeaderCipher;

	// RC5 32/20/8
	RC5<uint32_t, 20, 8> BodyCipher;
};
