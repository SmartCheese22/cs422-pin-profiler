#include <stdio.h>
#include <string.h>
#include <time.h>
#include <openssl/aes.h>
#include "AES_code.c"

#define ITERATIONS 100000

int main() {
    unsigned char key[16] = {
        0x10,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
    };
    unsigned char pt[16] = {
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
    };
    unsigned char ct[16];
    
    AES_KEY enc_key;
    AES_set_encrypt_key(key, 128, &enc_key);
    
    struct timespec t0, t1;
    double elapsed;

    /* Benchmark OpenSSL */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERATIONS; i++)
        AES_encrypt(pt, ct, &enc_key);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    elapsed = (t1.tv_sec - t0.tv_sec)*1e9 + (t1.tv_nsec - t0.tv_nsec);
    printf("OpenSSL AES:  %.2f ns/block  (%.0f total ns for %d blocks)\n",
           elapsed/ITERATIONS, elapsed, ITERATIONS);

    /* Benchmark our implementation */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERATIONS; i++)
        AES_encrypt_custom(pt, ct, (AES_KEY_Custom*)&enc_key);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    elapsed = (t1.tv_sec - t0.tv_sec)*1e9 + (t1.tv_nsec - t0.tv_nsec);
    printf("Our AES:      %.2f ns/block  (%.0f total ns for %d blocks)\n",
           elapsed/ITERATIONS, elapsed, ITERATIONS);

    return 0;
}
