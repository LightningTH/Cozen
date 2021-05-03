#ifndef NEKISAHLOTH_LIB_ENCRYPTION_DIFFIEHELLMAN
#define NEKISAHLOTH_LIB_ENCRYPTION_DIFFIEHELLMAN

#include "EncryptionBase.h"

class DiffieHellman : public EncryptionBase
{
	public:
        typedef enum DiffieHellmanConfigEnum
        {
            DH_P = 0x100,
            DH_G = 0x101
        } DiffieHellmanConfigEnum;

		DiffieHellman(ConnectionBase *Connection);
        ~DiffieHellman();

        size_t SetConfig(size_t ConfigID, void *Data);
		size_t EncryptData(size_t Len);
		size_t DecryptData(size_t Len, bool LastBlock);

	private:
        void DHMul128(__uint128_t a, __uint128_t b, __uint128_t *ret);
        void DHMul256(__uint128_t a[2], __uint128_t b[2], __uint128_t *ret);
        void DHMod512(__uint128_t a[4], __uint128_t b[2], __uint128_t *ret);
        void DHPowMod256(__uint128_t g[2], __uint128_t p[2], __uint128_t *ret);

        __uint128_t _P[2], _G[2], _private[2];
};

#endif