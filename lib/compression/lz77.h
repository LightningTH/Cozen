#ifndef NEKISAHLOTH_LIB_LZ77
#define NEKISAHLOTH_LIB_LZ77

#include "CompressionBase.h"

class LZ77 : public CompressionBase
{
	public:
		LZ77(ConnectionBase *Connection, HashBase *Hash);
		~LZ77();

		size_t DecompressData(size_t Len);
		size_t CompressData(size_t Len);
		size_t GetCompressSize(size_t Len);

	private:
		size_t GetBits(size_t BitCount);
		size_t WriteBits(size_t Val, size_t BitCount);
		size_t EncodeLength(uint32_t Len, uint8_t *Buffer);
		uint32_t DecodeLength(size_t Len);
		size_t SearchForMatch(uint8_t *Buffer, size_t SlidingWindowSize, size_t BufferLeft, size_t *FindPos, size_t *MatchLen);

		uint32_t _CurrentBits;
		uint32_t _BufferByteLoc;
		uint8_t _BufferBitLoc;
		uint8_t _BitCountAvailable;

		uint8_t *_TempBuffer;
};

#endif
