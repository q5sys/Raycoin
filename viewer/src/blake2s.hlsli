//Copyright (C) 2018 NewGamePlus Inc. - All Rights Reserved
//Unauthorized copying/distribution of this source code or any derived source code or binaries
//via any medium is strictly prohibited. Proprietary and confidential.


enum blake2s_constant
{
    BLAKE2S_BLOCKBYTES = 64,
    BLAKE2S_BLOCKINTS = BLAKE2S_BLOCKBYTES / 4
};

struct blake2s_state
{
    uint4 h[2];
    uint2 t;
    uint2 f;
};

#ifndef blake2s_inBytesMax
    #define blake2s_inBytesMax 128
#endif
typedef uint4 blake2s_in[(blake2s_inBytesMax)/16];

static const uint blake2s_IV[8] =
{
  0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
  0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
};

static const uint blake2s_sigma[10][BLAKE2S_BLOCKINTS] =
{
  {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
  { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 } ,
  { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 } ,
  {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 } ,
  {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 } ,
  {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 } ,
  { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 } ,
  { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 } ,
  {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 } ,
  { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13 , 0 } ,
};

#define ROTR32(w, c) ((w) >> c) | ((w) << (32 - c))

#define G(r,i,a,b,c,d)                          \
  {                                             \
    int j = in_cur + blake2s_sigma[r][2*i+0];   \
    a = a + b + in_[j>>2][j&3];                 \
    d = ROTR32(d ^ a, 16);                      \
    c = c + d;                                  \
    b = ROTR32(b ^ c, 12);                      \
    j = in_cur + blake2s_sigma[r][2*i+1];       \
    a = a + b + in_[j>>2][j&3];                 \
    d = ROTR32(d ^ a, 8);                       \
    c = c + d;                                  \
    b = ROTR32(b ^ c, 7);                       \
  }

#define ROUND(r)                    \
  {                                 \
    G(r,0,v[ 0],v[ 4],v[ 8],v[12]); \
    G(r,1,v[ 1],v[ 5],v[ 9],v[13]); \
    G(r,2,v[ 2],v[ 6],v[10],v[14]); \
    G(r,3,v[ 3],v[ 7],v[11],v[15]); \
    G(r,4,v[ 0],v[ 5],v[10],v[15]); \
    G(r,5,v[ 1],v[ 6],v[11],v[12]); \
    G(r,6,v[ 2],v[ 7],v[ 8],v[13]); \
    G(r,7,v[ 3],v[ 4],v[ 9],v[14]); \
  }

static void blake2s_compress(inout blake2s_state S, const blake2s_in in_, const int in_cur)
{
    uint v[BLAKE2S_BLOCKINTS];
    int i;
    [unroll]
    for (i = 0; i < 4; ++i)
    {
        v[i] = S.h[0][i];
        v[i+4] = S.h[1][i];
    }
        
    v[8] = blake2s_IV[0];
    v[9] = blake2s_IV[1];
    v[10] = blake2s_IV[2];
    v[11] = blake2s_IV[3];
    v[12] = S.t[0] ^ blake2s_IV[4];
    v[13] = S.t[1] ^ blake2s_IV[5];
    v[14] = S.f[0] ^ blake2s_IV[6];
    v[15] = S.f[1] ^ blake2s_IV[7];

    ROUND(0);
    ROUND(1);
    ROUND(2);
    ROUND(3);
    ROUND(4);
    ROUND(5);
    ROUND(6);
    ROUND(7);
    ROUND(8);
    ROUND(9);

    [unroll]
    for (i = 0; i < 4; ++i)
    {
        S.h[0][i] = S.h[0][i] ^ v[i] ^ v[i+8];
        S.h[1][i] = S.h[1][i] ^ v[i+4] ^ v[i+12];
    }
}

#undef G
#undef ROUND
#undef ROTR32

void blake2s_init(inout blake2s_state S)
{
    S.t = 0;
    S.f = 0;
    int i;
    [unroll]
    for (i = 0; i < 4; ++i)
    {
        S.h[0][i] = blake2s_IV[i];
        S.h[1][i] = blake2s_IV[i+4];
    }

    //IV XOR ParamBlock
    #define digest_length 32
    #define key_length 0
    #define fanout 1
    #define depth 1
    S.h[0][0] ^= digest_length | (key_length << 8) | (fanout << 16) | (depth << 24);
    #undef digest_length
    #undef key_length
    #undef fanout
    #undef depth
}

//use default inlen to step hash as a random generator
void blake2s(inout blake2s_state S, blake2s_in in_, uint inlen = 0)
{
    int in_cur = 0;
    while (inlen > BLAKE2S_BLOCKBYTES)
    {
        S.t[0] += BLAKE2S_BLOCKBYTES;
        S.t[1] += (S.t[0] < BLAKE2S_BLOCKBYTES);
        blake2s_compress(S, in_, in_cur);
        in_cur += BLAKE2S_BLOCKINTS;
        inlen -= BLAKE2S_BLOCKBYTES;
    }

    S.t[0] += inlen;
    S.t[1] += (S.t[0] < inlen);
    S.f[0] = (uint)-1;
    //pad last block int4s
    int blockint = (inlen+3)/4;
    int j = in_cur;
    switch ((blockint+3)/4)
    {
        case 0: in_[(j>>2)] = 0; in_[(j>>2)+1] = 0; in_[(j>>2)+2] = 0; in_[(j>>2)+3] = 0; break;
        case 1: in_[(j>>2)+1] = 0; in_[(j>>2)+2] = 0; in_[(j>>2)+3] = 0; break;
        case 2: in_[(j>>2)+2] = 0; in_[(j>>2)+3] = 0; break;
        case 3: in_[(j>>2)+3] = 0; break;
        default: break;
    }
    //pad last block ints
    j = in_cur + blockint;
    switch (blockint % 4)
    {
        case 0: break;
        case 1: in_[j>>2].yzw = 0; break;
        case 2: in_[j>>2].zw = 0; break;
        case 3: in_[j>>2].w = 0; break;
    }
    //pad last block bytes
    j = in_cur + inlen/4;
    switch (inlen % 4)
    {
        case 0: break;
        case 1: in_[j>>2][j&3] &= 0x000000FF; break;
        case 2: in_[j>>2][j&3] &= 0x0000FFFF; break;
        case 3: in_[j>>2][j&3] &= 0x00FFFFFF; break;
    }
    blake2s_compress(S, in_, in_cur);
}