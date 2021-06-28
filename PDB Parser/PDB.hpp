#pragma once
#include <Windows.h>
#include "RootStream.hpp"
#include "DBIHeader.hpp"
#include <map>
#include <vector>

enum ImportantStreams
{
	GUIDStream = 1,
	DBIStream = 3, // Contains structure which tells you what stream symbols are found (offset 20)
};

// ASSUMING VERSION 7
struct PDBHeader7
{
	//const char *signature = "Microsoft C/C++ MSF 7.00\r\n\x1A""DS\0\0\0";
	char signature[ 0x20 ];
	int page_size;
	int allocation_table_pointer;
	int file_page_count;
	int root_stream_size;
	int reserved;
	int root_stream_page_number_list_number;// ROLF?
};

struct StreamData
{
	char * data;
	int size;
};

struct DebugInfo
{
	char magic[ 4 ]; // RSDS
	GUID guid;
	int age;
	char pdb_path[ ];
};


class PDB7
{
	std::map< int, std::vector<int>*> streams;

	DBIHeader * pDBIHeader = nullptr;
	StreamData* pSymbolStream = nullptr;
	char * image_base; // base of image relating to PDB
	int sectionCount = 0;
	IMAGE_SECTION_HEADER * pSections = nullptr;
	PDBHeader7* pPDBHeader = nullptr;
	RootStream7* pRootStream = nullptr;

	char * pdbBuffer = nullptr;
	char * GetPage( int index );
	int GetStreamSize( int index );
	char * GetStreamLoc( int index );
	StreamData * ReadStream( int index );
	int GetPageCount( int stream_size );
	DebugInfo* GetModuleDebugInfo( );
public:
	bool MatchPDBToFile( const char * path = nullptr );
	bool SetModule( char * base );		// to get absolute address of symbols. Fails if module doesnt match PDB
	void DumpStreams( );
	void DumpSymbols( );
	bool DownloadPDB( char * modname, char * dlPath = nullptr ); // downloads pdb matching image_base from microsoft symbol servers. DLPath = full path .exe
	char* FindSymbol( const char * sym );
	PDB7( char * pdb_path );
	PDB7( );
};