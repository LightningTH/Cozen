#ifndef NEKISAHLOTH_LIB_ENCRYPTION_BASE
#define NEKISAHLOTH_LIB_ENCRYPTION_BASE

#include <unistd.h>
#include <stdint.h>
#include "ConnectionBase.h"

class EncryptionBase : public ConnectionBase
{
	public:
		typedef enum EncryptionConfigEnum : size_t
		{
			CONFIG_KEY = 0,
			CONFIG_IV = 1,
			CONFIG_BITS = 2,
			CONFIG_MODE = 3
		} EncryptionConfigEnum;

		typedef enum TwofishMode : size_t
		{
			ENCRYPTION_MODE_CBC = 0,
			ENCRYPTION_MODE_ECB,
			ENCRYPTION_MODE_CFB1
		} EncryptionModeEnum;

		#define READ_MAX_BLOCKS (size_t)-1
		virtual ~EncryptionBase();

		size_t Read(void *Buffer, size_t ReadLen);
		size_t Write(const void *Buffer, size_t WriteLen);
		bool DataAvailableToRead();
		virtual size_t SetConfig(size_t ConfigID, void *Data);
		size_t SetConfig(size_t ConfigID, size_t Data);
		size_t SetConfig(size_t ConfigID, const char *Data);
		size_t Connect();
		size_t Disconnect();

		EncryptionBase(ConnectionBase *Connection, uint8_t BlockSize, size_t MaxBlocksPerRead);
	protected:

		//it is assumed a call to these functions will return the length of data
		//operated on and that anything after in the buffer can be moved to the beginning
		//of the buffer
		virtual size_t EncryptData(size_t Len);
		virtual size_t DecryptData(size_t Len, bool LastBlock);

		//in and out buffers for encryption and decryption, to be done "in place"
		uint8_t *_InRingStart;
		uint8_t *_OutData;

		//minimum block size for reading or writing encrypted data
		uint8_t _BlockSize;
		size_t _BufferSize;
		size_t _MaxBlocksPerRead;

		//start and end positions for the data allowing for a ring buffer to be used
		//we have to track where the data starts and ends insid of the ring buffer along with the end of the buffer
		//and where the encrypted or decrypted data ends which may differ from where the end pointer is for where to write new data
		uint8_t *_InDataStart, *_InDataEnd, *_InRingEnd, *_InDecryptDataEnd;

	private:
		void ResetBuffers();
};

#define MAGIC_VAL(a,b,c,d) (((size_t)a << 24) | ((size_t)b << 16) | ((size_t)c << 8) | ((size_t)d))
typedef EncryptionBase *(*EncryptionClassInit)(ConnectionBase *Connection);
void RegisterEncryption(EncryptionClassInit Init, uint32_t ID, const char *Name);
EncryptionClassInit GetEncryption(uint32_t ID);

typedef struct EncryptionStruct {
    struct EncryptionStruct *Next;
    EncryptionClassInit Init;
    uint32_t ID;
	uint32_t CustomID;
    const char *Name;
} EncryptionStruct;

extern EncryptionStruct *EncryptionEntries;

#endif
