#pragma GCC optimize("O3,unroll-loops,inline-functions")
#include <openssl/aes.h>
#include <stdint.h>
#include <string.h>

#define AES_MAXNR 14
#define BLOCK_SIZE 16

/* ========= Same as OpenSSL AES_KEY_Custom ========= */
typedef struct {
    unsigned int rd_key[4 * (AES_MAXNR + 1)];
    int rounds;
} AES_KEY_Custom;

// 64-bit parallel xtime (multiplies 8 independent GF(2^8) bytes by 2)
static __attribute__((always_inline)) uint64_t xtime64(uint64_t a) {
    uint64_t mask = (a & 0x8080808080808080ULL) >> 7;
    /*
     * Places 0x1b exactly where MSB was 1, without any branching
     */
    mask = mask * 0x1bULL;
    return ((a & 0x7F7F7F7F7F7F7F7FULL) << 1) ^ mask;
}

// 64-bit parallel GF multiplier (multiplies 8 independent pairs of bytes)
static __attribute__((always_inline)) uint64_t gf_mul64(uint64_t a, uint64_t b) {
    uint64_t r = 0, m;
    #pragma GCC unroll 8
    for (int i = 0; i < 8; i++) {
        m = (b & 0x0101010101010101ULL) * 255ULL;
        r ^= (a & m);
        a = xtime64(a);
        b >>= 1;
    }
    return r;
}

/*
 * sbox64: AES S-Box for 8 bytes simultaneously
 *
 * S(a) = affine(a^(-1)) XOR 0x63
 * a^(-1) = a^254 via Fermat's little theorem
 */
static __attribute__((always_inline)) uint64_t sbox64(uint64_t a) {
    uint64_t p2   = gf_mul64(a, a);
    uint64_t p3   = gf_mul64(a, p2);
    uint64_t p6   = gf_mul64(p3, p3);
    uint64_t p12  = gf_mul64(p6, p6);
    uint64_t p15  = gf_mul64(p3, p12);
    uint64_t p30  = gf_mul64(p15, p15);
    uint64_t p60  = gf_mul64(p30, p30);
    uint64_t p63  = gf_mul64(p3, p60);
    uint64_t p126 = gf_mul64(p63, p63);
    uint64_t p252 = gf_mul64(p126, p126);
    uint64_t x    = gf_mul64(p2, p252); // x is now the inverse i.e. a^254

    // Affine transform applied to 8 bytes simultaneously
    uint64_t r1 = ((x << 1) & 0xFEFEFEFEFEFEFEFEULL) | ((x >> 7) & 0x0101010101010101ULL);
    uint64_t r2 = ((x << 2) & 0xFCFCFCFCFCFCFCFCULL) | ((x >> 6) & 0x0303030303030303ULL);
    uint64_t r3 = ((x << 3) & 0xF8F8F8F8F8F8F8F8ULL) | ((x >> 5) & 0x0707070707070707ULL);
    uint64_t r4 = ((x << 4) & 0xF0F0F0F0F0F0F0F0ULL) | ((x >> 4) & 0x0F0F0F0F0F0F0F0FULL);

    return x ^ r1 ^ r2 ^ r3 ^ r4 ^ 0x6363636363636363ULL;
}


