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

#ifndef FINTPTRACE_H
#define FINTPTRACE_H

#include "DllMainLog.h"

#include <map>
#include <vector>

#include <fstream>
#include <string>
#include <iostream>
#include <sstream>

#include <pthread.h>

using namespace std;

#ifdef WIN32
#define __MSXML_LIBRARY_DEFINED__
#include <windows.h>
#include <string.h>
#define FINTPFILE ( ( strrchr(__FILE__, '\\') != NULL ) ? strrchr(__FILE__, '\\') + 1 : __FILE__ )
#else
#define FINTPFILE (__FILE__)
#endif

#if defined( WIN32 ) && defined ( _DEBUG ) && defined( CHECK_MEM_LEAKS )

#define MEM_CHECKPOINT_INIT()														\
{																					\
	_CrtMemState state1, state2;													\
	_CrtMemCheckpoint( &state1 );													\
	_CrtMemState memDiff;															\
	{

#define MEM_CHECKPOINT_COLLECT()													\
	_CrtMemCheckpoint( &state2 );

#define MEM_CHECKPOINT_END( clause, module )										\
	}																				\
	MEM_CHECKPOINT_COLLECT()														\
	if ( _CrtMemDifference( &memDiff, &state1, &state2 ) )							\
	{																				\
		_CrtDbgReport( _CRT_WARN, FINTPFILE, __LINE__, module,						\
			"Memory leaks detected during "##clause". Dumping objects... \n" );		\
		_CrtMemDumpAllObjectsSince( &state1 );										\
		_CrtMemDumpStatistics( &memDiff );											\
		_CrtDbgReport( _CRT_WARN, FINTPFILE, __LINE__, module, "Dumping done. \n" );	\
	}																				\
}

#define MEM_CHECKPOINT_END_REPORT( clause, module )									\
	}																				\
	if ( _CrtMemDifference( &memDiff, &state1, &state2 ) )							\
	{																				\
		_CrtDbgReport( _CRT_WARN, FINTPFILE, __LINE__, module,						\
			"Memory leaks detected during "##clause". Dumping objects... \n" );		\
		_CrtMemDumpAllObjectsSince( &state1 );										\
		_CrtMemDumpStatistics( &memDiff );											\
		_CrtDbgReport( _CRT_WARN, FINTPFILE, __LINE__, module, "Dumping done. \n" );	\
	}																				\
}
#define MEM_CHECKPOINT_END_FAIL( clause, module )									\
	}																				\
	MEM_CHECKPOINT_COLLECT()														\
	if ( _CrtMemDifference( &memDiff, &state1, &state2 ) )							\
	{																				\
		_CrtDbgReport( _CRT_WARN, FINTPFILE, __LINE__, module,						\
			"Memory leaks detected during "##clause". Dumping objects... \n" );		\
		_CrtMemDumpAllObjectsSince( &state1 );										\
		_CrtMemDumpStatistics( &memDiff );											\
		_CrtDbgReport( _CRT_WARN, FINTPFILE, __LINE__, module, "Dumping done. \n" );	\
		CPPUNIT_FAIL( "Memory leaks detected during "##clause );					\
	}																				\
}

#else
#define MEM_CHECKPOINT_INIT() {
#define MEM_CHECKPOINT_END( clause, module ) }
#define MEM_CHECKPOINT_END_REPORT( clause, module ) }
#define MEM_CHECKPOINT_END_FAIL( clause, module ) }
#define MEM_CHECKPOINT_COLLECT()
#endif

#define cnull std::ofstream( "DebugOutput.out", ios::app )

#ifdef DEBUG_ENABLED
//EXPIMP_TEMPLATE template class ExportedLogObject std::map< pthread_t, ofstream* >;
namespace FinTP
{
	class ExportedLogObject FileOutputter
	{
		private :

			FileOutputter();

			static pthread_once_t KeysCreate;
			static pthread_key_t StreamKey;
			static pthread_key_t LinesKey;
			static pthread_key_t NamesKey;
			static pthread_key_t ExtraFilesKey;

			static string Prefix;
			static unsigned long MaxLines;
			static unsigned long MaxExtraFiles;

			static FileOutputter m_Instance;

			static bool m_Terminated;
			static pthread_mutex_t m_OutputtersSyncMutex;

			static void CreateKeys();
			static void DeleteStreams( void* data );
			static void DeleteLines( void* data );
			static void DeleteNames( void* data );
			static void DeleteExtraFiles( void* data );

			static ostream& internalGetStream();

		public :

			~FileOutputter();

			static void setLogMaxLines( const unsigned long maxLines = 0 );

			static void setLogPrefix( const string prefix = "Unnamed" );

			static ostream& getStream();
			static bool wasTerminated();
			static void setTerminated( const bool value );

			static void setLogMaxExtraFiles( const unsigned long maxLogs = 0 );
	};
}

//#define DEBUG_GLOBAL( expr ) { stringstream *_ostrlog = new stringstream(); *_ostrlog << "DEBUG [" << FINTPFILE << "] - " << __LINE__ << " : " << expr << endl << flush; cerr.write( _ostrlog->str().c_str(), _ostrlog->str().length() ); cerr << flush; delete _ostrlog; }
#define	DEBUG_GLOBAL( expr ) { if ( !FileOutputter::wasTerminated() ) { FileOutputter::getStream() << "DEBUG [" << FINTPFILE << "] - " << __LINE__ << " : " << expr << endl << flush; } }
#define DEBUG( expr )  DEBUG_GLOBAL( expr )

#ifdef WIN32
#define DEBUG_PRINT_STACK ;
#else
#define DEBUG_PRINT_STACK ;
#endif

#ifdef EXTENDED_DEBUG
#define DEBUG2( expr ) DEBUG( expr )
#define DEBUG_GLOBAL2( expr ) DEBUG_GLOBAL( expr )
#else
#define DEBUG2( expr ) ;
#define DEBUG_GLOBAL2( expr ) ;
#endif


#ifdef WIN32_SERVICE
#define TRACE_SERVICE( expr ) { stringstream _errormessage; _errormessage << expr; OutputDebugString( TEXT( _errormessage.str().c_str() ) ); FileOutputter::getStream() << "TRACE [" << FINTPFILE << "] - " << __LINE__ << " : " << expr << endl << flush; }
#define TRACE_GLOBAL( expr ) TRACE_SERVICE( expr )
#define TRACE( expr ) TRACE_SERVICE( expr )
#define TRACE_NOLOG( expr ) \
	{ \
		try \
		{ \
			stringstream _errormessage; _errormessage << expr; OutputDebugString( TEXT( _errormessage.str().c_str() ) ); FileOutputter::getStream() << "TRACE [" << FINTPFILE << "] - " << __LINE__ << " : " << expr << endl << flush; \
		} \
		catch( ... ){} \
	}
#define DEBUG_NOLOG( expr ) { cout << "DEBUG [" << FINTPFILE << "] - " << __LINE__ << " : " << expr << endl << flush; }
#else
#define TRACE_GLOBAL( expr ) { if ( !FileOutputter::wasTerminated() ) { FileOutputter::getStream() << "TRACE [" << FINTPFILE << "] - " << __LINE__ << " : " << expr << endl << flush; } }
#define TRACE( expr ) TRACE_GLOBAL( expr )
#define TRACE_SERVICE( expr ) TRACE_GLOBAL( expr )
#define TRACE_NOLOG( expr ) \
	{ \
		try \
		{ \
			FileOutputter::getStream() << "TRACE [" << FINTPFILE << "] - " << __LINE__ << " : " << expr << endl << flush; \
		} \
		catch( ... ){} \
		try \
		{ \
			cerr << "TRACE [" << FINTPFILE << "] - " << __LINE__ << " : " << expr << endl << flush; \
		} catch( ... ){}; \
	}
#define DEBUG_NOLOG( expr ) { cout << "DEBUG [" << FINTPFILE << "] - " << __LINE__ << " : " << expr << endl << flush; }
#endif // WIN32_SERVICE

#else //DEBUG_ENABLED

#define DEBUG( expr ) ;
#define DEBUG2( expr ) ;
#define DEBUG_GLOBAL( expr ) ;
#define DEBUG_GLOBAL2( expr ) ;
#define DEBUG_PRINT_STACK ;
#define TRACE_GLOBAL( expr ) ;
#define TRACE( expr ) ;
#define TRACE_NOLOG( expr ) ;

#endif //DEBUG_ENABLED

#endif // FINTPTRACE_H

