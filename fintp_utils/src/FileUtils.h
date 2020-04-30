/*
* FinTP - Financial Transactions Processing Application
* Copyright (C) 2013 Business Information Systems (Allevo) S.R.L.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>
* or contact Allevo at : 031281 Bucuresti, 23C Calea Vitan, Romania,
* phone +40212554577, office@allevo.ro <mailto:office@allevo.ro>, www.allevo.ro.
*/

#ifndef FILEUTILS_H
#define FILEUTILS_H
	
#include "DllMainUtils.h"
#include "curl/curl.h"

#include <string>
using namespace std;

namespace FinTP
{
	/// Util class for renaming files & more
	class ExportedUtilsObject FileUtils
	{
        private :

			struct uploadBuffer
			{
				const char *m_ReadPtr;
				size_t m_SizeLeft;
			};
			typedef uploadBuffer uploadBuffer;

            static size_t readCallbackMemory( void *data, size_t size, size_t nmemb, void *readBuffer );

		public :

			static void RenameFile( const string& sourceFilename, string& destinationFilename );
			//TODO: static void RemoveFile(...) 10 uses , CopyFile(...)
			//static void CopyFile( const string& sourceFilename, string& destinationFilename);
            
			/**
            Upload file to server using SSH Protocol (PORT 22)
            Accept: SFTP*/
            static void SecureUploadFile( const string& uri, const string& buffer );


	};
}

#endif //FILEUTILS_H
