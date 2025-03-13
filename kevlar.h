#ifndef KEVLAR_H
#define KEVLAR_H

#include <cstdint>
#include <cassert>
#include <random>
#include <wmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
#include <tmmintrin.h>
#include <immintrin.h>  // Required for _rdseed32_step and _rdseed64_step
#include <type_traits>

namespace kevlar {

static bool SC_warning = false;

// macro for access x86 RDSEED instruction
#define _my_rdrand64_step(x) ({ unsigned char err; asm volatile(".byte 0x48; .byte 0x0f; .byte 0xc7; .byte 0xf0; setc %1":"=a"(*x), "=qm"(err)); err; })

// Macro for AES-128 key expansion step. RC must be an immediate constant.
#define AES128_KEY_EXPANSION_STEP(KEY, RC) ({                 \
  __m128i _key = (KEY);                                       \
  __m128i _temp = _mm_aeskeygenassist_si128(_key, (RC));      \
  _temp = _mm_shuffle_epi32(_temp, _MM_SHUFFLE(3,3,3,3));     \
  _key = _mm_xor_si128(_key, _mm_slli_si128(_key, 4));        \
  _key = _mm_xor_si128(_key, _mm_slli_si128(_key, 4));        \
  _key = _mm_xor_si128(_key, _mm_slli_si128(_key, 4));        \
  _mm_xor_si128(_key, _temp);                                 \
})

//
// Generate ephermal key state into the XMM registers, the same table is used
// for encryption and decryption
//
extern "C" void
init_ephemeral_key(void)
{
  __m128i ephemeral_enc_keys[11];
  __m128i ephemeral_key;
  static bool ephemeral_key_initialized = false;

  if (!ephemeral_key_initialized)
  {
    long long unsigned rdrand_value;
    int success;
    while (!(success =  _my_rdrand64_step(&rdrand_value)))
      ;

    std::mt19937 gen((uint64_t)rdrand_value);
    std::uniform_int_distribution<uint32_t> dis;
    ephemeral_key = _mm_set_epi32(dis(gen), dis(gen), dis(gen), dis(gen));

    ephemeral_enc_keys[0] = ephemeral_key;
    ephemeral_enc_keys[1] = AES128_KEY_EXPANSION_STEP(ephemeral_enc_keys[0], 0x01);
    ephemeral_enc_keys[2] = AES128_KEY_EXPANSION_STEP(ephemeral_enc_keys[1], 0x02);
    ephemeral_enc_keys[3] = AES128_KEY_EXPANSION_STEP(ephemeral_enc_keys[2], 0x04);
    ephemeral_enc_keys[4] = AES128_KEY_EXPANSION_STEP(ephemeral_enc_keys[3], 0x08);
    ephemeral_enc_keys[5] = AES128_KEY_EXPANSION_STEP(ephemeral_enc_keys[4], 0x10);
    ephemeral_enc_keys[6] = AES128_KEY_EXPANSION_STEP(ephemeral_enc_keys[5], 0x20);
    ephemeral_enc_keys[7] = AES128_KEY_EXPANSION_STEP(ephemeral_enc_keys[6], 0x40);
    ephemeral_enc_keys[8] = AES128_KEY_EXPANSION_STEP(ephemeral_enc_keys[7], 0x80);
    ephemeral_enc_keys[9] = AES128_KEY_EXPANSION_STEP(ephemeral_enc_keys[8], 0x1B);
    ephemeral_enc_keys[10] = AES128_KEY_EXPANSION_STEP(ephemeral_enc_keys[9], 0x36);

    // Bind the first N keys to XMM registers.
    __asm__ volatile ("movdqa %0, %%xmm5\n" : : "m"(ephemeral_enc_keys[0]) : "memory", "xmm5");
    __asm__ volatile ("movdqa %0, %%xmm6\n" : : "m"(ephemeral_enc_keys[1]) : "memory", "xmm6");
    __asm__ volatile ("movdqa %0, %%xmm7\n" : : "m"(ephemeral_enc_keys[2]) : "memory", "xmm7");
    __asm__ volatile ("movdqa %0, %%xmm8\n" : : "m"(ephemeral_enc_keys[3]) : "memory", "xmm8");
    __asm__ volatile ("movdqa %0, %%xmm9\n" : : "m"(ephemeral_enc_keys[4]) : "memory", "xmm9");
    __asm__ volatile ("movdqa %0, %%xmm10\n" : : "m"(ephemeral_enc_keys[5]) : "memory", "xmm10");
    __asm__ volatile ("movdqa %0, %%xmm11\n" : : "m"(ephemeral_enc_keys[6]) : "memory", "xmm11");

    // xmm13 is the incrementor value
    __m128i salt_inc = _mm_set_epi32(0, 0, 1, 0);
    __asm__ volatile ("movdqa %0, %%xmm13\n" : : "m"(salt_inc) : "memory", "xmm13");

    // xmm14 is the incrementing salt value
    __m128i salt_value = _mm_set_epi32(0, 0, dis(gen), 0);
    __asm__ volatile ("movdqa %0, %%xmm14\n" : : "m"(salt_value) : "memory", "xmm14");
  }
}

