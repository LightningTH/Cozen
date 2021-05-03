#include "lz77.h"
#include <string.h>
#include <malloc.h>

#define POS_BITS 13
#define MAX_POS ((1 << POS_BITS)-1)
#define LEN_BITS 4
#define MAX_LEN ((1 << LEN_BITS)-1)

LZ77::LZ77(ConnectionBase *Connection, HashBase *Hash) :
	CompressionBase(Connection, Hash)
{
	_CurrentBits = 0;
	_BitCountAvailable = 0;
	_BufferByteLoc = 0;
	_BufferBitLoc = 0;

	_DecompressBufferSize = 1024 * 64;
	_CompressBufferSize = _DecompressBufferSize;
	_CompressBuffer = (uint8_t *)malloc(_CompressBufferSize);
	_DecompressBuffer = (uint8_t *)malloc(_DecompressBufferSize);
	_TempBuffer = (uint8_t *)malloc(_DecompressBufferSize);

#ifdef COZEN_DEBUG
	dprintf(2, "lz77 _DecompressBuffer %p\n", _DecompressBuffer);
#endif
}

LZ77::~LZ77()
{
	//if we have a buffer then free it
	if(_CompressBuffer)
		free(_CompressBuffer);

	if(_DecompressBuffer)
		free(_DecompressBuffer);

	if(_TempBuffer)
		free(_TempBuffer);
}

size_t LZ77::WriteBits(size_t Val, size_t BitCount)
{
	uint8_t CurByte;
	size_t BitsAvailable;
	size_t BitCountLeft;

	//make sure there is room in the compress buffer
	BitCountLeft = BitCount;
	if((_BufferByteLoc + ((BitCountLeft + 7) >> 3)) >= _CompressBufferSize)
		return 0;

	//write some number of bits to our compress buffer
	while(BitCountLeft)
	{
		CurByte = _TempBuffer[_BufferByteLoc];
		BitsAvailable = 8 - _BufferBitLoc;

		//if the available bits is more than the bits to store then just go with the bits to store
		if(BitsAvailable > BitCountLeft)
			BitsAvailable = BitCountLeft;

		//add bits in starting right to left for the bit stream
		CurByte |= ((Val >> (BitCountLeft - BitsAvailable)) << (8 - _BufferBitLoc - BitsAvailable));
		_TempBuffer[_BufferByteLoc] = CurByte;
	
		//update our value for bits left
		Val &= ((1 << (BitCountLeft - BitsAvailable)) - 1);

		//update our number of bits left and shift over our buffer location info
		BitCountLeft -= BitsAvailable;
		_BufferBitLoc = (_BufferBitLoc + BitsAvailable) % 8;
		if(!_BufferBitLoc)
			_BufferByteLoc++;
	}

	return BitCount;
}

size_t LZ77::GetBits(size_t BitCount)
{
	uint8_t NextByte;
	size_t Ret;

	//return the number of bits requested, if our current buffer is not full enough then get more from the input
	//note that size_t is 64bit and we never make a 64bit request so we don't need to check for overflow
	while(BitCount > _BitCountAvailable)
	{
		NextByte = _DecompressBuffer[_BufferByteLoc];
		_BufferByteLoc++;
		_BitCountAvailable += 8;
		_CurrentBits = (_CurrentBits << 8) | NextByte;
	};

	//we have enough bits to return so get the bits we need
	Ret = _CurrentBits >> (_BitCountAvailable - BitCount);

	//subtract and mask off the bits we returned
	_BitCountAvailable -= BitCount;
	_CurrentBits &= ((1 << _BitCountAvailable) - 1);

	//return the bits requested
	return Ret;
}

size_t LZ77::EncodeLength(uint32_t Len, uint8_t *Buffer)
{
	size_t ByteLoc = 0;

	//encode our length
	while(Len)
	{
		Len--;
		Buffer[ByteLoc] = Len & 0x7f;
		Len >>= 7;
		if(Len)
			Buffer[ByteLoc] |= 0x80;

		ByteLoc++;
	};

	return ByteLoc;
}

size_t LZ77::GetCompressSize(size_t Len)
{
	size_t Ret = 0;

	//count number of bytes with high bit set
	while((_DecompressBuffer[Ret] & 0x80) && (Ret < Len))
		Ret++;

	//if we hit the end then we are not done yet
	if(Ret >= Len)
		return 0;

	//decode the length in our buffer
	_BufferByteLoc = 0;

	//get the size of the compressed data
	return DecodeLength(Len);
}

