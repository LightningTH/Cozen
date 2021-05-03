#ifndef NEKISAHLOTH_LIB_HASH_BASE
#define NEKISAHLOTH_LIB_HASH_BASE

#include <unistd.h>
#include <stdint.h>

class HashBase
{
	public:
		HashBase(size_t HashLen);

		virtual uint8_t *GetHash();
		virtual ~HashBase();
		size_t GetHashLen();
		virtual void Reset();
		virtual size_t AddData(void *Buffer, size_t Len);
		bool CompareHash(void *HashBuffer);
		bool CompareHashAsString(char *HashBuffer);
		const char *GetHashAsString();
		const char *GetHashAsString(void *HashBuffer);

	protected:

		uint8_t *_HashData;
		size_t _HashLen;

	private:
		char *_InternalHashStr;
		char *_ExternalHashStr;
		char *_ConvertToString(char *CharBuffer, void *HashBuffer);
};

#define MAGIC_VAL(a,b,c,d) (((size_t)a << 24) | ((size_t)b << 16) | ((size_t)c << 8) | ((size_t)d))
typedef HashBase *(*HashClassInit)();
void RegisterHash(HashClassInit Init, uint32_t ID, const char *Name);
HashClassInit GetHash(uint32_t ID);

struct HashStruct {
    struct HashStruct *Next;
    HashClassInit Init;
    uint32_t ID;
	uint32_t CustomID;
    const char *Name;
};

extern HashStruct *HashEntries;

#endif
