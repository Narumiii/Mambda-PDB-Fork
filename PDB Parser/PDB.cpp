#include "PDB.hpp"
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iomanip>
#pragma comment(lib, "URLMon.lib") // fuck you im not using sockets


char * PDB7::GetPage( int index )
{
	return pdbBuffer + ( index * this->pPDBHeader->page_size );
}

int PDB7::GetStreamSize( int index )
{
	auto stream = streams.at( index );
	return stream->size( ) * this->pPDBHeader->page_size;
}

char * PDB7::GetStreamLoc( int index )
{
	auto stream = streams.at( index );
	auto pageIndex = stream->at( 0 );
	return GetPage( pageIndex );
}

StreamData * PDB7::ReadStream( int index )
{
	auto stream = streams.at( index );
	auto sizeNeeded = stream->size( ) * this->pPDBHeader->page_size;
	char * buf = new char[ sizeNeeded ];
	memset( buf, 0, sizeNeeded );

	for ( int i = 0; i < stream->size( ); i++ )
	{
		auto pageIndex = stream->at( i );
		memcpy( buf + ( i * this->pPDBHeader->page_size ), pdbBuffer + ( pageIndex * this->pPDBHeader->page_size ), this->pPDBHeader->page_size );
	}

	StreamData * pData = new StreamData( );
	pData->data = buf;
	pData->size = sizeNeeded;

	return pData;
}

int PDB7::GetPageCount( int stream_size )
{
	auto res = stream_size / this->pPDBHeader->page_size;
	if ( stream_size % this->pPDBHeader->page_size )
		res += 1;

	return res;
}

DebugInfo* PDB7::GetModuleDebugInfo( )
{
	if ( this->image_base == nullptr )
	{
		puts( "No module, are you stupid?" );
		return nullptr;
	}

	IMAGE_DOS_HEADER * pDos = ( IMAGE_DOS_HEADER* ) this->image_base;
	IMAGE_NT_HEADERS* pNt = ( IMAGE_NT_HEADERS* ) ( image_base + pDos->e_lfanew );

	this->sectionCount = pNt->FileHeader.NumberOfSections;
	this->pSections = ( IMAGE_SECTION_HEADER* ) ( ( char* ) pNt + sizeof( IMAGE_NT_HEADERS ) );

	auto pDebug = ( IMAGE_DEBUG_DIRECTORY * ) ( pNt->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_DEBUG ].VirtualAddress + image_base );

	auto pDebugInfo = ( DebugInfo* ) ( pDebug->AddressOfRawData + image_base );


	//memcpy( out, pDebugInfo->guid, sizeof( DebugInfo::guid ) );
	return pDebugInfo;
}

bool PDB7::MatchPDBToFile( const char * path )
{
	char* bin = nullptr;
	if ( path == nullptr )
		bin = this->image_base;
	else
		bin = ( char* ) LoadLibraryA( path );

	if ( this->pdbBuffer == nullptr )
	{
		puts( "No PDB to compare against" );
		return true;
	}

	IMAGE_DOS_HEADER * pDos = ( IMAGE_DOS_HEADER* ) bin;
	IMAGE_NT_HEADERS* pNt = ( IMAGE_NT_HEADERS* ) ( bin + pDos->e_lfanew );

	this->sectionCount = pNt->FileHeader.NumberOfSections;
	this->pSections = ( IMAGE_SECTION_HEADER* ) ( ( char* ) pNt + sizeof( IMAGE_NT_HEADERS ) );

	auto pDebug = ( IMAGE_DEBUG_DIRECTORY * ) ( pNt->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_DEBUG ].VirtualAddress + bin );
	auto pDebugInfo = ( DebugInfo* ) ( pDebug->AddressOfRawData + bin );

	struct Stream1
	{
		int ver;
		int date;
		int age;
		GUID guid;
	};

	auto pInfoStream = ( Stream1* ) GetStreamLoc( GUIDStream );
	auto guid1 = pDebugInfo->guid;
	auto guid2 = pInfoStream->guid;

	if ( guid1.Data1 != guid2.Data1 )
		return false;

	if ( guid1.Data2 != guid2.Data2 )
		return false;

	if ( guid1.Data3 != guid2.Data3 )
		return false;

	return !memcmp( guid1.Data4, guid2.Data4, sizeof( guid1.Data4 ) );
}

