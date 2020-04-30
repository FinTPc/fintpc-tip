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

#include "AbstractBackupLoader.h"
#include "LogManager.h"
#include "Trace.h"

using namespace FinTP;

AbstractBackupLoader::~AbstractBackupLoader()
{
	m_BackupThreadId = 0;
}

void AbstractBackupLoader::waitForExit()
{
	int joinResult = pthread_join( m_BackupThreadId, NULL ); 
	if ( 0 != joinResult )
	{
		TRACE( "An error occured when joining the backup thread [" << joinResult << "]" );
	}
}

void AbstractBackupLoader::setEnableRaisingEvents( bool val )
{
	if ( m_Enabled == val )
		return;

	m_Enabled = val;
	
	if( m_Enabled )
	{
		pthread_attr_t backupThreadAttr;

		int attrInitResult = pthread_attr_init( &backupThreadAttr );
		if ( 0 != attrInitResult )
		{
			TRACE( "Error initializing backup thread attribute [" << attrInitResult << "]" );

			int attrDestroyResult2 = pthread_attr_destroy( &backupThreadAttr );
			if ( 0 != attrDestroyResult2 )
			{
				TRACE( "Unable to destroy backup thread attribute [" << attrDestroyResult2 << "]" );
			}

			throw runtime_error( "Error initializing backup thread attribute" );
		}

		int setDetachResult = pthread_attr_setdetachstate( &backupThreadAttr, PTHREAD_CREATE_JOINABLE );
		if ( 0 != setDetachResult )
		{
			TRACE( "Error setting joinable option to backup thread attribute [" << setDetachResult << "]" );

			int attrDestroyResult2 = pthread_attr_destroy( &backupThreadAttr );
			if ( 0 != attrDestroyResult2 )
			{
				TRACE( "Unable to destroy backup thread attribute [" << attrDestroyResult2 << "]" );
			}

			throw runtime_error( "Error setting joinable option to backup thread attribute" );
		}
				
		// if running in debugger, reattempt to create thread on failed request
		int threadStatus = 0, errCodeCreate = 0;
		do
		{
			threadStatus = pthread_create( &m_BackupThreadId, &backupThreadAttr, AbstractBackupLoader::BackupInNewThread, this );
			errCodeCreate = errno;
			DEBUG( "Backup thread create result [" << threadStatus << "] errno [" << errCodeCreate << "]" );
		} while( ( threadStatus != 0 ) && ( errCodeCreate == EINTR ) );

		if ( threadStatus != 0 )
		{
			int errCode = errno;
			int attrDestroyResult = pthread_attr_destroy( &backupThreadAttr );
			if ( 0 != attrDestroyResult )
			{
				TRACE( "Unable to destroy backup thread attribute [" << attrDestroyResult << "]" );
			}	
			
			stringstream errorMessage;
#ifdef CRT_SECURE
			char errBuffer[ 95 ];
			strerror_s( errBuffer, sizeof( errBuffer ), errCode );
			errorMessage << "Unable to create backup thread. [" << errBuffer << "]";
#else
			errorMessage << "Unable to create backup thread. [" << strerror( errCode ) << "]";
#endif	
			throw runtime_error( errorMessage.str() );
		}
				
		int attrDestroyResult2 = pthread_attr_destroy( &backupThreadAttr );
		if ( 0 != attrDestroyResult2 )
		{
			TRACE( "Unable to destroy backup thread attribute [" << attrDestroyResult2 << "]" );
		}
	}
	else
	{
		DEBUG( "Stopping backup thread..." );
		int joinResult = pthread_join( m_BackupThreadId, NULL ); 
		if ( 0 != joinResult )
		{
			TRACE( "An error occured when joining the backup thread [" << joinResult << "]" );
		}
		DEBUG( "Backup thread stopped." );
	}
}

void* AbstractBackupLoader::BackupInNewThread( void* pThis )
{
	AbstractBackupLoader* me = static_cast< AbstractBackupLoader* >( pThis );
	TRACE( "BackupLoader thread" );

	try
	{
		me->internalLoad();
	}
	catch( const std::exception& ex )
	{
		stringstream errorMessage;
		errorMessage << typeid( ex ).name() << " exception encountered by backup loader [" << ex.what() << "]";
		TRACE( errorMessage.str() );

		try
		{
			LogManager::Publish( AppException( "Error encountered by backup loader", ex ) );
		}
		catch( ... ){}
	}
	catch( ... )
	{
		stringstream errorMessage;
		errorMessage << "Exception encountered by backup loader [unhandled exception]";
		TRACE( errorMessage.str() );

		try
		{
			LogManager::Publish( errorMessage.str() );
		}
		catch( ... ){}
	}

	pthread_exit( NULL );
	return NULL;
}
