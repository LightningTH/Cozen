#include <malloc.h>
#include <string.h>
#include <sys/random.h>
#include "DiffieHellman.h"

void DiffieHellman::DHMul128(__uint128_t a, __uint128_t b, __uint128_t *ret)
{
    __uint128_t low;
    __uint128_t mid1;
    __uint128_t mid2;
    __uint128_t high;
    __uint128_t a_part[2];
    __uint128_t b_part[2];

    //split up our 64bit incoming values so we can properly operate on it
    a_part[0] = a & 0xffffffffffffffffull;
    a_part[1] = a >> 64;
    b_part[0] = b & 0xffffffffffffffffull;
    b_part[1] = b >> 64;

    //handle portions
    high = (a_part[1] * b_part[1]);
    mid1 = (a_part[0] * b_part[1]);
    mid2 = (a_part[1] * b_part[0]);
    low = (a_part[0] * b_part[0]);

    //combine
    ret[0] = low + (mid1 << 64) + (mid2 << 64);
    ret[1] = high + (mid1 >> 64) + (mid2 >> 64);
    ret[1] += (((mid1 & 0xffffffffffffffffull) + (mid2 & 0xffffffffffffffffull) + (low >> 64)) >> 64);
}

void DiffieHellman::DHMul256(__uint128_t a[2], __uint128_t b[2], __uint128_t *ret)
{
    __uint128_t low[2];
    __uint128_t mid1[2];
    __uint128_t mid2[2];
    __uint128_t high[2];

    //handle portions
    DHMul128(a[1], b[1], high);
    DHMul128(a[0], b[1], mid1);
    DHMul128(a[1], b[0], mid2);
    DHMul128(a[0], b[0], low);

    //combine
    ret[0] = low[0];
    ret[2] = 0;
    ret[3] = 0;
    ret[1] = low[1] + mid1[0];
    if(ret[1] < low[1])
        ret[2]++;
    ret[1] += mid2[0];
    if(ret[1] < mid2[0])
        ret[2]++;
    ret[2] += high[0];
    if(ret[2] < high[0])
        ret[3]++;
    ret[2] += mid1[1];
    if(ret[2] < mid1[1])
        ret[3]++;
    ret[2] += mid2[1];
    if(ret[2] < mid2[1])
        ret[3]++;
    ret[3] += high[1];
}

void DiffieHellman::DHMod512(__uint128_t a[4], __uint128_t b[2], __uint128_t *ret)
{
    __uint128_t x[4];
    __uint128_t atemp[4];

    x[0] = b[0];
    x[1] = b[1];
    x[2] = 0;
    x[3] = 0;

    //atemp = A >> 1
    atemp[0] = a[0] >> 1;
    atemp[1] = a[1] >> 1;
    atemp[2] = a[2] >> 1;
    atemp[3] = a[3] >> 1;
    atemp[0] |= a[1] << 127;
    atemp[1] |= a[2] << 127;
    atemp[2] |= a[3] << 127;

    while(1)
    {
        //if X > (A / 2) break
        if(
           ((x[3] > atemp[3]) ||
            ((x[3] == atemp[3]) &&
             ((x[2] > atemp[2]) ||
              ((x[2] == atemp[2]) &&
               ((x[1] > atemp[1]) ||
                ((x[1] == atemp[1]) &&
                 (x[0] > atemp[0])
                )
               )
              )
             )
            )
           )
          )
            break;

        x[3] <<= 1;
        x[3] |= (x[2] >> 127);
        x[2] <<= 1;
        x[2] |= (x[1] >> 127);
        x[1] <<= 1;
        x[1] |= (x[0] >> 127);
        x[0] <<= 1;
    }

    //copy the input so we don't modify the original
    atemp[0] = a[0];
    atemp[1] = a[1];
    atemp[2] = a[2];
    atemp[3] = a[3];

    while(1)
    {
        //if A < B break
        if(!atemp[3] && !atemp[2] && ((atemp[1] < b[1]) || ((atemp[1] == b[1]) && (atemp[0] < b[0]))))
            break;

        //if A >= X {A -= X}
        if(
           ((atemp[3] > x[3]) ||
            ((atemp[3] == x[3]) &&
             ((atemp[2] > x[2]) ||
              ((atemp[2] == x[2]) &&
               ((atemp[1] > x[1]) ||
                ((atemp[1] == x[1]) &&
                 (atemp[0] >= x[0])
                )
               )
              )
             )
            )
           )
          )
        {
            if(atemp[0] < x[0])
                atemp[1]--;
            if(atemp[1] < x[1])
                atemp[2]--;
            if(atemp[2] < x[2])
                atemp[3]--;
            atemp[0] = atemp[0] - x[0];
            atemp[1] = atemp[1] - x[1];
            atemp[2] = atemp[2] - x[2];
            atemp[3] = atemp[3] - x[3];
        }

        //x >>= 1
        x[0] >>= 1;
        x[0] |= (x[1] << 127);
        x[1] >>= 1;
        x[1] |= (x[2] << 127);
        x[2] >>= 1;
        x[2] |= (x[3] << 127);
        x[3] >>= 1;
    }

    ret[0] = atemp[0];
    ret[1] = atemp[1];
}