uint32_t LZ77::DecodeLength(size_t Len)
{
	size_t TempChar;
	size_t Ret = 0;
	size_t ShiftPos = 0;

	//decode our length field
	do
	{
		TempChar = _DecompressBuffer[_BufferByteLoc];
		_BufferByteLoc++;
		Ret = (((TempChar & 0x7f) + 1) << ShiftPos) + Ret;

		if(Ret > UINT32_MAX)
			return 0;

		ShiftPos += 7;
	} while((TempChar & 0x80) && (_BufferByteLoc < Len));

	return (uint32_t)Ret;
}

#ifdef COZEN_DEBUG
#include <fcntl.h>
#endif

size_t LZ77::DecompressData(size_t Len)
{
	size_t Pos;
	size_t Counter;
	uint32_t OrigSize;
	uint32_t CompressSize;
	size_t DecompressSize = 0;
	size_t FoundEnd = 0;

	//setup for a new decompression
	_CurrentBits = 0;
	_BufferByteLoc = 0;
	_BufferBitLoc = 0;
	_BitCountAvailable = 0;

	//get the size of the compressed data
	CompressSize = DecodeLength(Len);
	if(!CompressSize)
		return 0;

	//get the size of the original data
	OrigSize = DecodeLength(Len);
	if(!OrigSize || (OrigSize > _DecompressBufferSize) || (CompressSize > Len))
		return 0;

	memset(_TempBuffer, 0, _DecompressBufferSize);
	while(DecompressSize < OrigSize)
	{
		//see if this is a normal character or a lookup
		if(GetBits(1) == 0)
		{
			if((DecompressSize + 1) > OrigSize)
				return 0;

			//now add a byte to the out buffer from the pointer
			_TempBuffer[DecompressSize] = GetBits(8);
			DecompressSize++;
		}
		else
		{
			//position must be at least 1 behind us
			Pos = GetBits(POS_BITS);

			//we will encode a minimum of 2 matches into an entry otherwise it is smaller
			//to just store the 2 bytes
			Len = GetBits(LEN_BITS);

			//we are done if all bits are set
			if((Pos == MAX_POS) && (Len == MAX_LEN))
			{
#ifdef COZEN_DEBUG
				int fd = open("a.dmp", O_RDWR | O_CREAT, 0755);
				write(fd, &_TempBuffer[-0x2000], 0x2000);
				close(fd);
#endif
				FoundEnd = 1;
				break;
			}

			//now add our offsets
			Pos += 1;
			Len += 2;

			//make sure there is enough room
			if((DecompressSize + Len) > OrigSize)
				return 0;
			
			//now keep adding bytes in a loop as we might be accessing data we place
			//BUG: we don't validate that the buffer we are looking back in is already filled or big enough for the offsets used
			for(Counter = 0; Counter < Len; Counter++)
				_TempBuffer[DecompressSize + Counter] = _TempBuffer[DecompressSize - Pos + Counter];

			//update the size
			DecompressSize += Len;
		}
	};

	//if we didn't find the end then attempt a read for the end flag to confirm we are done
	if(!FoundEnd)
	{
		if(!GetBits(1))
			return 0;

		Pos = GetBits(POS_BITS);
		Len = GetBits(LEN_BITS);
	}

	//see if we hit the end of the stream, if not then there is a problem so fail
	if((Pos != MAX_POS) || (Len != MAX_LEN))
		return 0;

	//we now have our original data, copy over our current data
	memcpy(_DecompressBuffer, _TempBuffer, DecompressSize);
	_DecompressSize = DecompressSize;

	//all good, return the size
	return DecompressSize;
}

