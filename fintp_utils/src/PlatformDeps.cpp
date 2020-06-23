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

#include "PlatformDeps.h"
#include "StringUtil.h"
#include "Log.h"

#if defined( AIX ) || defined( LINUX ) || defined( UNIX )
	#include <sys/utsname.h>
	#include <unistd.h>
	#include <sys/param.h>
	#include <termios.h>
	
#elif defined( WIN32 )
	#include <Winsock2.h>
	#include <windows.h>
	#include <process.h>
	
#if !defined( getpid )
	#define getpid _getpid
#endif
	
	#define MAXHOSTNAMELEN 255
#endif

#include <errno.h>

#include <sstream>
#include <string>
#include <exception>
#include <stdexcept>

#include <boost/cstdint.hpp>
#include <boost/crc.hpp>

using namespace std;
using namespace FinTP;

#ifdef WIN32
	string const Path::PATH_SEPARATOR = "\\";
	string const Platform::NEWLINE_SEPARATOR = "\r\n";
#else
	string const Path::PATH_SEPARATOR = "/";
	string const Platform::NEWLINE_SEPARATOR = "\n";
#endif

boost::uint16_t Platform::m_MachineHash = 0;
string Platform::m_MachineName = "localhost";
string Platform::m_MachineUID = "localhost";

string Path::Combine( const string& path1, const string& path2 )
{
	// path1 terminated with separator
	if( path1[ path1.length() - 1 ] == Path::PATH_SEPARATOR[ 0 ] )
		return path1 + path2;

	return path1 + Path::PATH_SEPARATOR + path2;
}

string Path::GetFilename( const string& path )
{
	string::size_type indexLastSeparator;

	// find last separator
	indexLastSeparator = path.find_last_of( Path::PATH_SEPARATOR );

	// none found, return the path
	if( indexLastSeparator == string::npos )
		return path;
		
	// return the substring starting at the last separator
	return path.substr( indexLastSeparator + Path::PATH_SEPARATOR.length() ); 
}


string Path::MoveBack( const string& path )
{
	string bufferPath;
#ifdef WIN32
	bufferPath = StringUtil::Replace( path, "/", Path::PATH_SEPARATOR );
#else
	bufferPath = StringUtil::Replace( path, "\\", Path::PATH_SEPARATOR );
#endif

	StringUtil returnPath( bufferPath );
	returnPath.Split( Path::PATH_SEPARATOR );

	return returnPath.PopBackToken();
}

string Platform::GetOSName()
{
#if defined( WIN32 )
	return "WIN32";
#elif defined( AIX )
	return "AIX";
#elif defined( LINUX )
	return "LINUX";
#else 
	throw runtime_error( "Unknown OS name" );
#endif
}

string Platform::GetIp( const string& name )
{
#if defined( WIN32 )
	WSADATA wsaData;
	hostent* remoteHost;
	string remoteHostIp = "";

	try
	{
		// required call to winsock initialize
		WORD wVersionRequested;
		wVersionRequested = MAKEWORD( 2, 2 );
		if ( 0 != WSAStartup( wVersionRequested, &wsaData ) )
		{
			stringstream errorMessage;
			errorMessage << "Failed to start WinSock [" << WSAGetLastError() << "]";
			TRACE_LOG( errorMessage.str() );
			throw runtime_error( errorMessage.str() );
		}
		
		remoteHost = gethostbyname( name.c_str() );
		if ( ( remoteHost != NULL ) && ( remoteHost->h_addr_list != NULL ) && ( *( remoteHost->h_addr_list ) != NULL ) )
		{
			for( unsigned int i=0; i<4; i++ )
			{
				if ( i!=0 )
					remoteHostIp += "."; 
				unsigned int part = *( ( unsigned char * )( remoteHost->h_addr_list[ 0 ] + i ) );
				remoteHostIp += StringUtil::ToString( part );
			}
		}
		else
		{
			stringstream errorMessage;
			errorMessage << "Failed to get host by name [" << WSAGetLastError() << "]";
			
			// required call to release winsock
			if ( 0 != WSACleanup() )
			{
				TRACE_LOG( "WSACleanup failed [" << WSAGetLastError() << "]" );
			}
			throw runtime_error( errorMessage.str() );
		}
		
		// required call to release winsock
		if ( 0 != WSACleanup() )
		{
			TRACE_LOG( "WSACleanup failed [" << WSAGetLastError() << "]" );
		}
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Failed to get host by name [" << ex.what() << "]";
		throw runtime_error( errorMessage.str() );
	}
	catch( ... )
	{
		throw runtime_error( "Failed to get host by name [unknown error]" );
	}
	return remoteHostIp;
#else
	return "127.0.0.1";
#endif
}

