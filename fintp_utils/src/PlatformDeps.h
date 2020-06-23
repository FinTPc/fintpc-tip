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

#ifndef PLATFORM_DEPS_H
#define PLATFORM_DEPS_H
	
#include "DllMainUtils.h"
#include <boost/cstdint.hpp>
#include <string>
using namespace std;

namespace FinTP
{
	/// Util class for using paths in a platform independent manner
	class ExportedUtilsObject Path
	{
		public :
		
			// Path separator 
			static const string PATH_SEPARATOR;		
			
			// Combines two paths( relative + relative or absolute + relative )
			// <returns>The combine path</returns>
			static string Combine( const string& path1, const string& path2 );
			
			// Finds the filename in the specified path
			//<returns>The full filename( filename + extension )</returns>
			static string GetFilename( const string& path );

			static string MoveBack( const string& path );
	};

	class ExportedUtilsObject Convert
	{
		//private :
		//	Convert();

		public :

			template <class T>
			static T ChangeEndian( T value )
			{
				for ( size_t i = 0; i < sizeof(T)/2; i++ )
				{
					char* valueAdr = reinterpret_cast<char*>( &value );
					char temp = valueAdr[i];
					valueAdr[i] = valueAdr[ sizeof(T)-1-i ];
					valueAdr[ sizeof(T)-1-i ] = temp;
				}
				return value;
			}
	};

	class ExportedUtilsObject Platform
	{
		private :
			Platform();
		
			static string m_MachineName;
			static string m_MachineUID;
			static boost::uint16_t m_MachineHash;
			// Path separator 
			static const string NEWLINE_SEPARATOR;		

		public :

			static string GetOSName();
			static string GetMachineName();
			static string GetIp( const string& name="" );
			static string GetName( const string& ip="" );
			static string GetUID();
			static boost::uint16_t GetUIDHash();
			static string getNewLineSeparator();
			static void EnableStdEcho( bool enable );
	};

	class ExportedUtilsObject Process
	{
		private :
			Process();
			
		public :
			static long GetPID();
	};
}

#endif //PLATFORM_DEPS_H