//
// XMM-based cipher routines
//
// in: plaintext value
register uint64_t value_reg asm("r14"); // avoid call-clobbered regs
// out: encryption authentication status
register uint64_t auth_reg  asm("r15"); // avoid call-clobbered regs

extern "C"  /* out: ciphertext */__m128i aes128_encrypt(/* in: value_reg  */);
extern "C" /* out: auth_reg, value_reg */ void aes128_decrypt(/* in: ciphertext */__m128i block);

// AES-128 encryption: iterate forward over XMM-resident encrpytion keys
extern "C" /* out:ciphertext */ __m128i
aes128_encrypt(/* in: value_reg */void)
{
  // build the plaintext 128-bit word to be AES-128 encryted in xmm0
  register __m128i block asm("xmm0")
     = _mm_set_epi32(static_cast<int>(value_reg >> 32),
                     static_cast<int>(value_reg & 0xffffffff),
                     0, /* hash */42);

  // mix in the incremented salt value from xmm14
  __asm__ volatile (
   "paddd   %xmm13, %xmm14    \n\t" // salt = salt + 1
   "paddd   %xmm14, %xmm0     \n\t" // mix in the salt
  );

  // perform a reduced-round AES-128 encryption, using x86 AES-NI instructions
  __asm__ volatile (
    "pxor   %%xmm5, %0        \n\t"  // block ^= key0
    "aesenc %%xmm6, %0        \n\t"  // round 1: key1
    "aesenc %%xmm7, %0        \n\t"  // round 2: key2
    "aesenc %%xmm8, %0        \n\t"  // round 3: key3
    "aesenc %%xmm9, %0        \n\t"  // round 4: key4
    "aesenc %%xmm10, %0       \n\t"  // round 5: key5
    "aesenc %%xmm11, %0       \n\t"  // round 6: key6
    // "aesenc %%xmm12, %0    \n\t"  // round 7: key7
    // "aesenc %%xmm13, %0    \n\t"  // round 8: key8
    // "aesenc %%xmm14, %0    \n\t"  // round 9: key9
    "aesenclast %%xmm15, %0   \n\t"  // final round with key10
    : "+x" (block)
    : 
    : 
  );

  // return the ciphertext
  return block;
}