string Platform::GetName( const string& ip )
{
#if defined( WIN32 )
	WSADATA wsaData;
	hostent* remoteHost;
	string remoteHostName = "";

	try
	{
		// required call to winsock initialize
		WORD wVersionRequested;
		wVersionRequested = MAKEWORD( 2, 2 );
		if ( 0 != WSAStartup( wVersionRequested, &wsaData ) )
		{
			stringstream errorMessage;
			errorMessage << "Failed to start WinSock [" << WSAGetLastError() << "]";
			TRACE_LOG( errorMessage.str() );
			throw runtime_error( errorMessage.str() );
		}
		
		unsigned int addr = inet_addr( ip.c_str() );
		remoteHost = gethostbyaddr( ( char * )&addr, 4, AF_INET );
		
		if ( ( remoteHost != NULL ) && ( remoteHost->h_name != NULL ) )
		{
			remoteHostName = remoteHost->h_name;
		}
		else
		{
			stringstream errorMessage;
			errorMessage << "Failed to get host by ip [" << WSAGetLastError() << "]";
			
			// required call to release winsock
			if ( 0 != WSACleanup() )
			{
				TRACE_LOG( "WSACleanup failed [" << WSAGetLastError() << "]" );
			}
			throw runtime_error( errorMessage.str() );
		}
		
		// required call to release winsock
		if ( 0 != WSACleanup() )
		{
			TRACE_LOG( "WSACleanup failed [" << WSAGetLastError() << "]" );
		}
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Failed to get host by ip [" << ex.what() << "]";
		throw runtime_error( errorMessage.str() );
	}
	catch( ... )
	{
		throw runtime_error( "Failed to get host by ip [unknown error]" );
	}
	return remoteHostName;
#else
	return "localhost";
#endif
}

string Platform::GetMachineName()
{
	if ( m_MachineName != "localhost" )
		return m_MachineName;

	// set local m_MachineName
	char name[ MAXHOSTNAMELEN ];

#if defined( WIN32 )
	WSADATA wsaData;
	try
	{
		// required call to winsock initialize
		WORD wVersionRequested;
		wVersionRequested = MAKEWORD( 2, 2 );
		if ( 0 != WSAStartup( wVersionRequested, &wsaData ) )
		{
			stringstream errorMessage;
			errorMessage << "Failed to start WinSock [" << WSAGetLastError() << "]";
			TRACE_LOG( errorMessage.str() );
			throw runtime_error( errorMessage.str() );
		}
		
		if ( gethostname( name, MAXHOSTNAMELEN ) != -1 )
			m_MachineName = name;
		else
		{
			stringstream errorMessage;
			errorMessage << "Failed to get hostname [" << WSAGetLastError() << "]";
			
			// required call to release winsock
			if ( 0 != WSACleanup() )
			{
				TRACE_LOG( "WSACleanup failed [" << WSAGetLastError() << "]" );
			}
			throw runtime_error( errorMessage.str() );
		}
		
		// required call to release winsock
		if ( 0 != WSACleanup() )
		{
			TRACE_LOG( "WSACleanup failed [" << WSAGetLastError() << "]" );
		}
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << "Failed to get hostname [" << ex.what() << "]";
		throw runtime_error( errorMessage.str() );
	}
	catch( ... )
	{
		throw runtime_error( "Failed to get hostname [unknown error]" );
	}

#elif defined( AIX ) || defined( LINUX )
	if ( gethostname( name, MAXHOSTNAMELEN ) != -1 )
		m_MachineName = name;
	else
	{
		stringstream errorMessage;
		errorMessage << "Failed to get hostname [" << sys_errlist[ errno ] << "]";
		throw runtime_error( errorMessage.str() );
	}
#endif

	return m_MachineName;
}

string Platform::GetUID()
{
	if ( m_MachineUID != "localhost" )
		return m_MachineUID;

#if defined( AIX ) || defined( LINUX )
	struct utsname uts;
	if ( uname( &uts ) < 0 )
	{
		stringstream errorMessage;
		errorMessage << "Unable to generate machine id : uname error [" << sys_errlist[ errno ] << "]";
		throw runtime_error( errorMessage.str() );
	}
	m_MachineUID = string( uts.machine );
#elif defined( WIN32 )
	m_MachineUID = GetMachineName();
#else
	throw logic_error( "Unable to generate machine id : unknown platform definition" );
#endif

	return m_MachineUID;
}

boost::uint16_t Platform::GetUIDHash()
{
	if ( m_MachineHash != 0 )
		return m_MachineHash;

	string machine = GetUID();
	boost::crc_16_type crcMac;

	crcMac.process_bytes( machine.c_str(), machine.length() );
	m_MachineHash = crcMac.checksum();
	
	return m_MachineHash;
}

string Platform::getNewLineSeparator()
{
	return NEWLINE_SEPARATOR;
}

//Process implementation
long Process::GetPID()
{
	return getpid();
}

void Platform::EnableStdEcho( bool enable )
{
#ifdef WIN32
	HANDLE standardInputHandle = GetStdHandle( STD_INPUT_HANDLE );
	DWORD lpMode;
	GetConsoleMode( standardInputHandle, &lpMode );

	if ( enable )
		lpMode |= ENABLE_ECHO_INPUT;
	else
		lpMode &= ~ENABLE_ECHO_INPUT;

	SetConsoleMode( standardInputHandle, lpMode );
#else
	struct termios tty;
	tcgetattr( STDIN_FILENO, &tty );
	if ( enable )
		tty.c_lflag |= ECHO;
	else
		tty.c_lflag &= ~ECHO;

	tcsetattr( STDIN_FILENO, TCSANOW, &tty );
#endif
}