static __attribute__((always_inline)) inline uint32_t rotr32(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

static __attribute__((always_inline)) void ShiftRows(uint8_t state[4][4]) {
    uint32_t r1, r2, r3;

    memcpy(&r1, state[1], 4);
    memcpy(&r2, state[2], 4);
    memcpy(&r3, state[3], 4);

    r1 = rotr32(r1, 8);
    r2 = rotr32(r2, 16);
    r3 = rotr32(r3, 24);

    memcpy(state[1], &r1, 4);
    memcpy(state[2], &r2, 4);
    memcpy(state[3], &r3, 4);
}

// 32-bit parallel xtime for MixColumns
static __attribute__((always_inline)) uint32_t xtime32(uint32_t a) {
    uint32_t mask = (a & 0x80808080) >> 7;
    mask = mask * 0x1b;
    return ((a & 0x7F7F7F7F) << 1) ^ mask;
}

// Mixes all 4 columns independently using 32-bit math
static __attribute__((always_inline)) void MixColumnsSWAR(uint8_t state[4][4]) {
    uint32_t r0, r1, r2, r3;
    memcpy(&r0, &state[0][0], 4);
    memcpy(&r1, &state[1][0], 4);
    memcpy(&r2, &state[2][0], 4);
    memcpy(&r3, &state[3][0], 4);

    uint32_t Tmp = r0 ^ r1 ^ r2 ^ r3;
    uint32_t t01 = xtime32(r0 ^ r1);
    uint32_t t12 = xtime32(r1 ^ r2);
    uint32_t t23 = xtime32(r2 ^ r3);
    uint32_t t30 = xtime32(r3 ^ r0);

    r0 = r0 ^ t01 ^ Tmp;
    r1 = r1 ^ t12 ^ Tmp;
    r2 = r2 ^ t23 ^ Tmp;
    r3 = r3 ^ t30 ^ Tmp;

    memcpy(&state[0][0], &r0, 4);
    memcpy(&state[1][0], &r1, 4);
    memcpy(&state[2][0], &r2, 4);
    memcpy(&state[3][0], &r3, 4);
}

static __attribute__((always_inline))
void AddRoundKey(uint8_t state[4][4], const unsigned int *rd_key, int round) {
    #pragma GCC unroll 4
    for (int col = 0; col < 4; col++) {
        uint32_t w = rd_key[round * 4 + col];
        state[0][col] ^= (uint8_t)(w      ); // LSB goes to Row 0
        state[1][col] ^= (uint8_t)(w >>  8);
        state[2][col] ^= (uint8_t)(w >> 16);
        state[3][col] ^= (uint8_t)(w >> 24); // MSB goes to Row 3
    }
}

/* ========= AES Encryption ========= */
void AES_encrypt_custom(const unsigned char *plaintext,
                        unsigned char *ciphertext,
                        const AES_KEY_Custom *enc_key) {
    uint8_t state[4][4];

    #pragma GCC unroll 4
    for (int col = 0; col < 4; col++){
      state[0][col] = plaintext[col * 4 + 0];
      state[1][col] = plaintext[col * 4 + 1];
      state[2][col] = plaintext[col * 4 + 2];
      state[3][col] = plaintext[col * 4 + 3];
    }

    AddRoundKey(state, enc_key->rd_key, 0);

    #pragma GCC unroll 14
    for (int round = 1; round <= 9; round++) {
        uint64_t st[2];
        memcpy(st, state, 16);
        st[0] = sbox64(st[0]);
        st[1] = sbox64(st[1]);
        memcpy(state, st, 16);

        ShiftRows(state);
        MixColumnsSWAR(state);
        AddRoundKey(state, enc_key->rd_key, round);
    }

    // Final Round
    uint64_t st[2];
    memcpy(st, state, 16);
    st[0] = sbox64(st[0]);
    st[1] = sbox64(st[1]);
    memcpy(state, st, 16);

    ShiftRows(state);
    AddRoundKey(state, enc_key->rd_key, 10);

    #pragma GCC unroll 4
    for (int col = 0; col < 4; col++) {
      ciphertext[col * 4 + 0] = state[0][col];
      ciphertext[col * 4 + 1] = state[1][col];
      ciphertext[col * 4 + 2] = state[2][col];
      ciphertext[col * 4 + 3] = state[3][col];
    }
}

/* ========= Wrapper ========= */

void AES_code(unsigned char plaintext[16],
              unsigned char ciphertext[16],
              AES_KEY *enc_key)
{

     AES_encrypt_custom(plaintext, ciphertext, (AES_KEY_Custom *)enc_key); //Your AES code where there is no secret dependent memory access
   // AES_encrypt(plaintext, ciphertext, enc_key); //OpenSSL AES where there is secret dependent memory access
}
