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

#include <fstream>
#include <string>
#include <iostream>
#include <sstream>

#include "Collections.h"
#include "Trace.h"
#include "StringUtil.h"

#ifdef WIN32	
	#define __MSXML_LIBRARY_DEFINED__
		
	#include <windows.h>
	//#include <dbghelp.h>
	#include <crtdbg.h>
	#include <malloc.h> 
	//#include <tlhelp32.h>
	
	#pragma comment (lib, "dbghelp")

	//std::ofstream cnull( "debugOutput.txt" );
#else
	#include "PlatformDeps.h"
	//std::ofstream cnull( "/dev/null" );
#endif

using namespace FinTP;

pthread_mutex_t FileOutputter::m_OutputtersSyncMutex = PTHREAD_MUTEX_INITIALIZER;
FileOutputter FileOutputter::m_Instance;
bool FileOutputter::m_Terminated = false;
string FileOutputter::Prefix = "Unnamed";
pthread_once_t FileOutputter::KeysCreate = PTHREAD_ONCE_INIT;
pthread_key_t FileOutputter::StreamKey, FileOutputter::LinesKey, FileOutputter::NamesKey, FileOutputter::ExtraFilesKey;
unsigned long FileOutputter::MaxLines = 0;
unsigned long FileOutputter::MaxExtraFiles = 0;

/*std::string DEBUG_INDENTATION( int tabs )
{
	stringstream returnString;
	for( int i=0; i<tabs; i++ )
		returnString << "\t";
	return returnString.str();
}*/

FileOutputter::FileOutputter()
{
	int onceResult = pthread_once( &FileOutputter::KeysCreate, &FileOutputter::CreateKeys );
	if ( 0 != onceResult )
	{
		cerr << "One time key creation for FileOutputter threads failed [" << onceResult << "]";
	}
}

FileOutputter::~FileOutputter()
{
}

void FileOutputter::CreateKeys()
{
	cout << "Thread [" << pthread_self() << "] creating log keys..." << endl;
	int keyCreateResult = pthread_key_create( &FileOutputter::StreamKey, &FileOutputter::DeleteStreams );
	if ( 0 != keyCreateResult )
	{
		cerr << "Unable to create thread key FileOutputter::StreamKey [" << keyCreateResult << "]";
	}
	keyCreateResult = pthread_key_create( &FileOutputter::LinesKey, &FileOutputter::DeleteLines );
	if ( 0 != keyCreateResult )
	{
		cerr << "Unable to create thread key FileOutputter::LinesKey [" << keyCreateResult << "]";
	}
	keyCreateResult = pthread_key_create( &FileOutputter::NamesKey, &FileOutputter::DeleteNames );
	if ( 0 != keyCreateResult )
	{
		cerr << "Unable to create thread key FileOutputter::NamesKey [" << keyCreateResult << "]";
	}
	keyCreateResult = pthread_key_create( &FileOutputter::ExtraFilesKey, &FileOutputter::DeleteExtraFiles );
	if ( 0 != keyCreateResult )
	{
		cerr << "Unable to create thread key FileOutputter::LinearLogsKey [" << keyCreateResult << "]";
	}
}

void FileOutputter::DeleteStreams( void* data )
{
	//cout << "Thread [" << pthread_self() << "] deleting log keys..." << endl;
	
	ofstream* threadStream = ( ofstream* )data;
	if ( threadStream != NULL )
	{
		if ( threadStream->is_open() )
			threadStream->close();
		delete threadStream;
	}
	int setSpecificResult = pthread_setspecific( FileOutputter::StreamKey, NULL );
	if ( 0 != setSpecificResult )
	{
		cerr << "Set thread specific StreamKey failed [" << setSpecificResult << "]";
	}
}

void FileOutputter::DeleteLines( void* data )
{
	//cout << "Thread [" << pthread_self() << "] deleting log keys..." << endl;
	
	unsigned long * threadLines = ( unsigned long * )data;
	if ( threadLines != NULL )
		delete threadLines;
	
	int setSpecificResult = pthread_setspecific( FileOutputter::LinesKey, NULL );
	if ( 0 != setSpecificResult )
	{
		cerr << "Set thread specific LinesKey failed [" << setSpecificResult << "]";
	}
}

void FileOutputter::DeleteNames( void* data )
{
	//cout << "Thread [" << pthread_self() << "] deleting log keys..." << endl;
	
	string * threadNames = ( string * )data;
	if ( threadNames != NULL )
		delete threadNames;
	
	int setSpecificResult = pthread_setspecific( FileOutputter::NamesKey, NULL );
	if ( 0 != setSpecificResult )
	{
		cerr << "Set thread specific LinesKey failed [" << setSpecificResult << "]";
	}
}