// AES-128 decryption: iterate in reverse order, applying inverse MixColumns on encryption keys
extern "C" /* out: auth_reg, value_reg */ void
aes128_decrypt(/* in: ciphertext */__m128i block)
{
  // perform a reduced-round AES-128 decryption, using x86 AES-NI instructions
  __asm__ volatile (
    "pxor   %%xmm15, %0        \n\t"  // block ^= key10
    // "aesimc %%xmm14, %%xmm4 \n\t"
    // "aesdec %%xmm4, %0      \n\t"  // round 1: inverse of key9
    // "aesimc %%xmm13, %%xmm4 \n\t"
    // "aesdec %%xmm4, %0      \n\t"  // round 2: inverse of key8
    // "aesimc %%xmm12, %%xmm4 \n\t"
    // "aesdec %%xmm4, %0      \n\t"  // round 3: inverse of key7
    "aesimc %%xmm11, %%xmm4    \n\t"
    "aesdec %%xmm4, %0         \n\t"  // round 4: inverse of key6
    "aesimc %%xmm10, %%xmm4    \n\t"
    "aesdec %%xmm4, %0         \n\t"  // round 5: inverse of key5
    "aesimc %%xmm9, %%xmm4     \n\t"
    "aesdec %%xmm4, %0         \n\t"  // round 6: inverse of key4
    "aesimc %%xmm8, %%xmm4     \n\t"
    "aesdec %%xmm4, %0         \n\t"  // round 7: inverse of key3
    "aesimc %%xmm7, %%xmm4     \n\t"
    "aesdec %%xmm4, %0         \n\t"  // round 8: inverse of key2
    "aesimc %%xmm6, %%xmm4     \n\t"
    "aesdec %%xmm4, %0         \n\t"  // round 9: inverse of key1
    "aesdeclast %%xmm5, %0     \n\t"  // final round with key0
    // outputs
    : "+x" (block)
    // inputs
    : 
    // clobbers
    : "xmm4"
  );

  // check the authentication "cookie", set the auth_reg output
  __asm__ volatile (
    "movd %xmm0, %r14d     \n\t" // Move lowest 32-bit lane into r14d
    "cmpq $0x2a, %r14      \n\t" // Compare with 42
    "sete %r15b            \n\t" // Set r15b to 1 if not equal, 0 if equal
    "movzx %r15b, %r15     \n\t" // Zero-extend r15b into r15
  );

  // extract the plaintext value into value_reg
  uint32_t low = _mm_extract_epi32(block, 2);
  uint32_t high = _mm_extract_epi32(block, 3);
  value_reg = (static_cast<uint64_t>(high) << 32) | low;
}

//
// print an XMM register value
//
void
print_m128i(const char *varname, __m128i value)
{
  uint8_t bytes[16];
  _mm_storeu_si128(reinterpret_cast<__m128i*>(bytes), value);

  printf("%6s: ", varname);
  for (int i = 0; i < 16; i++) {
      printf("%02x", bytes[i]);  // Print as hexadecimal
  }
  printf("\n"); // fflush(stdout);
}

void
sideChannelWarning(void)
{
  printf("WARNING: Program behaviors are likely leaking secrets!\n");
  SC_warning = true;
}

// reset all warnings
void resetWarnings(void)
{
  printf("INFO: Resetting leaky behavior detectors.\n");
  SC_warning = false;
}


// --- enc_uint64_t Class ---
//
// enc_uint64_t is an encrypted 64-bit unsigned integer type, the plaintext
// state include: value, salt, and an integrity hash, for a 128-bit data
// value packet, the packet is encrypted with 128-bit reduced round register
// resident AES cipher, every operation operation (construction, assignment,
// cast, arithmetic, etc.) generates a new random salt
//
class enc_uint64_t
{
  // encrypted state stored as a 128-bit block.
  __m128i encrypted_state;

public:
  // Constructors.
  enc_uint64_t()
  {
    value_reg = 0;
    encrypted_state = aes128_encrypt();
  }
  enc_uint64_t(uint64_t v)
  {
    value_reg = v;
    encrypted_state = aes128_encrypt();
  }
  enc_uint64_t(__m128i c)
  {
    encrypted_state = c;
  }

  // print ciphertext state
  void printState(const char *varname) const
  {
    print_m128i(varname, encrypted_state);
  }

