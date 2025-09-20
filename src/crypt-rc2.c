#include "rc2.h"
#include "dsd.h"

static inline uint64_t rol64(uint64_t x, int n) {
    return ((x << n) | (x >> (63 - n) >> 1)) & 0xffffffffffffffff;
}

void swapbit(uint64_t* internalstate, uint8_t bit) {
    unsigned char bitB = bit & 1;
    if (bitB)
        *internalstate |= bitB;
    else
        *internalstate &= (~bitB ^ 1);
}

// MD2 functions
void md2_init(MD2State* state) {
    state->x1 = 0;
    state->x2 = 0;
    memset(state->h2, 0, n1);
    memset(state->h1, 0, n1 * 3);
}

void md2_hashing(MD2State* state, unsigned char t1[], size_t b6) {
    static unsigned char s4[256] = {
        13,199,11,67,237,193,164,77,115,184,141,222,73,
        38,147,36,150,87,21,104,12,61,156,101,111,145,
        119,22,207,35,198,37,171,167,80,30,219,28,213,
        121,86,29,214,242,6,4,89,162,110,175,19,157,
        3,88,234,94,144,118,159,239,100,17,182,173,238,
        68,16,79,132,54,163,52,9,58,57,55,229,192,
        170,226,56,231,187,158,70,224,233,245,26,47,32,
        44,247,8,251,20,197,185,109,153,204,218,93,178,
        212,137,84,174,24,120,130,149,72,180,181,208,255,
        189,152,18,143,176,60,249,27,227,128,139,243,253,
        59,123,172,108,211,96,138,10,215,42,225,40,81,
        65,90,25,98,126,154,64,124,116,122,5,1,168,
        83,190,131,191,244,240,235,177,155,228,125,66,43,
        201,248,220,129,188,230,62,75,71,78,34,31,216,
        254,136,91,114,106,46,217,196,92,151,209,133,51,
        236,33,252,127,179,69,7,183,105,146,97,39,15,
        205,112,200,166,223,45,48,246,186,41,148,140,107,
        76,85,95,194,142,50,49,134,23,135,169,221,210,
        203,63,165,82,161,202,53,14,206,232,103,102,195,
        117,250,99,0,74,160,241,2,113
    };
   
    int b4 = 0;
    
    while (b6) {
        for (; b6 && state->x2 < n1; b6--, state->x2++) {
            int b5 = t1[b4++];
            state->h1[state->x2 + n1] = b5;
            state->h1[state->x2 + (n1 * 2)] = b5 ^ state->h1[state->x2];
            state->x1 = state->h2[state->x2] ^= s4[b5 ^ state->x1];
        }

        if (state->x2 == n1) {
            int b2 = 0;
            state->x2 = 0;
            
            for (int b3 = 0; b3 < (n1 + 2); b3++) {
                for (int b1 = 0; b1 < (n1 * 3); b1++)
                    b2 = state->h1[b1] ^= s4[b2];
                b2 = (b2 + b3) % 256;
            }
        }
    }
}

void md2_end(MD2State* state, unsigned char h4[n1]) {
    unsigned char h3[n1];
    int n4 = n1 - state->x2;
    for (int i = 0; i < n4; i++) h3[i] = n4;
    md2_hashing(state, h3, n4);
    md2_hashing(state, state->h2, sizeof(state->h2));
    for (int i = 0; i < n1; i++) h4[i] = state->h1[i];
}