bool PDB7::SetModule( char * base )
{
	if ( base == nullptr )
		return true; // do your thing dipshit

	this->image_base = base;
	auto res = this->MatchPDBToFile( );
	if ( !res )
	{
		this->image_base = 0;
		this->pSections = 0;
		this->sectionCount = 0;
	}

	return res;
}

void PDB7::DumpStreams( )
{
	for ( int i = 0; i < this->pRootStream->num_streams; i++ )
	{
		auto pData = ReadStream( i );
		char name[ 1000 ] = { 0 };
		auto dirCreate = CreateDirectoryA( "./Dumps/", 0 );
		if ( !dirCreate && GetLastError( ) != ERROR_ALREADY_EXISTS )
		{
			printf_s( "Failed to create dir ((((\n" );
			getchar( );
			return;
		}
		sprintf_s( name, "Dumps/stream_%d.bin", i );
		std::ofstream of( name, std::ios::binary );
		of.write( pData->data, pData->size );
		delete pData->data;
		delete pData;
		of.close( );
	}
}

bool PDB7::DownloadPDB( char * modName, char * dlPath )
{
	auto pDebugInfo = this->GetModuleDebugInfo( );
	if ( !pDebugInfo )
	{
		puts( "No debug info, feelsbad?" );
		return false;
	}

	auto guid = &pDebugInfo->guid;

	char * ctx = 0;
	char* module_name = nullptr;

	if ( !_stricmp( "ntoskrnl.exe", modName ) )
	{
		// there are 4 possible names, ntoskrnl, ntkrnlmp, ntkrnlpa, and ntkrpamp. Loop through all to find the PDB as theyre all called NTOSKRNL
		const char *names[ ] = { "ntoskrnl", "ntkrnlmp", "ntkrnlpa", "ntkrpamp" };
		for ( int i = 0; i < sizeof( names ) / sizeof( char* ); i++ )
		{
			auto curName = names[ i ];
			printf_s( "Attempting kernel file name %s\n", curName );

			std::stringstream urlStream;
			urlStream << "http://msdl.microsoft.com/download/symbols/" << curName << ".pdb/";

			// blatantly ripped from drew0709's pdbparse.cpp cause fuck GUIDs
			//https://github.com/drew0709/pdbparse/blob/master/pdbparse/pdbparse.cpp#L74

			urlStream << std::setfill( '0' ) << std::setw( 8 ) << std::hex << guid->Data1 << std::setw( 4 ) << std::hex << guid->Data2 << std::setw( 4 ) << std::hex << guid->Data3;
			for ( const auto i : guid->Data4 )
				urlStream << std::setw( 2 ) << std::hex << +i;
			urlStream << pDebugInfo->age;

			urlStream << "/" << curName << ".pdb";
			// download this to ./modName.pdb

			puts( "Attempting to download kernel file, be patient." );
			if ( URLDownloadToFileA( 0, urlStream.str( ).c_str( ), dlPath ? dlPath : "ntoskrnl.pdb", 0, 0 ) )
			{
				printf_s( "Failed to download using url %s\nError %d\n", urlStream.str( ).c_str( ), GetLastError( ) );
				continue;
			}
			else
				return true;
		}
	}
	else
	{
		module_name = strtok_s( modName, ".", &ctx );

		// format: http://msdl.microsoft.com/download/symbols/pdbname.pdb/guid/pdbname.pdb
		// example: https://msdl.microsoft.com/download/symbols/ntkrnlmp.pdb/ F2C39CCB E477FA99 A815 CE04EC327B061 /ntkrnlmp.pdb for an ntkrnlmp.pdb with GUID of that F2 shit

		std::stringstream urlStream;
		urlStream << "http://msdl.microsoft.com/download/symbols/" << module_name << ".pdb/";

		// blatantly ripped from drew0709's pdbparse.cpp cause fuck GUIDs
		//https://github.com/drew0709/pdbparse/blob/master/pdbparse/pdbparse.cpp#L74

		urlStream << std::setfill( '0' ) << std::setw( 8 ) << std::hex << guid->Data1 << std::setw( 4 ) << std::hex << guid->Data2 << std::setw( 4 ) << std::hex << guid->Data3;
		for ( const auto i : guid->Data4 )
			urlStream << std::setw( 2 ) << std::hex << +i;
		urlStream << pDebugInfo->age;

		urlStream << "/" << module_name << ".pdb";
		// download this to ./modName.pdb

		char name[ 100 ] = { 0 };
		sprintf_s( name, "%s.pdb", module_name );

		puts( "Attempting to download file, be patient." );
		if ( URLDownloadToFileA( 0, urlStream.str( ).c_str( ), dlPath ? dlPath : name, 0, 0 ) )
		{
			printf_s( "Failed to download using url %s\nError %d\n", urlStream.str( ).c_str( ), GetLastError( ) );
			return false;
		}

		return true;
	}

	return false;
}

