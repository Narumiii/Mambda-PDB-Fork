#pragma once

#pragma pack(push, 1)
struct DBIHeader
{
	int sig; // 0xFFFFFFFF			// 0
	int version;					// 4
	int age;						// 8
	short globalStreamIndex;		// C
	short pdbBuildNumber;			// E
	short publicStreamIndex;		// 10
	short dllVersion;				// 12
	short symbolStreamIndex;		// 14 !!!!
private:
	char _pad[ 0x40 - 0x14 ];
};

struct SymbolInfo
{
	short length;
	short magic; // 0x110E
	int flags;
	int offset;
	short section;
	char symbol[ ];
};
#pragma pack(pop)