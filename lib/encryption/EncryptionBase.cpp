#include <stdio.h>
#include <malloc.h>
#include <byteswap.h>
#include <string.h>
#include "EncryptionBase.h"

EncryptionStruct *EncryptionEntries;

#define BUFFER_SIZE 2048

EncryptionBase::EncryptionBase(ConnectionBase *Connection, uint8_t BlockSize, size_t MaxBlockPerRead) :
    ConnectionBase(Connection)
{
    _BlockSize = BlockSize;
    _BufferSize = BUFFER_SIZE;
    _InRingStart = (uint8_t *)malloc(_BufferSize);
    _InRingEnd = &_InRingStart[_BufferSize];
    _OutData = (uint8_t *)malloc(_BufferSize);
    _MaxBlocksPerRead = MaxBlockPerRead;

    ResetBuffers();
}

EncryptionBase::~EncryptionBase()
{
    _InDataStart = 0;
    _InDataEnd = 0;
    _InDecryptDataEnd = 0;

    if(_InRingStart)
    {
        memset(_InRingStart, 0, _BufferSize);
        free(_InRingStart);
        _InRingStart = 0;
    }
    if(_OutData)
    {
        memset(_OutData, 0, _BufferSize);
        free(_OutData);
    }
}

void EncryptionBase::ResetBuffers()
{
    memset(_InRingStart, 0, _BufferSize);
    memset(_OutData, 0, _BufferSize);

    _InDataStart = _InRingStart;
    _InDataEnd = _InDataStart;
    _InDecryptDataEnd = _InDataStart;
}

size_t EncryptionBase::Connect()
{
    ResetBuffers();
    return ConnectionBase::Connect();
}

size_t EncryptionBase::Disconnect()
{
    ResetBuffers();
    return ConnectionBase::Disconnect();
}

bool EncryptionBase::DataAvailableToRead()
{
    //if no data here then try below us
    if(_InDecryptDataEnd <= _InDataStart)
        return ConnectionBase::DataAvailableToRead();

    //all good
    return 1;
}

size_t EncryptionBase::Read(void *InBuffer, size_t BytesToRead)
{
    ssize_t read_len;
    size_t count = 0;
    uint8_t *Buffer = (uint8_t *)InBuffer;
    static size_t BlocksLeft = 0;

    //if there is room in the buffer for a block of data then see if there is anything to receive
    //if so then receive it and ask for it to be decrypted
    if(((_InDataEnd >= _InDataStart) && (_InDataEnd + _BlockSize) < _InRingEnd) ||
       ((_InDataEnd < _InDataStart) && (_InDataEnd + _BlockSize) < _InDataStart))
    {
        if(ConnectionBase::DataAvailableToRead())
        {
            //if we don't have any blocks left then read a single byte to determine how many blocks there are to read
            if(!BlocksLeft)
            {
                ConnectionBase::Read(&BlocksLeft, 1);

                //0 based, increment by 1
                BlocksLeft++;
            }

            //read as many blocks as allowed to help speed up decryption

            //if the end is before start then we must read up to start but not past it otherwise go to the end of the buffer
            if(_InDataEnd < _InDataStart)
                count = (_InDataStart - _InDataEnd) / _BlockSize;
            else
                count = (_InRingEnd - _InDataEnd) / _BlockSize;

            //count is now the maximum number of blocks we could read, restrict our actual read amount as needed
            if(count > BlocksLeft)
                count = BlocksLeft;

            if(count > _MaxBlocksPerRead)
                count = _MaxBlocksPerRead;

            read_len = 0;
            if(count)
                read_len = ConnectionBase::_ReadAll(_InDataEnd, _BlockSize * count);

            if(read_len)
            {
                //decrement our block left count by how many blocks we read, when it hits 0 then DecryptData knows it
                //has no more blocks left to decrypt for this chunk of blocks
                BlocksLeft -= count;

                //decrypt and step the end pointer forward based on the number of bytes returned
                //BUG - one of the algorithms will not validate the padding value correctly allowing you to return a count
                //that moves us backwards due to returning too large of a count
                count = DecryptData(read_len, !BlocksLeft);

                //dprintf(2, "_InDataEnd %016lx + %d = %016lx\n", (size_t)_InDataEnd, count, (size_t)_InDataEnd + count);
                //update our end for the data
                _InDataEnd += count;

                //only update the decrypted end if end is after start otherwise
                //start hasn't read all of the buffer yet so wait as we just stored data before start
                //due to the ring buffer
                if(_InDataEnd >= _InDataStart)
                    _InDecryptDataEnd += count;

                //if we don't have enough room for another block to decrypt then move the end to the beginning
                if((_InDataEnd + _BlockSize) >= _InRingEnd)
                    _InDataEnd = _InRingStart;
            }
        }
    }

    //decrypted end will always be after start and be the decrypted data we can return
    count = _InDecryptDataEnd - _InDataStart;


    //if we have data then move it to the buffer
    if(count)
    {
        //if we have too much data then return just what was requested
        if(count > BytesToRead)
            count = BytesToRead;

        //copy the data
        memcpy(Buffer, _InDataStart, count);
        _InDataStart += count;

        //if we hit the end of the decrypted data then reset everything to the beginning
        if(_InDataStart >= _InDecryptDataEnd)
        {
            //if the end of the incoming data is before us then continue in the ring otherwise don't move as we don't have anything left to handle
            if(_InDataEnd < _InDataStart)
            {
                _InDataStart = _InRingStart;
                _InDecryptDataEnd = _InDataEnd;
            }
        }
    }

    return count;
}

