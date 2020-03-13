#include <Windows.h>
#include <wincrypt.h>

#include "Base64.h"
#include "Trace.h"
#include "ServiceUtil.h"

namespace FinTP {

void ServiceUtil::changeCurrentDirectory( const std::string& moduleFileName )
{
	size_t pos = moduleFileName.find_last_of( "\\/" );
	string workingDirectory = moduleFileName.substr( 0, pos );

#ifdef EXTENDED_DEBUG
	DEBUG2("working directory: " << workingDirectory );
	TCHAR currentDirectory[MAX_PATH];
	GetCurrentDirectory( MAX_PATH, currentDirectory );
	DEBUG2("current directory: " << currentDirectory );
#endif
	if ( SetCurrentDirectory( workingDirectory.c_str() ) == 0 )
		TRACE("Failed changing current directory to working directory");
}

string ServiceUtil::decryptServiceKey( const string& configEncryptionKey )
{
	const string decodedKey = Base64::decode( configEncryptionKey );

	DATA_BLOB encrypted;
	encrypted.pbData = (BYTE *)decodedKey.c_str();
	encrypted.cbData = decodedKey.size();

	DATA_BLOB decrypted;
								
	if ( !CryptUnprotectData( &encrypted, NULL, NULL, NULL, NULL, 0, &decrypted ) )
	{
		DWORD lastError = GetLastError();
		LPSTR buffer = (LPSTR)LocalAlloc( LMEM_ZEROINIT, 512 );
		size_t size = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 512, NULL);
		try
		{
			TRACE("Cannot decrypt key. Error code: " << lastError << ". " << string( buffer, size ) )
		}
		catch ( ... )
		{
			LocalFree(buffer);
			LocalFree(decrypted.pbData);
			throw;
		}
		LocalFree(buffer);
		LocalFree(decrypted.pbData);
		return "";
	}

	string result = string( reinterpret_cast<const char*>(decrypted.pbData), decrypted.cbData );
	LocalFree(decrypted.pbData);

	return result;
}

}