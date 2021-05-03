#include <stdio.h>
#include <malloc.h>
#include <byteswap.h>
#include <stdarg.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include "ConnectionBase.h"

struct ConnectionStruct {
    struct ConnectionStruct *Next;
    ConnectionClassInit Init;
    uint32_t ID;
    const char *Name;
};

ConnectionStruct *ConnectionEntries;

ConnectionBase::ConnectionBase(ConnectionBase *Conn)
{
    _inFD = Conn->_inFD;
    _outFD = Conn->_outFD;
    _Conn = Conn;
}

ConnectionBase::ConnectionBase(size_t inFD, size_t outFD)
{
    _inFD = inFD;
    _outFD = outFD;
    _Conn = 0;
}

size_t ConnectionBase::Connect()
{
    if(_Conn)
        return _Conn->Connect();

    return 0;
}

size_t ConnectionBase::Disconnect()
{
    if(_Conn)
        return _Conn->Disconnect();

    return 0;
}

size_t ConnectionBase::Read(void *Buffer, size_t BytesToRead)
{
    if(_Conn)
        return _Conn->Read(Buffer, BytesToRead);

    ssize_t ReadLen;

    ReadLen = read(_inFD, Buffer, BytesToRead);
    if(ReadLen <= -1)
    {
        close(0);
        close(1);
        close(2);
        _exit(0);
    }

    return ReadLen;
}

size_t ConnectionBase::Write(const void *Buffer, size_t BytesToWrite)
{
    size_t CurLen = 0;
    ssize_t WriteLen;
    const uint8_t *OutBuffer = (const uint8_t *)Buffer;

    while(CurLen < BytesToWrite)
    {
        if(_Conn)
            WriteLen = _Conn->Write(&OutBuffer[CurLen], BytesToWrite - CurLen);
        else
            WriteLen = write(_outFD, &OutBuffer[CurLen], BytesToWrite - CurLen);

        if(WriteLen <= -1)
        {
            close(0);
            close(1);
            close(2);
            _exit(0);
        }

        CurLen += (size_t)WriteLen;
    };

    return CurLen;
}

size_t ConnectionBase::SetConfig(size_t ConfigID, void *Data)
{
    if(_Conn)
        return _Conn->SetConfig(ConfigID, Data);

    return 0;
}

bool ConnectionBase::DataAvailableToRead()
{
    if(_Conn)
        return _Conn->DataAvailableToRead();

    fd_set fdset;
    timeval timeout;

    FD_ZERO(&fdset);
    FD_SET(_inFD, &fdset);

    //see if we have any data to read
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    if(select(1, &fdset, 0, 0, &timeout))
        return 1;

    return 0;    
}

size_t ConnectionBase::_ReadAll(void *Buffer, size_t BytesToRead)
{
    if(_Conn)
        return _Conn->ReadAll(Buffer, BytesToRead);

    return ReadAll(Buffer, BytesToRead);
}

size_t ConnectionBase::ReadAll(void *Buffer, size_t BytesToRead)
{
    ssize_t ReadLen;
    size_t TotalRead;

    TotalRead = 0;
    while(BytesToRead)
    {
        ReadLen = Read(&((char *)Buffer)[TotalRead], BytesToRead);
        if(ReadLen > 0)
        {
            TotalRead += ReadLen;
            BytesToRead -= ReadLen;
        }
    }

    return TotalRead;
}

size_t ConnectionBase::Readline(char *Buffer, size_t MaxLen)
{
    size_t CurLen;
    char ReadChar;

    for(CurLen = 0; CurLen < MaxLen; CurLen++)
    {
        //if we fail to read anything then exit
        if(ReadAll(&ReadChar, 1) <= 0)
        {
            close(0);
            close(1);
            close(2);
            _exit(0);
        }

        if(ReadChar == '\n')
        {
            Buffer[CurLen] = 0;
            break;
        }

        //skip carriage return
        if(ReadChar == '\r')
            continue;

        Buffer[CurLen] = ReadChar;
    }

    //return how much we read
    return CurLen;
}

size_t ConnectionBase::_WriteFString(const char *Buffer, ...)
{
    va_list argp;
    char *new_buffer;
    size_t ret;

    new_buffer = (char *)malloc(1024);

    va_start(argp, Buffer);
    vsnprintf(new_buffer, 1024, Buffer, argp);
    va_end(argp);

    ret = _WriteString(new_buffer);
    free(new_buffer);
    return ret;
}

size_t ConnectionBase::WriteFString(const char *Buffer, ...)
{
    va_list argp;
    char *new_buffer;
    size_t ret;

    new_buffer = (char *)malloc(1024);

    va_start(argp, Buffer);
    vsnprintf(new_buffer, 1024, Buffer, argp);
    va_end(argp);

    ret = WriteString(new_buffer);
    free(new_buffer);
    return ret;
}

size_t ConnectionBase::_WriteString(const char *Buffer)
{
    if(_Conn)
        return _Conn->WriteString(Buffer);

    return WriteString(Buffer);
}

size_t ConnectionBase::WriteString(const char *Buffer)
{
    return Write(Buffer, strlen(Buffer));
}

void RegisterConnection(ConnectionClassInit Init, uint32_t ID, const char *Name)
{
    struct ConnectionStruct *NewEntry;
    char TempBuffer[sizeof(uint32_t) + 1];

    NewEntry = (struct ConnectionStruct *)malloc(sizeof(struct ConnectionStruct));
    NewEntry->ID = ID;
    NewEntry->Init = Init;
    NewEntry->Next = ConnectionEntries;
    NewEntry->Name = Name;
    ConnectionEntries = NewEntry;

    *(uint32_t *)&TempBuffer[0] = __bswap_32(ID);
    TempBuffer[sizeof(TempBuffer) - 1] = 0;
    //printf("Connection: Registered %s: %s\n", TempBuffer, Name);
}

ConnectionClassInit GetConnection(uint32_t ID)
{
    struct ConnectionStruct *CurEntry;
    for(CurEntry = ConnectionEntries; CurEntry; CurEntry=CurEntry->Next)
    {
        if(CurEntry->ID == ID)
            return CurEntry->Init;
    }

    return 0;
}

__attribute__((constructor)) static void init()
{
    ConnectionEntries = 0;
}