size_t EncryptionBase::Write(const void *Buffer, size_t BytesToWrite)
{
    size_t EncryptedLen;
    size_t CurLen = 0;
    size_t WriteLen;
    size_t TotalBlocks;

    //we always encrypt from the beginning of our block as we will write as much as possible out
    if(BytesToWrite >= _BufferSize)
        BytesToWrite = _BufferSize - 1;

    //if the number of bytes is larger than 256 blocks worth of data
    //then restrict to almost a full 256 blocks worth
    if((BytesToWrite / _BlockSize) >= 256)
        BytesToWrite = (_BlockSize * 256) - 1;

    //copy to our buffer
    memcpy(_OutData, Buffer, BytesToWrite);
    EncryptedLen = EncryptData(BytesToWrite);
    if(EncryptedLen == 0)
        return 0;

    //write out how many blocks we are sending, single byte
    TotalBlocks = (EncryptedLen / _BlockSize) - 1;
    WriteLen = ConnectionBase::Write(&TotalBlocks, 1);
    if(WriteLen == 0)
        return 0;

    //send the actual data
    while(CurLen < EncryptedLen)
    {
        WriteLen = ConnectionBase::Write(&_OutData[CurLen], EncryptedLen - CurLen);
        if(WriteLen == 0)
            break;

        CurLen += (size_t)WriteLen;
    };

    //return how many bytes of the original buffer we sent encrypted
    return BytesToWrite;
}

size_t EncryptionBase::SetConfig(size_t ConfigID, void *Data)
{
    return 0;
}

size_t EncryptionBase::SetConfig(size_t ConfigID, size_t Data)
{
    return SetConfig(ConfigID, (void *)Data);
}


size_t EncryptionBase::SetConfig(size_t ConfigID, const char *Data)
{
    return SetConfig(ConfigID, (void *)Data);
}

size_t EncryptionBase::EncryptData(size_t Len)
{
    return 0;
}

size_t EncryptionBase::DecryptData(size_t Len, bool LastBlock)
{
    return 0;
}

void RegisterEncryption(EncryptionClassInit Init, uint32_t ID, const char *Name)
{
    struct EncryptionStruct *NewEntry;
    char TempBuffer[sizeof(uint32_t) + 1];

    NewEntry = (struct EncryptionStruct *)malloc(sizeof(struct EncryptionStruct));
    NewEntry->ID = ID;
    NewEntry->CustomID = 0;
    NewEntry->Init = Init;
    NewEntry->Next = EncryptionEntries;
    NewEntry->Name = Name;
    EncryptionEntries = NewEntry;

    *(uint32_t *)&TempBuffer[0] = __bswap_32(ID);
    TempBuffer[sizeof(TempBuffer) - 1] = 0;
    //printf("Encryption: Registered %s: %s\n", TempBuffer, Name);
}

EncryptionClassInit GetEncryption(uint32_t ID)
{
    struct EncryptionStruct *CurEntry;
    for(CurEntry = EncryptionEntries; CurEntry; CurEntry=CurEntry->Next)
    {
        if(CurEntry->ID == ID)
            return CurEntry->Init;
    }

    return 0;
}

__attribute__((constructor)) static void init()
{
    EncryptionEntries = 0;
}