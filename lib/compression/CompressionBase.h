#ifndef NEKISAHLOTH_LIB_COMPRESSION_BASE
#define NEKISAHLOTH_LIB_COMPRESSION_BASE

#include <unistd.h>
#include <stdint.h>
#include "ConnectionBase.h"
#include "HashBase.h"

class CompressionBase : public ConnectionBase
{
	public:
		CompressionBase(ConnectionBase *Connection, HashBase *Hash);
		virtual ~CompressionBase();

		size_t Read(void *Buffer, size_t ReadLen);
		size_t Write(const void *Buffer, size_t WriteLen);
		bool DataAvailableToRead();

	protected:
		virtual size_t DecompressData(size_t Len);
		virtual size_t CompressData(size_t Len);
		virtual size_t GetCompressSize(size_t Len);

		size_t _CompressSize;
		size_t _DecompressSize;
		size_t _CompressBufferSize;
		size_t _DecompressBufferSize;
		size_t _DecompressBufferPos;
		uint8_t *_CompressBuffer;
		uint8_t *_DecompressBuffer;
		uint8_t *_DecompressHashData;
		HashBase *_Hash;
};

#define MAGIC_VAL(a,b,c,d) (((size_t)a << 24) | ((size_t)b << 16) | ((size_t)c << 8) | ((size_t)d))
typedef CompressionBase *(*CompressionClassInit)(ConnectionBase *Connection, HashBase *Hash);
void RegisterCompression(CompressionClassInit Init, uint32_t ID, const char *Name);
CompressionClassInit GetCompression(uint32_t ID);

typedef struct CompressionStruct {
    struct CompressionStruct *Next;
    CompressionClassInit Init;
    uint32_t ID;
	uint32_t CustomID;
    const char *Name;
} CompressionStruct;

extern CompressionStruct *CompressionEntries;

#endif
