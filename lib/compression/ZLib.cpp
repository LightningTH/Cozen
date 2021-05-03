#include "ZLib.h"
#include <sstream>
#include <bsd/bsd.h>
#include <fstream>

typedef struct ZLibSizeHeader
{
	uint32_t CompressSize;
	uint32_t DecompressSize;
} ZLibSizeHeader;

#define HEADER_SIZE sizeof(ZLibSizeHeader)

ZLib::ZLib(ConnectionBase *Connection, HashBase *Hash) :
	CompressionBase(Connection, Hash)
{
	_CompressBufferSize = 1024 * 32;
	_DecompressBufferSize = 1024 * 32;
	_CompressBuffer = (uint8_t *)malloc(_CompressBufferSize);
	_DecompressBuffer = (uint8_t *)malloc(_DecompressBufferSize);
	_TempBuffer = (uint8_t *)malloc(_DecompressBufferSize);
}

ZLib::~ZLib()
{
	//if we have a buffer then free it
	if(_CompressBuffer)
		free(_CompressBuffer);

	if(_DecompressBuffer)
		free(_DecompressBuffer);

	if(_TempBuffer)
		free(_TempBuffer);
}

size_t ZLib::GetCompressSize(size_t Len)
{
	uint32_t count;

	//if we don't have room then fail
	if(Len < HEADER_SIZE)
		return 0;

	//return the amount of data needed to read before we can decompress
	count = ((ZLibSizeHeader *)&_DecompressBuffer[0])->CompressSize;
	if(count < HEADER_SIZE)
		return 0;

	return count;
}

size_t ZLib::DecompressData(size_t Len)
{
	z_stream strm;
	int ret;
	ZLibSizeHeader Header;

	//if no data then fail
	if(Len < HEADER_SIZE)
		return 0;

	//get our sizes
	memcpy(&Header, _DecompressBuffer, sizeof(Header));

	//if no size or too large then fail
	if(!Header.DecompressSize || (Header.DecompressSize > _DecompressBufferSize) || (Header.CompressSize <= HEADER_SIZE) || (Header.CompressSize > Len))
		return 0;

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = Header.CompressSize - HEADER_SIZE;
	strm.next_in = Z_NULL;
	ret = inflateInit(&strm);
	if (ret != Z_OK)
		return ret;

	//copy our compressed stream to our temporary buffer
	memcpy(_TempBuffer, &_DecompressBuffer[HEADER_SIZE], Header.CompressSize - HEADER_SIZE);

	//setup the output area
	strm.next_in = _TempBuffer;
	strm.avail_out = _DecompressBufferSize;
	strm.next_out = _DecompressBuffer;

	//decompress the data, should hit end of the stream
	ret = inflate(&strm, Z_FINISH);
	inflateEnd(&strm);
	if(ret != Z_STREAM_END)
		return 0;

	//check our decompressed size
	if((_DecompressBufferSize - strm.avail_out) != Header.DecompressSize)
		return 0;

	//set our decompressed size
	_DecompressSize = Header.DecompressSize;
	return 1;
}

size_t ZLib::CompressData(size_t Len)
{
	z_stream strm;
	int ret;
	size_t orig_available;
	ZLibSizeHeader *CompressHeader;

	//if the input is too large then fail
	if(Len > _CompressBufferSize)
		return 0;

	/* allocate deflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = Len;
	strm.next_in = _CompressBuffer;
	ret = deflateInit(&strm, 9);
	if (ret != Z_OK)
		return 0;

	//setup the output area
	orig_available = _CompressBufferSize - HEADER_SIZE;
	strm.avail_out = orig_available;
	strm.next_out = &_TempBuffer[HEADER_SIZE];

	//have all the data, compress it
	ret = deflate(&strm, Z_FINISH);
	deflateEnd(&strm);
	if((ret == Z_STREAM_ERROR) || (strm.avail_in != 0))
		return 0;

	//store how much we compressed into the buffer
	_CompressSize = orig_available - strm.avail_out + HEADER_SIZE;
	CompressHeader = (ZLibSizeHeader *)&_TempBuffer[0];
	CompressHeader->DecompressSize = Len;
	CompressHeader->CompressSize = _CompressSize;

	//copy from our temporary buffer to our compressed buffer
	memcpy(_CompressBuffer, _TempBuffer, _CompressSize);
	return _CompressSize;
}

CompressionBase *CreateZLib(ConnectionBase *Connection, HashBase *Hash)
{
	return new ZLib(Connection, Hash);
}

__attribute__((constructor)) static void init()
{
	RegisterCompression(CreateZLib, MAGIC_VAL('Z', 'L', 'i', 'b'), "ZLib");
}