#ifndef NEKISAHLOTH_LIB_ZLIB
#define NEKISAHLOTH_LIB_ZLIB

#include <zlib.h>
#include "CompressionBase.h"

class ZLib : public CompressionBase
{
	public:
		ZLib(ConnectionBase *Connection, HashBase *Hash);
		~ZLib();

		size_t DecompressData(size_t Len);
		size_t CompressData(size_t Len);
		size_t GetCompressSize(size_t Len);

	private:
		uint8_t *_TempBuffer;
};

#endif
