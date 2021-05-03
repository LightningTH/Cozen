#ifndef NEKISAHLOTH_LIB_CONNECTION_BASE
#define NEKISAHLOTH_LIB_CONNECTION_BASE

#include <unistd.h>
#include <stdint.h>

class ConnectionBase
{
	public:
		ConnectionBase(size_t inFD, size_t outFD);
		ConnectionBase(ConnectionBase *Conn);

		virtual size_t Read(void *Buffer, size_t ReadLen);
		virtual size_t Write(const void *Buffer, size_t WriteLen);
		virtual size_t SetConfig(size_t ConfigID, void *Data);
		virtual size_t Connect();
		virtual size_t Disconnect();
		virtual bool DataAvailableToRead();

		size_t ReadAll(void *Buffer, size_t ReadLen);
		size_t Readline(char *Buffer, size_t MaxLen);
		size_t WriteFString(const char *Buffer, ...);
		size_t WriteString(const char *Buffer);

	protected:
		size_t _inFD;
		size_t _outFD;
		size_t _ReadAll(void *Buffer, size_t BytesToRead);
		size_t _WriteFString(const char *Buffer, ...);
		size_t _WriteString(const char *Buffer);

	private:
		ConnectionBase *_Conn;
};

#define MAGIC_VAL(a,b,c,d) (((size_t)a << 24) | ((size_t)b << 16) | ((size_t)c << 8) | ((size_t)d))
typedef ConnectionBase *(*ConnectionClassInit)(size_t inFD, size_t outFD);
void RegisterConnection(ConnectionClassInit Init, uint32_t ID, const char *Name);
ConnectionClassInit GetConnection(uint32_t ID);

#endif