// RC4 functions
uint64_t next(RC4State* state) {
    uint64_t z = (state->x += 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

void rc4_init(RC4State* state, unsigned char key[]) {
    int tmp;
    
    for (state->i_rc4 = 0; state->i_rc4 < 256; state->i_rc4++) {
        state->array_rc4[state->i_rc4] = state->i_rc4;
    }
    
    state->j_rc4 = 0;
    for (state->i_rc4 = 0; state->i_rc4 < 256; state->i_rc4++) {
        state->j_rc4 = (state->j_rc4 + state->array_rc4[state->i_rc4] + key[state->i_rc4 % 256]) % 256;
        tmp = state->array_rc4[state->i_rc4];
        state->array_rc4[state->i_rc4] = state->array_rc4[state->j_rc4];
        state->array_rc4[state->j_rc4] = tmp;
    }
    state->i_rc4 = 0;
    state->j_rc4 = 0;
}

unsigned char rc4_output(RC4State* state) {
    uint8_t rndbyte, decal;
    int tmp, t;

    state->i_rc4 = (state->i_rc4 + 1) % 256;
    state->j_rc4 = (state->j_rc4 + state->array_rc4[state->i_rc4]) % 256;
    tmp = state->array_rc4[state->i_rc4];
    state->array_rc4[state->i_rc4] = state->array_rc4[state->j_rc4];
    state->array_rc4[state->j_rc4] = tmp;
    t = (state->array_rc4[state->i_rc4] + state->array_rc4[state->j_rc4]) % 256;

    
        if (state->xyz == 0) state->bb = next(state);
        decal = 56 - (8 * state->xyz);
        rndbyte = (state->bb >> decal) & 0xff;
        state->xyz++;
        if (state->xyz == 8) state->xyz = 0;
        
        if (state->count == 0) {
            rndbyte = rndbyte ^ state->array_rc4[t];
            state->count = 1;
        } else {
            rndbyte = rndbyte + state->array_rc4[t];
            state->count = 0;
        }
   
   
    return rndbyte;
}

/* Convert 64-bit integer to bytes (big-endian) */
static void u64_to_bytes_be(uint64_t val, unsigned char *out) {
    for (int i = 0; i < 8; i++) {
        out[i] = (unsigned char)((val >> (56 - 8 * i)) & 0xFF);
    }
}

// RC2 functions
void rc2_keyschedule(RC2State* state) {
    unsigned i;
    // Phase 3 - copy to xkey in little-endian order
    i = 63;
    do {
        state->xkey[i] = ((unsigned char *)state->xkey)[2 * i] +
                        (((unsigned char *)state->xkey)[2 * i + 1] << 8);
    } while (i--);
}

void rc2_encrypt(RC2State* state) {
    uint16_t x76, x54, x32, x10, i;

    x76 = (state->plain[7] << 8) + state->plain[6];
    x54 = (state->plain[5] << 8) + state->plain[4];
    x32 = (state->plain[3] << 8) + state->plain[2];
    x10 = (state->plain[1] << 8) + state->plain[0];

    for (i = 0; i < 16; i++) {
        x10 += (x32 & ~x76) + (x54 & x76) + state->xkey[4 * i + 0];
        x10 = (x10 << 1) + (x10 >> 15 & 1);
        
        x32 += (x54 & ~x10) + (x76 & x10) + state->xkey[4 * i + 1];
        x32 = (x32 << 2) + (x32 >> 14 & 3);

        x54 += (x76 & ~x32) + (x10 & x32) + state->xkey[4 * i + 2];
        x54 = (x54 << 3) + (x54 >> 13 & 7);

        x76 += (x10 & ~x54) + (x32 & x54) + state->xkey[4 * i + 3];
        x76 = (x76 << 5) + (x76 >> 11 & 31);

        if (i == 4 || i == 10) {
            x10 += state->xkey[x76 & 63];
            x32 += state->xkey[x10 & 63];
            x54 += state->xkey[x32 & 63];
            x76 += state->xkey[x54 & 63];
        }
    }

    state->cipher[0] = (unsigned char)x10;
    state->cipher[1] = (unsigned char)(x10 >> 8);
    state->cipher[2] = (unsigned char)x32;
    state->cipher[3] = (unsigned char)(x32 >> 8);
    state->cipher[4] = (unsigned char)x54;
    state->cipher[5] = (unsigned char)(x54 >> 8);
    state->cipher[6] = (unsigned char)x76;
    state->cipher[7] = (unsigned char)(x76 >> 8);
    
}

// Main cryptographic functions
void create_keys_rc2(CryptoContext* ctx, unsigned char key1[], size_t size1) {
    unsigned char h4[n1];
    
    // Initialize MD2 and hash the key
    md2_init(&ctx->md2);
    md2_hashing(&ctx->md2, key1, size1);
    md2_end(&ctx->md2, h4);
    
    // Copy first 16 bytes to keys
    for (int i = 0; i < 16; i++) ctx->keys[i] = h4[i];
    
    // Initialize RC4 with hashed key
    rc4_init(&ctx->rc4, h4);
    
    // Initialize RC4 state variables
    ctx->rc4.x = 0;
    for (int i = 0; i < 8; i++) ctx->rc4.x = (ctx->rc4.x << 8) + (h4[256 + i] & 0xff);
    
    ctx->rc4.xyz = 0;
    ctx->rc4.count = 0;
    
    // Warm-up RC4
    for (int i = 0; i < 22000; i++) rc4_output(&ctx->rc4);
    
    // Generate RC2 keys
    int k = rc4_output(&ctx->rc4) + 256;
    for (int i = 0; i < k; i++) rc4_output(&ctx->rc4);
    
    for (int i = 0; i < 64; i++) {
        ctx->rc2.xkey[i] = (rc4_output(&ctx->rc4) << 8) + rc4_output(&ctx->rc4);
    }
    
    // Generate internal zero value
    k = rc4_output(&ctx->rc4) + 256;
    for (int i = 0; i < k; i++) rc4_output(&ctx->rc4);
    
    ctx->internal_zero = 0;
    for (int i = 0; i < 8; i++) {
        ctx->internal_zero = (ctx->internal_zero << 8) + rc4_output(&ctx->rc4);
    }
    
    // Generate RC2 key schedule
    rc2_keyschedule(&ctx->rc2);
}

void encryption_rc2(CryptoContext* ctx, uint8_t bits[49]) {
    ctx->internal_state = ctx->internal_zero;

    for (int sso = 0; sso < 49; sso++) {
        // Prepare plaintext from internal state
        ctx->rc2.plain[0] = (ctx->internal_state >> 56) & 0xff;
        ctx->rc2.plain[1] = (ctx->internal_state >> 48) & 0xff;
        ctx->rc2.plain[2] = (ctx->internal_state >> 40) & 0xff;
        ctx->rc2.plain[3] = (ctx->internal_state >> 32) & 0xff;
        ctx->rc2.plain[4] = (ctx->internal_state >> 24) & 0xff;
        ctx->rc2.plain[5] = (ctx->internal_state >> 16) & 0xff;
        ctx->rc2.plain[6] = (ctx->internal_state >> 8) & 0xff;
        ctx->rc2.plain[7] = ctx->internal_state & 0xff;
        
        // Encrypt with RC2
        rc2_encrypt(&ctx->rc2);
        
        // Reconstruct internal state from ciphertext
        ctx->internal_state = 0;
        for (int i = 0; i < 8; i++) {
            ctx->internal_state = (ctx->internal_state << 8) + (ctx->rc2.cipher[i] & 0xff);
        }
                
        // XOR the bit and update internal state
        bits[48 - sso] = bits[48 - sso] ^ (ctx->internal_state & 1);
        ctx->internal_state = rol64(ctx->internal_state, 1);
        swapbit(&ctx->internal_state, bits[48 - sso]);
    }
}

void decrypt_rc2(CryptoContext* ctx, uint8_t bits[49]) {
    uint8_t tempy;
    ctx->internal_state = ctx->internal_zero;

    
    for (int sso = 0; sso < 49; sso++) {
        // Prepare plaintext from internal state
        ctx->rc2.plain[0] = (ctx->internal_state >> 56) & 0xff;
        ctx->rc2.plain[1] = (ctx->internal_state >> 48) & 0xff;
        ctx->rc2.plain[2] = (ctx->internal_state >> 40) & 0xff;
        ctx->rc2.plain[3] = (ctx->internal_state >> 32) & 0xff;
        ctx->rc2.plain[4] = (ctx->internal_state >> 24) & 0xff;
        ctx->rc2.plain[5] = (ctx->internal_state >> 16) & 0xff;
        ctx->rc2.plain[6] = (ctx->internal_state >> 8) & 0xff;
        ctx->rc2.plain[7] = ctx->internal_state & 0xff;
       
        // Encrypt with RC2
        rc2_encrypt(&ctx->rc2);
        
        // Reconstruct internal state from ciphertext
        ctx->internal_state = 0;
        for (int i = 0; i < 8; i++) {
            ctx->internal_state = (ctx->internal_state << 8) + (ctx->rc2.cipher[i] & 0xff);
        }

        // XOR the bit and update internal state
        tempy = bits[48 - sso];
        bits[48 - sso] = bits[48 - sso] ^ (ctx->internal_state & 1);
        ctx->internal_state = rol64(ctx->internal_state, 1);
        swapbit(&ctx->internal_state, tempy);
        
    }
}

/* Key creation for Retevis AP */
void retevis_rc2_keystream_creation(dsd_state *state, char *input) {
 
    unsigned char key1[64]; memset(key1, 0, sizeof(key1));
    unsigned char key2[64]; memset(key2, 0, sizeof(key2));
    
    char buf[1024];
    strncpy(buf, input, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    
    char *pEnd;
    uint64_t K1 = strtoull(buf, &pEnd, 16);
    uint64_t K2 = strtoull(pEnd, &pEnd, 16);
    uint64_t K3 = strtoull(pEnd, &pEnd, 16);
    uint64_t K4 = strtoull(pEnd, &pEnd, 16);
    
    if (K3 != 0 || K4 != 0) //256-bit keys loaded as ASCII characters
    {
        //start loading K1, K2, K3, and K4 as ASCII to fill 64 bytes of key1
        int k = 0; uint8_t x = 0;

        for (int i = 0; i < 16; i++)
        {
            x = ((K1 >> (60-(i*4))) & 0xF);
            if (x >= 0 && x <= 9) //numbers 0-9
                x += 0x30;  //ASCII representation of numbers is 0x30, 0x31....0x39
            else x += 0x37; //Upper ASCII A,B,C,D,E,F is 0x41, 0x42...0x46
            key1[k++] = x;
        }

        for (int i = 0; i < 16; i++)
        {
            x = ((K2 >> (60-(i*4))) & 0xF);
            if (x >= 0 && x <= 9) //numbers 0-9
                x += 0x30;  //ASCII representation of numbers is 0x30, 0x31....0x39
            else x += 0x37; //Upper ASCII A,B,C,D,E,F is 0x41, 0x42...0x46
            key1[k++] = x;
        }

        for (int i = 0; i < 16; i++)
        {
            x = ((K3 >> (60-(i*4))) & 0xF);
            if (x >= 0 && x <= 9) //numbers 0-9
                x += 0x30;  //ASCII representation of numbers is 0x30, 0x31....0x39
            else x += 0x37; //Upper ASCII A,B,C,D,E,F is 0x41, 0x42...0x46
            key1[k++] = x;
        }

        for (int i = 0; i < 16; i++)
        {
            x = ((K4 >> (60-(i*4))) & 0xF);
            if (x >= 0 && x <= 9) //numbers 0-9
                x += 0x30;  //ASCII representation of numbers is 0x30, 0x31....0x39
            else x += 0x37; //Upper ASCII A,B,C,D,E,F is 0x41, 0x42...0x46
            key1[k++] = x;
        }

        //debug
        // fprintf (stderr, "ASCII: ");
        // for (int i = 0; i < 64; i++)
        //     fprintf (stderr, " %02X", key1[i]);
        // fprintf (stderr, "\n");

        // Initialize RC2 context
        static CryptoContext rc2_ctx;
        create_keys_rc2(&rc2_ctx, key1, 64);
        
        // Store context in DSD state
        state->rc2_context = malloc(sizeof(CryptoContext));
        memcpy(state->rc2_context, &rc2_ctx, sizeof(CryptoContext));

        fprintf(stderr, "DMR RETEVIS AP (RC2) 256-bit Key %016llX%016llX%016llX%016llX with Forced Application\n", 
            (unsigned long long)K1, (unsigned long long)K2, (unsigned long long)K3, (unsigned long long)K4);

        state->retevis_ap = 1;
    }

    else //128-bit loaded as reverse bytes
    {

        u64_to_bytes_be(K1, &key1[0]);
        u64_to_bytes_be(K2, &key1[8]);

        // reverse load the key bytes into key2
        for (int i=0;i<16;i++)
            key2[i] = key1[15-i];
        
        // Initialize RC2 context
        static CryptoContext rc2_ctx;
        create_keys_rc2(&rc2_ctx, key2, 16);
        
        // Store context in DSD state
        state->rc2_context = malloc(sizeof(CryptoContext));
        memcpy(state->rc2_context, &rc2_ctx, sizeof(CryptoContext));
        
        fprintf(stderr, "DMR RETEVIS AP (RC2) 128-bit Key %016llX%016llX with Forced Application\n", 
            (unsigned long long)K1, (unsigned long long)K2);

        state->retevis_ap = 1;
    }
}
