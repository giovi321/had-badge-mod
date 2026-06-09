/* AES primitive validated against the FIPS-197 Appendix C known-answer vectors. */
#include "test_util.h"
#include "mesh/aes.h"

void run_aes(void)
{
    SUITE("aes/fips-197");
    uint8_t key[32], in[16], out[16], exp[16];
    aes_ctx_t ctx;

    /* FIPS-197 C.1 (AES-128) */
    ht_hex("000102030405060708090a0b0c0d0e0f", key, 32);
    ht_hex("00112233445566778899aabbccddeeff", in, 16);
    ht_hex("69c4e0d86a7b0430d8cdb78070b4c55a", exp, 16);
    CHECK_EQI(aes_setkey_enc(&ctx, key, 128), 0);
    aes_encrypt_block(&ctx, in, out);
    CHECK_MEM(out, exp, 16);

    /* FIPS-197 C.3 (AES-256) */
    ht_hex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", key, 32);
    ht_hex("00112233445566778899aabbccddeeff", in, 16);
    ht_hex("8ea2b7ca516745bfeafc49904b496089", exp, 16);
    CHECK_EQI(aes_setkey_enc(&ctx, key, 256), 0);
    aes_encrypt_block(&ctx, in, out);
    CHECK_MEM(out, exp, 16);

    /* Unsupported key length is rejected. */
    CHECK_EQI(aes_setkey_enc(&ctx, key, 192), -1);
}