  void flipBits(uint64_t hiMask, uint64_t loMask) const
  {
    uint64_t *p = (uint64_t *)&encrypted_state;
    p[0] ^= loMask;
    p[1] ^= hiMask;
  }

  // authentication failure
  void authFailure() const
  {
    printf("ERROR: Decryption authentication failure!\n");

    printf("NOTE: Attempting recovery of corrupted ciphertext...\n");
    unsigned word, bit;
    for (word=0; word < 2; word++)
    {
      uint64_t mask = 1;
      for (bit=0; bit < sizeof(uint64_t)*8; bit++,mask <<= 1)
      {
         // flip bit
         if (word == 0)
           flipBits(0x0ULL, mask);
         else
           flipBits(mask, 0x0ULL);

         // attempt decryption with adjusted ciphertext
         aes128_decrypt(encrypted_state);
         if (auth_reg)
           goto fixed;

         // undo flip bit
         if (word == 0)
           flipBits(0x0ULL, mask);
         else
           flipBits(mask, 0x0ULL);
      }
    }

    // could not fix ciphertext
    printf("NOTE: Ciphertext was not fixed, too many bit flips.\n");
    return;

fixed:
    // ciphertext is fixed
    printf("NOTE: Ciphertext was fixed! (Flipped bit `%u')\n", word*64+bit);
  }

  // Copy constructor: decrypt then re-encrypt with new random salt.
  enc_uint64_t(const enc_uint64_t &other)
  {
    bool auth = true;
    aes128_decrypt(other.encrypted_state);
    auth = auth && auth_reg;
    /* value_reg passes through */
    encrypted_state = aes128_encrypt();
    if (!auth)
      authFailure();
  }

  enc_uint64_t &operator=(const enc_uint64_t &other)
  {
    if (this != &other)
    {
      bool auth = true;
      aes128_decrypt(other.encrypted_state);
      auth = auth && auth_reg;
      /* value_reg passes through */
      encrypted_state = aes128_encrypt();
      if (!auth)
        authFailure();
    }
    return *this;
  }

  // Getters
  uint64_t getValue() const
  {
    bool auth = true;
    aes128_decrypt(encrypted_state);
    auth = auth && auth_reg;
    if (!auth)
      authFailure();
    if (!SC_warning)
      sideChannelWarning();
    return value_reg;
  }

  // DO safe
  uint64_t printValue()
  {
    bool auth = true;
    aes128_decrypt(encrypted_state);
    auth = auth && auth_reg;
    if (!auth)
      authFailure();
    return value_reg;
  }

  // Arithmetic operators
  enc_uint64_t operator+(const enc_uint64_t &other) const
  {
    bool auth = true;
    aes128_decrypt(encrypted_state);
    uint64_t op1 = value_reg;
    auth = auth && auth_reg;
    aes128_decrypt(other.encrypted_state);
    uint64_t op2 = value_reg;
    auth = auth && auth_reg;
    value_reg = op1 + op2;
    __m128i encrypted_state = aes128_encrypt();
    if (!auth)
      authFailure();
    return enc_uint64_t(encrypted_state);
  }

  enc_uint64_t operator-(const enc_uint64_t &other) const
  {
    bool auth = true;
    aes128_decrypt(encrypted_state);
    uint64_t op1 = value_reg;
    auth = auth && auth_reg;
    aes128_decrypt(other.encrypted_state);
    uint64_t op2 = value_reg;
    auth = auth && auth_reg;
    value_reg = op1 - op2;
    __m128i encrypted_state = aes128_encrypt();
    if (!auth)
      authFailure();
    return enc_uint64_t(encrypted_state);
  }