void FileOutputter::DeleteExtraFiles( void* data )
{
	//cout << "Thread [" << pthread_self() << "] deleting log keys..." << endl;
	
	unsigned long * threadLogs = ( unsigned long * )data;
	if ( threadLogs != NULL )
		delete threadLogs;
	
	int setSpecificResult = pthread_setspecific( FileOutputter::ExtraFilesKey, NULL );
	if ( 0 != setSpecificResult )
	{
		cerr << "Set thread specific LinesKey failed [" << setSpecificResult << "]";
	}
}

ostream& FileOutputter::getStream() 
{
	//return getInstance()->internalGetStream();
	ofstream* threadStream = ( ofstream* )pthread_getspecific( FileOutputter::StreamKey );
	if ( threadStream != NULL )
	{
		// reset log
		if ( MaxLines > 0 )
		{
			// get no of lines
			unsigned long *outputSofar = ( unsigned long * )pthread_getspecific( FileOutputter::LinesKey );

			if ( *outputSofar > MaxLines )
			{
				string *streamName = ( string * )pthread_getspecific( FileOutputter::NamesKey );
				//TODO check seek .. it may work
				if ( threadStream->is_open() )
					threadStream->close();

				if ( MaxExtraFiles > 0 )
				{
					unsigned long* logIndex = ( unsigned long* ) pthread_getspecific( FileOutputter::ExtraFilesKey );
					stringstream oldExt;
					( *logIndex == 0 ) ? ( oldExt << ".log" ) : ( oldExt << "_" << *logIndex << ".log" );
					stringstream newExt;
					( *logIndex < MaxExtraFiles ) ? ( *logIndex )++ : *logIndex = 1;
					newExt << "_" << *logIndex << ".log";
					*streamName = StringUtil::Replace( *streamName, oldExt.str(), newExt.str() );
					
					threadStream->open( streamName->c_str(), ios::binary | ios::out | ios::trunc );
					
					*outputSofar = 0;

					*threadStream << "Log file created because MaxLines was exceeded [" << MaxLines << "] and MaxExtraFiles is [" << MaxExtraFiles << "]" << endl;

				}
				else
				{
					threadStream->open( streamName->c_str(), ios::binary | ios::out | ios::trunc );
					
					*outputSofar = 0;
					//pthread_setspecific( FileOutputter::LinesKey, outputSofar );

					*threadStream << "Log rewind because MaxLines was exceeded [" << MaxLines << "]" << endl;
				}
			}
			else
				( *outputSofar )++;
		}
		return *threadStream;
	}

	return internalGetStream();
}

ostream& FileOutputter::internalGetStream()
{
	pthread_t selfId = pthread_self();

	ofstream* newStream = new ofstream();
	
	stringstream namestream;
				
	// bootstrap
#ifndef WIN32
	if ( ( selfId == 1 ) && ( Prefix == "Unnamed" ) )
		namestream << "Bootstrap_" << Process::GetPID() << ".log";
	else
		namestream << Prefix << "_" << selfId << ".log";
#else
	if ( Prefix.length() == 0 )
		namestream << "Bootstrap_" << selfId << ".log";
	else
		namestream << Prefix << "_" << selfId << ".log";
#endif
	
	string name = namestream.str();
	newStream->open( name.c_str(), ios::binary | ios::out | ios::trunc );
	int setSpecificResult = pthread_setspecific( FileOutputter::StreamKey, newStream );
	if ( 0 != setSpecificResult )
	{
		cerr << "Set thread specific StreamKey failed [" << setSpecificResult << "]";
	}

	unsigned long* newLines = new unsigned long;
	*newLines = 0;
	setSpecificResult = pthread_setspecific( FileOutputter::LinesKey, newLines );
	if ( 0 != setSpecificResult )
	{
		cerr << "Set thread specific LinesKey failed [" << setSpecificResult << "]";
	}

	string* newName = new string( name );
	setSpecificResult = pthread_setspecific( FileOutputter::NamesKey, newName );
	if ( 0 != setSpecificResult )
	{
		cerr << "Set thread specific NamesKey failed [" << setSpecificResult << "]";
	}

	unsigned long* newExtraFiles = new unsigned long;
	*newExtraFiles = 0;
	setSpecificResult = pthread_setspecific( FileOutputter::ExtraFilesKey, newExtraFiles );
	if ( 0 != setSpecificResult )
	{
		cerr << "Set thread specific LinesKey failed [" << setSpecificResult << "]";
	}

	return *newStream;
}

void FileOutputter::setLogMaxLines( const unsigned long maxLines )
{
	MaxLines = maxLines;
}

void FileOutputter::setLogPrefix( const string prefix )
{
	Prefix = prefix;
}

void FileOutputter::setLogMaxExtraFiles( const unsigned long maxLogs )
{
	MaxExtraFiles = maxLogs;
}

bool FileOutputter::wasTerminated()
{
	return m_Terminated;
}

void FileOutputter::setTerminated( const bool value ) 
{
	m_Terminated = value;
}
