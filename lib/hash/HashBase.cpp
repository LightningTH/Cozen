#include "HashBase.h"
#include <malloc.h>
#include <byteswap.h>
#include <string.h>

HashStruct *HashEntries;

HashBase::HashBase(size_t HashLen)
{
    _HashLen = HashLen;
    _HashData = (uint8_t *)malloc(HashLen);
    _InternalHashStr = 0;
    _ExternalHashStr = 0;
    memset(_HashData, 0, _HashLen);
}

HashBase::~HashBase()
{
    free(_HashData);
}

uint8_t *HashBase::GetHash()
{
    return _HashData;

}
size_t HashBase::GetHashLen()
{
    return _HashLen;
}

void HashBase::Reset()
{
    memset(_HashData, 0, _HashLen);
}

size_t HashBase::AddData(void *Buffer, size_t Len)
{
    return 0;
}

bool HashBase::CompareHash(void *HashBuffer)
{
    //make sure the hash is written to the buffer
    GetHash();
    return (memcmp(HashBuffer, _HashData, _HashLen) == 0);
}

bool HashBase::CompareHashAsString(char *HashBuffer)
{
    //make sure the hash is written to the buffer
    GetHashAsString();
    return (strncasecmp(_InternalHashStr, HashBuffer, _HashLen*2) == 0);
}

const char *HashBase::GetHashAsString()
{
    //make sure it is complete
    GetHash();

    if(!_InternalHashStr)
        _InternalHashStr = (char *)malloc((_HashLen * 2) + 1);

    return _ConvertToString(_InternalHashStr, _HashData);
}

const char *HashBase::GetHashAsString(void *HashBuffer)
{
    if(!_ExternalHashStr)
        _ExternalHashStr = (char *)malloc((_HashLen * 2) + 1);

    return _ConvertToString(_ExternalHashStr, HashBuffer);
}

char *HashBase::_ConvertToString(char *CharBuffer, void *HashBuffer)
{
    size_t i;
    uint8_t CurChar;

    for(i = 0; i < _HashLen; i++)
    {
        CurChar = ((uint8_t *)HashBuffer)[i];
        CharBuffer[i*2] = (CurChar >> 4) + 0x30;
        CharBuffer[(i*2) + 1] = (CurChar & 0xF) + 0x30;

        if(CharBuffer[i*2] > 0x39)
            CharBuffer[i*2] += 7;

        if(CharBuffer[(i*2) + 1] > 0x39)
            CharBuffer[(i*2) + 1] += 7;
    }
    CharBuffer[(i*2)] = 0;

    return CharBuffer;
}

void RegisterHash(HashClassInit Init, uint32_t ID, const char *Name)
{
    struct HashStruct *NewEntry;
    char TempBuffer[sizeof(uint32_t) + 1];

    NewEntry = (struct HashStruct *)malloc(sizeof(struct HashStruct));
    NewEntry->ID = ID;
    NewEntry->Init = Init;
    NewEntry->Next = HashEntries;
    NewEntry->Name = Name;
    HashEntries = NewEntry;

    *(uint32_t *)&TempBuffer[0] = __bswap_32(ID);
    TempBuffer[sizeof(TempBuffer) - 1] = 0;
    //printf("Hash: Registered %s: %s\n", TempBuffer, Name);
}

HashClassInit GetHash(uint32_t ID)
{
    struct HashStruct *CurEntry;
    for(CurEntry = HashEntries; CurEntry; CurEntry=CurEntry->Next)
    {
        if(CurEntry->ID == ID)
            return CurEntry->Init;
    }

    return 0;
}

__attribute__((constructor)) static void init()
{
    HashEntries = 0;
}