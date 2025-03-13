#define UNIT_TEST_AEGIS
#include <stdio.h>
#include <cassert>
#include <limits>

#include "kevlar.h"

using namespace kevlar;

enc_uint64_t
isqrt(enc_uint64_t n)
{
  // Initial guess: start with n.
  enc_uint64_t x = n;
  // Next guess using integer division.
  enc_uint64_t y = (x + n / x) / 2;
    
  // Iterate until the guess stops improving.
  while (y < x)
  {
    x = y;
    y = (x + n / x) / 2;
  }
  return x;
}


enc_uint64_t
do_isqrt(enc_uint64_t n)
{
  // Initial guess: start with n.
  enc_uint64_t x = n;
  // Next guess using integer division.
  enc_uint64_t y = (x + n / x) / 2;
    
  // Iterate until the guess stops improving.
  for (unsigned i=0; i<64; i++)
  {
    bool done = kevlar::cmovLT(y, x, false, true);
    x = kevlar::cmov(!done, y, x);
    y = kevlar::cmov(!done, (x + n / x) / 2, y);
  }
  return x;
}

int
main(void)
{
  printf("Testing type: uint64_t\n");

  for (unsigned iter=0; iter < 1; iter++)
  {
    uint64_t a_val, b_val, mult_val;

    enc_uint64_t x = 18;
    printf("Testing 'x'...\n");
    if (x)
      printf("'x' is non-zero...\n");
    else
      printf("'x' is zero...\n");

    // For 16, 32, 64-bit types we can use slightly larger values.
    a_val = 10;
    b_val = 20;
    mult_val = 3;

    // Default constructor.
    enc_uint64_t a;
    assert(a.getValue() == 0);
    a.printState("a");

    // Default constructor.
    enc_uint64_t aprime;
    aprime.printState("aprime");

    // Value constructor.
    enc_uint64_t b(a_val);
    assert(b.getValue() == a_val);
    b.printState("b");

    enc_uint64_t c(a_val * 2);
    assert(c.getValue() == a_val * 2);
    c.printState("c");

    // Copy constructor.
    enc_uint64_t d = b;
    assert(d.getValue() == b.getValue());
    d.printState("d");

    // Arithmetic operators.
    enc_uint64_t e = b + c;  // a_val + 2*a_val = 3*a_val.
    assert(e.getValue() == a_val + a_val * 2);
    e.printState("e");

    enc_uint64_t f = c - b;  // 2*a_val - a_val = a_val.
    assert(f.getValue() == a_val);
    f.printState("f");

    enc_uint64_t g = b * c;  // a_val * (2*a_val)
    assert(g.getValue() == a_val * (2 * a_val));
    g.printState("g");

    // Avoid division by zero.
    if (b.getValue() != 0)
    {
      enc_uint64_t h = c / b;  // (2*a_val) / a_val = 2
      assert(h.getValue() == 2);
      h.printState("h");
 
      enc_uint64_t i = c % b;  // (2*a_val) % a_val = 0
      assert(i.getValue() == 0);
      i.printState("i");
    }

    // Compound assignment.
    enc_uint64_t j(mult_val);
    j += b; // mult_val + a_val
    // inject a BIT ERROR
    printf("HACK: Flipping bit 8 of protected variable `j'\n");
    j.flipBits(0x0000000000000000ULL, 0x0000000000000100ULL);
    assert(j.getValue() == mult_val + a_val);
    j.printState("j");
  }
  printf("All tests passed for uint64_t.\n");
  // printf("All tests for all supported types passed.\n");


  // perform an integer square root
  kevlar::resetWarnings();
  enc_uint64_t n = 975461057789971041ULL;
  printf("INFO: Running data-heuristic ISQRT() algorithm...\n");
  printf("INFO: The integer square root of '%lu' is '%lu',\n", n.printValue(), isqrt(n).printValue());

  kevlar::resetWarnings();
  n = 975461057789971041ULL;
  printf("INFO: Running data-oblivious ISQRT() algorithm...\n");
  printf("INFO: The integer square root of '%lu' is '%lu',\n", n.printValue(), do_isqrt(n).printValue());

  register __m128i g_xmm4  asm("xmm4");
  register __m128i g_xmm5  asm("xmm5");
  register __m128i g_xmm6  asm("xmm6");
  register __m128i g_xmm7  asm("xmm7");
  register __m128i g_xmm8  asm("xmm8");
  register __m128i g_xmm9  asm("xmm9");
  register __m128i g_xmm10 asm("xmm10");
  register __m128i g_xmm11 asm("xmm11");
  register __m128i g_xmm12 asm("xmm12");
  register __m128i g_xmm13 asm("xmm13");
  register __m128i g_xmm14 asm("xmm14");
  register __m128i g_xmm15 asm("xmm15");

  printf("\nNOTE: Final XMM register state is below...\n");
  print_m128i("xmm4", g_xmm4);
  print_m128i("xmm5", g_xmm5);
  print_m128i("xmm6", g_xmm6);
  print_m128i("xmm7", g_xmm7);
  print_m128i("xmm8", g_xmm8);
  print_m128i("xmm9", g_xmm9);
  print_m128i("xmm10", g_xmm10);
  print_m128i("xmm11", g_xmm11);
  print_m128i("xmm12", g_xmm12);
  print_m128i("xmm13", g_xmm13);
  print_m128i("xmm14", g_xmm14);
  print_m128i("xmm15", g_xmm15);

  return 0;
}

