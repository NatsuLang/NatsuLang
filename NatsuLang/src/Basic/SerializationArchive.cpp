#include "Basic/SerializationArchive.h"

using namespace NatsuLib;
using namespace NatsuLang;

ISerializationArchiveReader::~ISerializationArchiveReader()
{
}

nBool ISerializationArchiveReader::ReadBool(nStrView key, nBool& out)
{
	nuLong dummy;
	const auto ret = ReadInteger(key, dummy, 1);
	out = !!dummy;
	return ret;
}

ISerializationArchiveWriter::~ISerializationArchiveWriter()
{
}

void ISerializationArchiveWriter::WriteBool(nStrView key, nBool value)
{
	WriteInteger(key, value, 1);
}