  enc_uint64_t operator*(const enc_uint64_t &other) const
  {
    bool auth = true;
    aes128_decrypt(encrypted_state);
    uint64_t op1 = value_reg;
    auth = auth && auth_reg;
    aes128_decrypt(other.encrypted_state);
    uint64_t op2 = value_reg;
    value_reg = op1 * op2;
    auth = auth && auth_reg;
    __m128i encrypted_state = aes128_encrypt();
    if (!auth)
      authFailure();
    return enc_uint64_t(encrypted_state);
  }

  enc_uint64_t operator/(const enc_uint64_t &other) const
  {
    bool auth = true;
    aes128_decrypt(encrypted_state);
    uint64_t op1 = value_reg;
    auth = auth && auth_reg;
    aes128_decrypt(other.encrypted_state);
    uint64_t op2 = value_reg;
    auth = auth && auth_reg;
    value_reg = op1 / op2;
    __m128i encrypted_state = aes128_encrypt();
    if (!auth)
      authFailure();
    return enc_uint64_t(encrypted_state);
  }

  enc_uint64_t operator%(const enc_uint64_t &other) const
  {
    bool auth = true;
    aes128_decrypt(encrypted_state);
    uint64_t op1 = value_reg;
    auth = auth && auth_reg;
    aes128_decrypt(other.encrypted_state);
    uint64_t op2 = value_reg;
    auth = auth && auth_reg;
    value_reg = op1 % op2;
    __m128i encrypted_state = aes128_encrypt();
    if (!auth)
      authFailure();
    return enc_uint64_t(encrypted_state);
  }

  // relational operators
  bool operator<(const enc_uint64_t &other) const
  {
    bool auth = true;
    aes128_decrypt(encrypted_state);
    uint64_t op1 = value_reg;
    auth = auth && auth_reg;
    aes128_decrypt(other.encrypted_state);
    uint64_t op2 = value_reg;
    auth = auth && auth_reg;
    bool value = op1 < op2;
    if (!SC_warning)
      sideChannelWarning();
    return value;
  }

  // Compound assignment operator example
  enc_uint64_t &operator+=(const enc_uint64_t &other)
  {
    bool auth = true;
    aes128_decrypt(encrypted_state);
    uint64_t op1 = value_reg;
    auth = auth && auth_reg;
    aes128_decrypt(other.encrypted_state);
    uint64_t op2 = value_reg;
    auth = auth && auth_reg;
    value_reg = op1 + op2;
    encrypted_state = aes128_encrypt();
    if (!auth)
      authFailure();
    return *this;
  }

  // Explicit conversion operator to underlying type
  explicit operator uint64_t() const
  {
    if (!SC_warning)
      sideChannelWarning();
    return getValue();
  }

  // Explicit conversion operator to underlying type.
  explicit operator int() const
  {
    if (!SC_warning)
      sideChannelWarning();
    return (int)getValue();
  }

  // Explicit conversion operator to underlying type.
  explicit operator bool() const
  {
    if (!SC_warning)
      sideChannelWarning();
    return (bool)getValue();
  }

};

// CMOV primitive
enc_uint64_t cmov(bool p, enc_uint64_t x, enc_uint64_t y)
{
  bool SC_temp = SC_warning;
  SC_warning = true;
  enc_uint64_t retval = p ? x : y;
  SC_warning = SC_temp;
  return retval;
}

// CMOV primitive
bool cmov(bool p, bool x, bool y)
{
  bool SC_temp = SC_warning;
  SC_warning = true;
  bool retval = p ? x : y;
  SC_warning = SC_temp;
  return retval;
}

// CMOV primitive
bool cmovLT(enc_uint64_t a, enc_uint64_t b, bool x, bool y)
{
  bool SC_temp = SC_warning;
  SC_warning = true;
  bool retval = (a < b) ? x : y;
  SC_warning = SC_temp;
  return retval;
}

} // namespace kevlar

// Static function with constructor attribute
static void __attribute__((constructor)) load_time_init()
{
    // call crypto library initialization function
    kevlar::init_ephemeral_key();
}

#endif // KEVLAR_H