void PDB7::DumpSymbols( )
{
	SymbolInfo* pSymbolInfo = nullptr;

	std::ofstream of( "sym_dump.txt" );
	std::stringstream sstream;

	for ( char * base = pSymbolStream->data; base < ( pSymbolStream->data + pSymbolStream->size ); base += pSymbolInfo->length + 2 )
	{
		pSymbolInfo = ( SymbolInfo* ) base;
		if ( pSymbolInfo->magic != 0x110E )	// not a symbol (
			break; // no more symbols left

		sstream << pSymbolInfo->symbol << "\n";
	}

	of.write( sstream.str( ).c_str( ), sstream.str( ).length( ) );

	return;
}


char* PDB7::FindSymbol( const char * sym )
{
	SymbolInfo* pSymbolInfo = nullptr;

	for ( char * base = pSymbolStream->data; base < ( pSymbolStream->data + pSymbolStream->size ); base += pSymbolInfo->length + 2 )
	{
		pSymbolInfo = ( SymbolInfo* ) base;
		if ( pSymbolInfo->magic != 0x110E )	// not a symbol (
			break; // no more symbols left

		if ( !_stricmp( pSymbolInfo->symbol, sym ) )
		{
			if ( this->image_base )
			{
				if ( pSymbolInfo->section > this->sectionCount )
				{
					printf_s( "Section of symbol non-existent, check this shit out!\n" );
					printf_s( "%s found at section %02X:%08X", pSymbolInfo->section, pSymbolInfo->offset );
				}
				else
				{
					auto secBase = this->pSections[ pSymbolInfo->section - 1 ].VirtualAddress;
					return ( secBase + pSymbolInfo->offset + this->image_base );
				}
			}
			else
			{
				printf_s( "No module loaded, can only give section:offset\n" );
				printf_s( "%s found at section %02X:%08X\n", sym, pSymbolInfo->section, pSymbolInfo->offset );
			}
		}
	}


	return 0;
}

PDB7::PDB7( char * pdb_path )
{
	std::ifstream is( pdb_path, std::ios::binary | std::ios::ate );
	size_t len = is.tellg( );
	pdbBuffer = new char[ len + 1 ];
	memset( pdbBuffer, 0, len + 1 );
	is.seekg( 0 );
	is.read( pdbBuffer, len );
	is.close( );

	this->pPDBHeader = ( PDBHeader7* ) this->pdbBuffer;


	struct RootPageNumList
	{
		int nums[ ];
	};

	RootPageNumList * list = ( decltype( list ) ) GetPage( this->pPDBHeader->root_stream_page_number_list_number );
	pRootStream = ( RootStream7* ) GetPage( list->nums[ 0 ] );

	int * streamSizes = new int[ pRootStream->num_streams ];
	memset( streamSizes, 0, pRootStream->num_streams );

	int curPageNum = 0;

	for ( int i = 0; i < pRootStream->num_streams; i++ )
	{
		auto curSize = pRootStream->stream_sizes[ i ];
		streamSizes[ i ] = curSize != 0xFFFFFFFF ? curSize : 0;

		//Log( "Stream %d size: %X", i, streamSizes[ i ] );
		auto neededPages = this->GetPageCount( streamSizes[ i ] );
		//Log( "Need %d pages", neededPages );

		std::vector< int >* nums = new std::vector<int>( );

		for ( int x = 0; x < neededPages; x++ )
		{
			auto curNum = pRootStream->stream_sizes[ pRootStream->num_streams + curPageNum++ ];
			//Log( "Page number for Stream %d: %x", i, curNum );
			nums->push_back( curNum );
		}

		streams.insert( { i, nums } );
	}

	pDBIHeader = ( DBIHeader* ) GetStreamLoc( DBIStream );
	pSymbolStream = ReadStream( pDBIHeader->symbolStreamIndex );
}

PDB7::PDB7( )
{ }