//a multiple of size_t that is big enough to cover the maximum number of matches we can have
#define MATCH_ARRAY_SIZE (MAX_LEN + 3 + (sizeof(size_t) - 1)) / sizeof(size_t)
size_t LZ77::SearchForMatch(uint8_t *Buffer, size_t SlidingWindowSize, size_t BufferLeft, size_t *FindPos, size_t *MatchLen)
{
	size_t MatchLocs[MAX_LEN + 1];
	size_t CurWindowPos;
	size_t MaxWindowSize;
	size_t MinMatchLen;
	size_t CharsToMatch[MATCH_ARRAY_SIZE];
	uint8_t *CharsToMatchByte = (uint8_t *)CharsToMatch;
	size_t MatchMask[MATCH_ARRAY_SIZE];
	uint8_t *MatchMaskByte = (uint8_t *)MatchMask;
	size_t CompareCount;
	size_t CompareLen;

	//clear out a couple buffers used for matching
	memset(CharsToMatch, 0, sizeof(CharsToMatch));
	memset(MatchMask, 0, sizeof(MatchMask));

	//start with a minimum of matching bytes
	MatchMask[0] = 0xffff;

	//copy over what we need to match against to make comparisons easier, done to avoid a memcmp call
	//first see how many characters we can copy
	MinMatchLen = MAX_LEN + 3;
	if(MinMatchLen > BufferLeft)
		MinMatchLen = BufferLeft;

	//if our minimum match length is smaller than 2 then exit
	if(MinMatchLen < 2)
		return 0;

	//copy over as many size_t entries as we can without hitting the end
	for(CompareLen = 0; CompareLen < (MinMatchLen / sizeof(size_t)); CompareLen++)
		CharsToMatch[CompareLen] = *(size_t *)(&Buffer[CompareLen * sizeof(size_t)]);
	CompareLen *= sizeof(size_t);

	//if we have a 4 byte space to copy then copy it
	if((CompareLen + sizeof(uint32_t)) <= MinMatchLen)
	{
		*(uint32_t *)&CharsToMatchByte[CompareLen] = *(uint32_t *)&Buffer[CompareLen];
		CompareLen += sizeof(uint32_t);
	}
	//if we have a 2 byte space to copy then copy it
	if((CompareLen + sizeof(uint16_t)) <= MinMatchLen)
	{
		*(uint16_t *)&CharsToMatchByte[CompareLen] = *(uint16_t *)&Buffer[CompareLen];
		CompareLen += sizeof(uint16_t);
	}
	//if we have a 1 byte space to copy then copy it
	if((CompareLen + sizeof(uint8_t)) <= MinMatchLen)
	{
		CharsToMatchByte[CompareLen] = Buffer[CompareLen];
		CompareLen += sizeof(uint8_t);
	}

	//start looking for a match
	MaxWindowSize = MAX_POS;
	if(MaxWindowSize > SlidingWindowSize)
		MaxWindowSize = SlidingWindowSize;

	//adjust our max by 1 as we are not 0 based in the loop
	MaxWindowSize++;

	//walk backwards through the buffer up to our maximum window size looking for matches
	MinMatchLen = 0;
	for(CurWindowPos = 1; CurWindowPos < MaxWindowSize; CurWindowPos++)
	{
		//see if the current location matches, if so then store it in our match loc buffer and
		//increase our min match len
		for(CompareCount = 0; CompareCount < (CompareLen / sizeof(size_t)); CompareCount++)
		{
			if((CharsToMatch[CompareCount] ^ *(size_t *)(&Buffer[-CurWindowPos + (CompareCount * sizeof(size_t))])) & MatchMask[CompareCount])
				break;
		}

		//if we didn't get through all the entries then try the next character
		if(CompareCount != (CompareLen / sizeof(size_t)))
			continue;

		//update and see if we need to check any 4, 2, or 1 byte values
		CompareCount *= sizeof(size_t);
		if(CompareCount + sizeof(uint32_t) <= CompareLen)
		{
			if((*(uint32_t *)&CharsToMatchByte[CompareCount] ^ *(uint32_t *)&Buffer[-CurWindowPos + CompareCount]) & *(uint32_t *)&MatchMaskByte[CompareCount])
				continue;
			CompareCount += sizeof(uint32_t);
		}
		if(CompareCount + sizeof(uint16_t) <= CompareLen)
		{
			if((*(uint16_t *)&CharsToMatchByte[CompareCount] ^ *(uint16_t *)&Buffer[-CurWindowPos + CompareCount]) & *(uint16_t *)&MatchMaskByte[CompareCount])
				continue;
			CompareCount += sizeof(uint16_t);
		}
		if(CompareCount + sizeof(uint8_t) <= CompareLen)
		{
			if((CharsToMatchByte[CompareCount] ^ Buffer[-CurWindowPos + CompareCount]) & MatchMaskByte[CompareCount])
				continue;
		}

		//so far we have matched, store the location off so we know how many characters matched then adjust our mask and try again
		MatchLocs[MinMatchLen] = CurWindowPos - 1;
		MinMatchLen++;

		//if we hit maximum number of matches then exit the loop
		if((MinMatchLen > MAX_LEN) || ((MinMatchLen+2) > CompareLen))
			break;

		//adjust our mask
		for(CompareCount = 0; CompareCount < MATCH_ARRAY_SIZE; CompareCount++)
		{
			//if we filled a mask then skip and update the next entry
			if((MatchMask[CompareCount] + 1) != 0)
			{
				//we aren't full so adjust our mask and then stop as we can't update after us yet
				MatchMask[CompareCount] = (MatchMask[CompareCount] << 8) | 0xff;
				break;
			}
		}

		//try this location again
		CurWindowPos--;
	}

	//return if we found a match and what it was
	if(!MinMatchLen)
		return 0;

	//return data for the match
	*FindPos = MatchLocs[MinMatchLen - 1];
	*MatchLen = (MinMatchLen - 1);

	//if the value we are returning is our special marker then we need to alter by a single byte
	if((*FindPos == MAX_POS) && (*MatchLen == MAX_LEN))
		(*MatchLen)--;

	return 1;
}