void DiffieHellman::DHPowMod256(__uint128_t g[2], __uint128_t p[2], __uint128_t *ret)
{
    //do a 128bit pow/mod, (g**pr) % m
    __uint128_t result[4];
    __uint128_t gl[4];
    __uint128_t ml[2];
    __uint128_t pr[2];

    pr[0] = p[0];
    pr[1] = p[1];
    gl[0] = g[0];
    gl[1] = g[1];
    gl[2] = 0;
    gl[3] = 0;
    ml[0] = _P[0];
    ml[1] = _P[1];
    result[0] = 1;
    result[1] = 0;
    result[2] = 0;
    result[3] = 0;

    while(pr[0] || pr[1])
    {
        //if set then do a multiple and modulus
        if(pr[0] & 1)
        {
            //result = (result * gl) % ml;
            DHMul256(result, gl, result);
            DHMod512(result, ml, result);
            result[2] = 0;
            result[3] = 0;
        }

        //bit shift
        pr[0] >>= 1;
        pr[0] |= pr[1] << 127;
        pr[1] >>= 1;

        //gl = (gl * gl) % ml;
        DHMul256(gl, gl, gl);
        DHMod512(gl, ml, gl);
        gl[2] = 0;
        gl[3] = 0;
    };

    ret[0] = result[0];
    ret[1] = result[1];
}

DiffieHellman::DiffieHellman(ConnectionBase *Connection) :
EncryptionBase(Connection, 32, 1)
{
    _P[0] = 0;
    _P[1] = 0;
    _G[0] = 0;
    _G[1] = 0;
    _private[0] = 0;
    _private[1] = 0;
}

DiffieHellman::~DiffieHellman()
{
    _P[0] = 0;
    _P[1] = 0;
    _G[0] = 0;
    _G[1] = 0;
    _private[0] = 0;
    _private[1] = 0;
}

size_t DiffieHellman::EncryptData(size_t Len)
{
    __uint128_t challenge[2];

    if(!_P[0] || !_P[1] || !_G[0] || !_G[1])
        return 0;

    //create a DH handshake
    getrandom(&_private, sizeof(_private), 0);
    DHPowMod256(_G, _private, challenge);
    memcpy(_OutData, challenge, sizeof(challenge));
    return sizeof(challenge);
}

size_t DiffieHellman::DecryptData(size_t Len, bool LastBlock)
{
    //finish the DH handshake
    __uint128_t ret[2];
    __uint128_t challenge[2];

    if(!_P[0] || !_P[1] || !_G[0] || !_G[1] || !_private[0] || !_private[1])
        return 0;

    if(Len != sizeof(challenge))
        return 0;

    memcpy(challenge, _InDataEnd, sizeof(challenge));
    DHPowMod256(challenge, _private, ret);
    memcpy(_InDataEnd, ret, sizeof(ret));

    _private[0] = 0;
    _private[1] = 0;

    return sizeof(ret);
}

size_t DiffieHellman::SetConfig(size_t ConfigID, void *Data)
{
    switch(ConfigID)
    {
        case DiffieHellman::DH_P:
            memcpy(&_P, Data, sizeof(_P));
            _G[0] = 0;
            _G[1] = 0;
            _private[0] = 0;
            _private[1] = 0;
            return 1;

        case DiffieHellman::DH_G:
            memcpy(&_G, Data, sizeof(_G));
            _private[0] = 0;
            _private[1] = 0;
            if((_G[1] > _P[1]) || ((_G[1] == _P[1]) && (_G[0] >= _P[0])))
                return 0;

            return 1;

        default:
            break;
    };
    return 0;
}

EncryptionBase *CreateDiffieHellman(ConnectionBase *Connection)
{
    return new DiffieHellman(Connection);
}

__attribute__((constructor)) static void init()
{
    RegisterEncryption(CreateDiffieHellman, MAGIC_VAL('D', 'H', 'M', 'N'), "Diffie Hellman");
}
