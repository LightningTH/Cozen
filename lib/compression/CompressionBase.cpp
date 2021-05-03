#include "CompressionBase.h"
#include <malloc.h>
#include <byteswap.h>
#include <stdio.h>
#include <string.h>

CompressionStruct *CompressionEntries;

CompressionBase::CompressionBase(ConnectionBase *Connection, HashBase *Hash) :
    ConnectionBase(Connection)
{
    _CompressBuffer = 0;
    _DecompressBuffer = 0;
    _CompressSize = 0;
    _DecompressSize = 0;
    _CompressBufferSize = 0;
    _DecompressBufferSize = 0;
    _DecompressBufferPos = 0;
    _Hash = Hash;
    _DecompressHashData = (uint8_t *)malloc(_Hash->GetHashLen());
}

CompressionBase::~CompressionBase()
{
}

bool CompressionBase::DataAvailableToRead()
{
    //if we don't have data ask the layer below us
    if(_DecompressBufferPos == _DecompressSize)
        return ConnectionBase::DataAvailableToRead();

    return 1;
}

size_t CompressionBase::Read(void *InBuffer, size_t BytesToRead)
{
    size_t count = 0;

    //if they match then see if we can decompress anything
    if(_DecompressBufferPos == _DecompressSize)
    {
        while(ConnectionBase::DataAvailableToRead())
        {
            //read in data
            if(ConnectionBase::Read(&_DecompressBuffer[_DecompressSize], 1) == 0)
                continue;

            _DecompressSize++;

            //keep in sync with size until we have the full header
            _DecompressBufferPos++;

            //if we can read a header then try to read the rest of the data
            count = GetCompressSize(_DecompressSize);
            if(count)
            {
                //make sure the count appears proper
                if(count > (_DecompressBufferSize - _Hash->GetHashLen()))
                {
                    //this is an error, abort
                    close(0);
                    close(1);
                    close(2);
                    _exit(0);
                }

                //we have data to read
                ConnectionBase::_ReadAll(&_DecompressBuffer[_DecompressSize], count - _DecompressSize + _Hash->GetHashLen());

                //copy the hash off
                memcpy(_DecompressHashData, &_DecompressBuffer[count], _Hash->GetHashLen());

                //decompress it
                _DecompressBufferPos = 0;
                if(!DecompressData(count) || (_DecompressSize < 1))
                {
                    //something is wrong, reset how much data we have
                    _DecompressSize = 0;
                    ConnectionBase::_WriteString("Error decompressing\n");
                    break;
                }

                //BUG: This allows a leak from a poorly implemented compression algo
                _Hash->Reset();
                _Hash->AddData(_DecompressBuffer, _DecompressSize);
                if(!_Hash->CompareHash(_DecompressHashData))
                {
                    ConnectionBase::_WriteFString("Hash Validation failed for decompression: %s != %s\n", _Hash->GetHashAsString(), _Hash->GetHashAsString(_DecompressHashData));
                    _DecompressSize = 0;
                }

                //stop looking for data
                break;
            }
        }
    }

    //if our position does not equal the end of the decompressed buffer then we have data to provide
    count = 0;
    if(_DecompressBufferPos != _DecompressSize)
    {
        //copy over what we can
        count = BytesToRead;
        if((_DecompressSize - _DecompressBufferPos) < count)
            count = _DecompressSize - _DecompressBufferPos;

        memcpy(InBuffer, &_DecompressBuffer[_DecompressBufferPos], count);
        _DecompressBufferPos += count;

        //if they match then reset everything for next time
        if(_DecompressBufferPos == _DecompressSize)
        {
            _DecompressSize = 0;
            _DecompressBufferPos = 0;
        }
    }

    //return bytes provided
    return count;
}

size_t CompressionBase::Write(const void *Buffer, size_t BytesToWrite)
{
    size_t CurDecompressSize;
    size_t OrigBytesToWrite = BytesToWrite;
    const uint8_t *CurBuffer = (const uint8_t *)Buffer;

    //copy the data to our decompressed buffer then compress it
    //if the data is too large then we need to do it in blocks
    while(BytesToWrite)
    {
        //see how much data we can copy
        CurDecompressSize = BytesToWrite;
        if(CurDecompressSize > _CompressBufferSize)
            CurDecompressSize = _CompressBufferSize;

        //copy and indicate how much data there is
        memcpy(_CompressBuffer, CurBuffer, CurDecompressSize);

        //calculate the hash
        _Hash->Reset();
		_Hash->AddData(_CompressBuffer, CurDecompressSize);

        //compress it
        if(!CompressData(CurDecompressSize))
        {
            //failed to compress, use half of the provided length and try again
            CurDecompressSize >>= 1;

            //calculate hash
            _Hash->Reset();
            _Hash->AddData(_CompressBuffer, CurDecompressSize);

            //attempt to compress
            if(!CompressData(CurDecompressSize))
                return 0;
        }

        //if the hash won't fit then cut in half and try
        if((_CompressSize + _Hash->GetHashLen()) > _CompressBufferSize)
        {
            //failed to have room, use half of the provided length and try again
            CurDecompressSize >>= 1;

            //calculate hash
            _Hash->Reset();
            _Hash->AddData(_CompressBuffer, CurDecompressSize);

            //attempt to compress
            if(!CompressData(CurDecompressSize))
                return 0;

            //if still no room then fail
            if((_CompressSize + _Hash->GetHashLen()) > _CompressBufferSize)
                return 0;
        }

        //add the hash to the end of the compressed data
        memcpy(&_CompressBuffer[_CompressSize], _Hash->GetHash(), _Hash->GetHashLen());

        //now write our data out, if write fails we exit so we know all data had to be written
        ConnectionBase::Write(_CompressBuffer, _CompressSize + _Hash->GetHashLen());

        //move forward in the buffer
        BytesToWrite -= CurDecompressSize;
        CurBuffer += CurDecompressSize;
    }

    //return how many bytes of the original buffer we sent
    return OrigBytesToWrite;
}

size_t CompressionBase::DecompressData(size_t Len)
{
    return 0;
}

size_t CompressionBase::CompressData(size_t Len)
{
    return 0;
}

size_t CompressionBase::GetCompressSize(size_t Len)
{
    return 0;
}

void RegisterCompression(CompressionClassInit Init, uint32_t ID, const char *Name)
{
    struct CompressionStruct *NewEntry;
    char TempBuffer[sizeof(uint32_t) + 1];

    NewEntry = (struct CompressionStruct *)malloc(sizeof(struct CompressionStruct));
    NewEntry->ID = ID;
    NewEntry->CustomID = 0;
    NewEntry->Init = Init;
    NewEntry->Next = CompressionEntries;
    NewEntry->Name = Name;
    CompressionEntries = NewEntry;

    *(uint32_t *)&TempBuffer[0] = __bswap_32(ID);
    TempBuffer[sizeof(TempBuffer) - 1] = 0;
    //printf("Compression: Registered %s: %s\n", TempBuffer, Name);
}

CompressionClassInit GetCompression(uint32_t ID)
{
    struct CompressionStruct *CurEntry;
    for(CurEntry = CompressionEntries; CurEntry; CurEntry=CurEntry->Next)
    {
        if(CurEntry->ID == ID)
            return CurEntry->Init;
    }

    return 0;
}

__attribute__((constructor)) static void init()
{
    CompressionEntries = 0;
}