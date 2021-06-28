#include "Generics.hpp"
#include "PDB.hpp"
	// ASSUMIGN THEYRE ALL VERSION 7


void main( int argc, char * argv[ ] )
{
	if ( argc < 3 )
	{
		printf_s( "Usage: %s <pdb_path> <symbol_name> <module_path>(optional)\n", argv[ 0 ] );
		puts( "Or, supply only a module path and the word 'download' to attempt to retrieve a PDB from microsoft symbol servers" );
		return;
	}

	if ( !_stricmp( argv[ 2 ], "download" ) )
	{
		printf_s( "Attempting to download PDB for %s\n", argv[ 1 ] );
		PDB7 * pPDB = new PDB7( );
		char * mod = ( char* ) LoadLibraryA( argv[ 1 ] );
		pPDB->SetModule( mod );
		char GUID[ 16 ] = { 0 };
		if ( !pPDB->DownloadPDB( argv[ 1 ] ) )
		{
			puts( "Failed to download PDB for binary, sucks to be you!" );
			return;
		}
		else
		{
			puts( "PDB downloaded to local directory, run me again!" );
			return;
		}
	}
	else if ( !_stricmp( argv[ 2 ], "dump" ) )
	{
		printf_s( "Dumping symbols to a file.\n" );
		PDB7 * pPDB = new PDB7( argv[ 1 ] );
		pPDB->DumpSymbols( );
		return;
	}

	PDB7 * pPDB = new PDB7( argv[ 1 ] );
	char * module = 0;
	if ( argc == 4 )
		module = ( char* ) LoadLibraryA( argv[ 3 ] );

	//PDB7 * pPDB = new PDB7( ( char* ) "ntkrnlmp.pdb" );
	//auto module = ( char* ) LoadLibraryA( "ntkrnlmp.exe" );

	//pPDB->FindSymbol( argv[ 2 ] ); // searching without a module cannot give you an exact address

	if ( pPDB->SetModule( module ) )
	{
		// parse/dump!
		printf_s( "Module matches PDB!\n" );

		auto addr = pPDB->FindSymbol( argv[ 2 ] );
		printf_s( "Found %s at %p\n", argv[2], addr );
	}
	else
	{
		// hang yourself!
		printf_s( "Module doesn't match PDB!\n" );
	}
}