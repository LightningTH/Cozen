#ifndef NEKISAHLOTH_LIB_CRC32
#define NEKISAHLOTH_LIB_CRC32

#include "HashBase.h"

class Crc32 : public HashBase
{
	public:
		Crc32();
		~Crc32(){}

		void Reset();
		size_t AddData(void *Buffer, size_t Len);

	private:

};

#endif
