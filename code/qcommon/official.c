#include "q_shared.h"
#include "qcommon.h"

typedef struct {
    unsigned int h[8];
    byte block[64];
    int used;
    uint64_t total;
} officialSha_t;

static const unsigned int officialK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static unsigned int Official_Rot(unsigned int x, int n) {
    return (x >> n) | (x << (32 - n));
}

static void Official_ShaBlock(officialSha_t* s, const byte* p) {
    unsigned int w[64];
    unsigned int v[8];
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = ((unsigned int)p[i * 4] << 24) | ((unsigned int)p[i * 4 + 1] << 16) |
               ((unsigned int)p[i * 4 + 2] << 8) | (unsigned int)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        unsigned int a = Official_Rot(w[i - 15], 7) ^ Official_Rot(w[i - 15], 18) ^ (w[i - 15] >> 3);
        unsigned int b = Official_Rot(w[i - 2], 17) ^ Official_Rot(w[i - 2], 19) ^ (w[i - 2] >> 10);

        w[i] = w[i - 16] + a + w[i - 7] + b;
    }
    for (i = 0; i < 8; i++) {
        v[i] = s->h[i];
    }
    for (i = 0; i < 64; i++) {
        unsigned int e = Official_Rot(v[4], 6) ^ Official_Rot(v[4], 11) ^ Official_Rot(v[4], 25);
        unsigned int t1 = v[7] + e + ((v[4] & v[5]) ^ (~v[4] & v[6])) + officialK[i] + w[i];
        unsigned int a = Official_Rot(v[0], 2) ^ Official_Rot(v[0], 13) ^ Official_Rot(v[0], 22);
        unsigned int t2 = a + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));

        v[7] = v[6];
        v[6] = v[5];
        v[5] = v[4];
        v[4] = v[3] + t1;
        v[3] = v[2];
        v[2] = v[1];
        v[1] = v[0];
        v[0] = t1 + t2;
    }
    for (i = 0; i < 8; i++) {
        s->h[i] += v[i];
    }
}

static void Official_ShaInit(officialSha_t* s) {
    static const unsigned int start[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    Com_Memcpy(s->h, start, sizeof(start));
    s->used = 0;
    s->total = 0;
}

static void Official_ShaUpdate(officialSha_t* s, const void* data, int len) {
    const byte* p = (const byte*)data;

    s->total += (uint64_t)len;
    while (len > 0) {
        int n = 64 - s->used;

        if (n > len) {
            n = len;
        }
        Com_Memcpy(s->block + s->used, p, n);
        s->used += n;
        p += n;
        len -= n;
        if (s->used == 64) {
            Official_ShaBlock(s, s->block);
            s->used = 0;
        }
    }
}

static void Official_ShaFinal(officialSha_t* s, byte* out) {
    uint64_t bits = s->total * 8;
    byte pad = 0x80;
    byte len[8];
    int i;

    Official_ShaUpdate(s, &pad, 1);
    pad = 0;
    while (s->used != 56) {
        Official_ShaUpdate(s, &pad, 1);
    }
    for (i = 0; i < 8; i++) {
        len[i] = (byte)(bits >> (56 - i * 8));
    }
    Official_ShaUpdate(s, len, 8);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (byte)(s->h[i] >> 24);
        out[i * 4 + 1] = (byte)(s->h[i] >> 16);
        out[i * 4 + 2] = (byte)(s->h[i] >> 8);
        out[i * 4 + 3] = (byte)s->h[i];
    }
}

static void Official_Hmac(const char* key, const char* text, byte* out) {
    officialSha_t s;
    byte k[64];
    byte pad[64];
    byte inner[32];
    int len = (int)strlen(key);
    int i;

    Com_Memset(k, 0, sizeof(k));
    if (len > 64) {
        Official_ShaInit(&s);
        Official_ShaUpdate(&s, key, len);
        Official_ShaFinal(&s, k);
    } else {
        Com_Memcpy(k, key, len);
    }
    for (i = 0; i < 64; i++) {
        pad[i] = k[i] ^ 0x36;
    }
    Official_ShaInit(&s);
    Official_ShaUpdate(&s, pad, 64);
    Official_ShaUpdate(&s, text, (int)strlen(text));
    Official_ShaFinal(&s, inner);
    for (i = 0; i < 64; i++) {
        pad[i] = k[i] ^ 0x5c;
    }
    Official_ShaInit(&s);
    Official_ShaUpdate(&s, pad, 64);
    Official_ShaUpdate(&s, inner, 32);
    Official_ShaFinal(&s, out);
}

static void Official_Hex(const byte* in, int bytes, char* out, int size) {
    static const char digits[] = "0123456789abcdef";
    int i;

    for (i = 0; i < bytes && i * 2 + 2 < size; i++) {
        out[i * 2] = digits[in[i] >> 4];
        out[i * 2 + 1] = digits[in[i] & 15];
    }
    out[i * 2] = '\0';
}

void Com_OfficialKey(const char* master, const char* version, char* out, int size) {
    byte mac[32];

    Official_Hmac(master, version, mac);
    Official_Hex(mac, 32, out, size);
}

void Com_OfficialProof(const char* key, int challenge, uint64_t steamId, char* out, int size) {
    byte mac[32];

    Official_Hmac(key, va("%i %llu", challenge, (unsigned long long)steamId), mac);
    Official_Hex(mac, OFFICIAL_PROOF_BYTES, out, size);
}