size_t LZ77::CompressData(size_t Len)
{
	size_t CurPos;
	size_t Pos;
	size_t MatchLen;
	size_t EncodeSize;

	//if too large then fail
	if(Len > _CompressBufferSize)
		return 0;

	//first, store into the buffer our original size
	memset(_TempBuffer, 0, _DecompressBufferSize);
	_BufferByteLoc = 0;
	_BufferBitLoc = 0;
	_CurrentBits = 0;
	_BufferByteLoc = EncodeLength(Len, _TempBuffer);

	//now to compress

	//write out the first character as we know we won't match
	WriteBits(0, 1);
	WriteBits(_CompressBuffer[0], 8);

	//compress everything else
	CurPos = 1;
	while(CurPos < Len)
	{
		if(!SearchForMatch(&_CompressBuffer[CurPos], CurPos, Len - CurPos, &Pos, &MatchLen))
		{
			//no match, output a character
			WriteBits(0, 1);
			WriteBits(_CompressBuffer[CurPos], 8);
			CurPos++;
		}
		else
		{
			//output our pos and match length
			WriteBits(1, 1);
			WriteBits(Pos, POS_BITS);
			WriteBits(MatchLen, LEN_BITS);

			//remember we match a minimum of 2 bytes
			CurPos += MatchLen + 2;
		}
	}

	//add our footer which is a special block with all bits set
	if(!WriteBits(1, 1))
		return 0;

	if(!WriteBits((1 << POS_BITS) - 1, POS_BITS))
		return 0;

	if(!WriteBits((1 << LEN_BITS) - 1, LEN_BITS))
		return 0;

	//if we didn't end up on a byte boundary then increment by 1 so we get all the bits
	if(_BufferBitLoc)
		_BufferByteLoc++;

	//encode the final compressed length
	EncodeSize = EncodeLength(_BufferByteLoc, _CompressBuffer);

	//now that we know how many bytes it took too encode re-encode again
	memset(_CompressBuffer, 0, EncodeSize);
	_CompressSize = EncodeLength(_BufferByteLoc + EncodeSize, _CompressBuffer);

	//if the sizes don't match then try one more time as the minor increase rolled into an additional byte
	if(_CompressSize != EncodeSize)
	{
		EncodeSize = _CompressSize;
		memset(_CompressBuffer, 0, EncodeSize);
		_CompressSize = EncodeLength(_BufferByteLoc + EncodeSize, _CompressBuffer);
	}

	//copy the final compressed data blob to the compress buffer
	memcpy(&_CompressBuffer[_CompressSize], _TempBuffer, _BufferByteLoc);

	//return how many bytes we ended up with
	_CompressSize += _BufferByteLoc;
	return _CompressSize;
}

CompressionBase *CreateLZ77(ConnectionBase *Connection, HashBase *Hash)
{
	return new LZ77(Connection, Hash);
}

__attribute__((constructor)) static void init()
{
	RegisterCompression(CreateLZ77, MAGIC_VAL('L', 'Z', '7', '7'), "LZ77");
}