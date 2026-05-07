#include "pbg3/Pbg3Parser.hpp"

Pbg3Parser::Pbg3Parser()
{
}

i32 Pbg3Parser::OpenArchive(const char *path)
{
    if (!this->Open(path, "rb"))
    {
        return 0;
    }
    this->fileSize = this->GetSize();
    this->Reset();
    return 1;
}

i32 Pbg3Parser::ReadBit()
{
    if (this->bitIdxInCurByte == 0)
    {
        i32 res = this->FileAbstraction::ReadByte();
        if (res == -1)
        {
            return -1;
        }
        this->curByte = (u32)res;
        this->bitIdxInCurByte = 0x80;
    }

    i32 res = (this->curByte & this->bitIdxInCurByte) != 0;
    this->bitIdxInCurByte >>= 1;
    return res;
}

u32 Pbg3Parser::ReadInt(u32 numBitsAsPowersOf2)
{
    u32 res = 0;
    for (u32 i = 0; i < numBitsAsPowersOf2; i++)
    {
        i32 bit = this->ReadBit();
        if (bit == -1)
        {
            return -1;
        }
        if (bit)
        {
            res |= (1 << (numBitsAsPowersOf2 - 1 - i));
        }
    }
    return res;
}

i32 Pbg3Parser::ReadByteAssumeAligned()
{
    this->bitIdxInCurByte = 0;
    return this->FileAbstraction::ReadByte();
}

i32 Pbg3Parser::SeekToOffset(u32 fileOffset)
{
    this->bitIdxInCurByte = 0;
    return this->Seek(fileOffset, SEEK_SET);
}

i32 Pbg3Parser::SeekToNextByte()
{
    this->bitIdxInCurByte = 0;
    return 1;
}

i32 Pbg3Parser::ReadByteAlignedData(u8 *data, u32 bytesToRead)
{
    u32 numBytesRead;
    this->bitIdxInCurByte = 0;
    return this->Read(data, bytesToRead, &numBytesRead);
}

void Pbg3Parser::Close()
{
    this->FileAbstraction::Close();
}

i32 Pbg3Parser::ReadByte()
{
    return this->FileAbstraction::ReadByte();
}

Pbg3Parser::~Pbg3Parser()
{
}
