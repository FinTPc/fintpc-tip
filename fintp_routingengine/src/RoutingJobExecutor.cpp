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

#include "LogManager.h"

#include "RoutingJobExecutor.h"
#include "RoutingJob.h"
#include "RoutingExceptions.h"
#include "RoutingEngine.h"
#include "RoutingDbOp.h"

RoutingJobExecutor::RoutingJobExecutor() : m_Signature( "unknown" ), m_ShouldContinue( true ), m_ReadyToExecute( false ), m_JobRead( false )
{
	int mutexInitResult = pthread_mutex_init( &m_ExecuteMutex, NULL );
	if ( 0 != mutexInitResult )
	{
		TRACE_NOLOG( "Unable to init mutex ExecuteMutex [" << mutexInitResult << "]" );
	}
	int condInitResult = pthread_cond_init( &m_ExecuteCondition, NULL );
	if ( 0 != condInitResult )
	{
		TRACE_NOLOG( "Unable to init condition ExecuteCondition [" << condInitResult << "]" );
	}

	string phase = "read job";
	string userId = "";

	try
	{
		m_RoutingJob.ReadNextJob( "" );
		m_Signature = m_RoutingJob.getFunction() + string( " on " ) + m_RoutingJob.getJobTable();
		userId = m_RoutingJob.getUserId();

		m_JobRead = true;
		DEBUG_GLOBAL( "Routing job read" );
	}
	catch( const RoutingExceptionJobAttemptsExceded &rex )
	{
		m_ShouldContinue = false;
		m_RoutingJob.Abort();
		
		stringstream errorMessage;
		errorMessage << "Unable to " << phase << " [Job backout count of 3 exceeded]";

		TRACE( rex.what() );
		LogManager::Publish( errorMessage.str() );
		return;
	}
	catch( AppException& ex )
	{
		EventSeverity severity = ex.getSeverity();
		if( severity.getSeverity() == EventSeverity::Fatal )
			m_RoutingJob.Commit();
		else
			m_RoutingJob.Rollback();

		stringstream errorMessage;
		errorMessage << "Unable to " << phase << ". [A " << severity.ToString() << " error has occured during routing : " << ex.getMessage() << "]";
		
		TRACE( errorMessage.str() );

		if ( userId.length() > 0 )
			ex.addAdditionalInfo( "User id", userId );

		LogManager::Publish( ex );
		return;
	}
	catch( const std::exception& ex )
	{
		m_RoutingJob.Rollback();
		
		stringstream errorMessage;
		errorMessage << "Unable to " << phase << ". [" << ex.what() << "]";
		
		TRACE( errorMessage.str() );
		
		AppException aex( errorMessage.str(), EventType::Error );	
		if ( userId.length() > 0 )
			aex.addAdditionalInfo( "User id", userId );
		aex.addAdditionalInfo( "Job id", m_RoutingJob.getJobId() );

		LogManager::Publish( aex );
		return;
	}
	catch( ... )
	{
		m_RoutingJob.Rollback();

		stringstream errorMessage;
		errorMessage << "Unable to " << phase << ". [unknown reason]";
		
		TRACE( errorMessage.str() );
		
		AppException aex( errorMessage.str(), EventType::Error );
		if ( userId.length() > 0 )
			aex.addAdditionalInfo( "User id", userId );
		aex.addAdditionalInfo( "Job id", m_RoutingJob.getJobId() );

		LogManager::Publish( aex );
		return;
	}

	pthread_attr_t scanThreadAttr;
	int attrInitResult = pthread_attr_init( &scanThreadAttr );
	if ( 0 != attrInitResult )
	{
		TRACE( "Error initializing scan thread attribute [" << attrInitResult << "]" );
		throw runtime_error( "Error initializing scan thread attribute" );
	}

	int setDetachResult = pthread_attr_setdetachstate( &scanThreadAttr, PTHREAD_CREATE_JOINABLE );
	if ( 0 != setDetachResult )
	{
		TRACE( "Error setting joinable option to scan thread attribute [" << setDetachResult << "]" );
		throw runtime_error( "Error setting joinable option to scan thread attribute" );
	}
	
	int threadStatus = 0;
	
	// if running in debugger, reattempt to create thread on failed request	
	do
	{
		threadStatus = pthread_create( &m_ExecThreadId, &scanThreadAttr, RoutingJobExecutor::JobExecutor, this );
	} while( ( threadStatus != 0 ) && ( errno == EINTR ) );
	if ( threadStatus != 0 )
	{
		int errCode = errno;
		int attrDestroyResult = pthread_attr_destroy( &scanThreadAttr );
		if ( 0 != attrDestroyResult )
		{
			TRACE( "Unable to destroy thread attribute [" << attrDestroyResult << "]" );
		}	
		
		stringstream errorMessage;
#ifdef CRT_SECURE
		char errBuffer[ 95 ];
		strerror_s( errBuffer, sizeof( errBuffer ), errCode );
		errorMessage << "Unable to create job exec thread. [" << errBuffer << "]";
#else
		errorMessage << "Unable to create job exec thread. [" << strerror( errCode ) << "]";
#endif
		throw runtime_error( errorMessage.str() );
	}

	int attrDestroyResult = pthread_attr_destroy( &scanThreadAttr );
	if ( 0 != attrDestroyResult )
	{
		TRACE( "Unable to destroy thread attribute [" << attrDestroyResult << "]" );
	}
}

RoutingJobExecutor::~RoutingJobExecutor()
{
	if( !m_JobRead )
		return;

	try
	{
		Signal( false );
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured when signalig execute condition" );
		} catch( ... ){}
	}

	try
	{
		int joinResult = pthread_join( m_ExecThreadId, NULL );
		if ( 0 != joinResult )
		{
			TRACE( "An error occured when joining the execute thread [" << joinResult << "]" );
		}
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured when joining the execute thread" );
		} catch( ... ){}
	}
}

void RoutingJobExecutor::Lock()
{
	if( !m_JobRead )
		return;
	int joinResult = pthread_join( m_ExecThreadId, NULL );
	if ( 0 != joinResult )
	{
		TRACE( "An error occured when joining the execute thread [" << joinResult << "]" );
	}
}

void RoutingJobExecutor::Signal( bool shouldContinue )
{
	if( !m_JobRead )
		return;

	int mutexLockResult = pthread_mutex_lock( &m_ExecuteMutex );
	if ( 0 != mutexLockResult )
	{
		TRACE( "Unable to lock ExecuteMutex mutex [" << mutexLockResult << "]" );
	}

	// if this thread was the first to acquire the mutex, m_ReadyToExecute should allow the exec thread
	// not to wait for a signal anymore ( it was already sent )
	m_ShouldContinue = shouldContinue;
	m_ReadyToExecute = true;
	int condSignalResult = pthread_cond_signal( &m_ExecuteCondition );
	if ( 0 != condSignalResult )
	{
		TRACE_NOLOG( "Signal ExecuteCondition failed [" << condSignalResult << "]" );
	}
	int mutexUnlockResult = pthread_mutex_unlock( &m_ExecuteMutex );
	if ( 0 != mutexUnlockResult )
	{
		TRACE_NOLOG( "Unable to unlock m_ExecuteMutex mutex [" << mutexUnlockResult << "]" );
	}
}

void RoutingJobExecutor::Execute()
{
	string correlationId = "";
	string phase = "read message [";
	string userId = "";

	try
	{
		userId = m_RoutingJob.getUserId();
		
		// read associated message
		phase.append( m_RoutingJob.getMessageId() );
		phase.append( "]" );

		WorkItem< RoutingMessage > workMessage( new RoutingMessage( m_RoutingJob.getJobTable(), m_RoutingJob.getMessageId(), true ) );
		RoutingMessage* routingMessage = workMessage.get();
		routingMessage->Read();
		DEBUG_GLOBAL( "Message read" );

		// enqueue message
		RoutingEngine::getMessagePool().addPoolItem( m_RoutingJob.getJobId(), workMessage );
		DEBUG_GLOBAL( "Inserted routing message in pool [" << routingMessage->getMessageId() << "]" );

		// set correlation ID of the message ( correlate events )
		correlationId = routingMessage->getCorrelationId();

		// wait for a signal to start executing the job
		phase = "execute job";
		if ( !m_ReadyToExecute )
		{
			int condWaitResult = pthread_cond_wait( &m_ExecuteCondition, &m_ExecuteMutex );
			if ( 0 != condWaitResult )
			{
				TRACE_NOLOG( "Condition wait on ExecuteCondition failed [" << condWaitResult << "]" );
			}
		}
		int mutexUnlockResult = pthread_mutex_unlock( &m_ExecuteMutex );
		if ( 0 != mutexUnlockResult )
		{
			TRACE_NOLOG( "Unable to unlock m_ExecuteMutex mutex [" << mutexUnlockResult << "]" );
		}

		if ( !m_ShouldContinue )
		{
			TRACE( "Routing job canceled." );		
			return;
		}

		// signal received, execute the job
		RoutingEngine::getRoutingSchema()->ApplyRouting( &m_RoutingJob );

		// the job was defered
		if ( m_RoutingJob.isDefered() )
		{
			DEBUG( "The job was defered" )
			return;
		}
		
		m_RoutingJob.Commit();
	}
	catch( const RoutingExceptionJobAttemptsExceded &rex )
	{
		m_ShouldContinue = false;
		m_RoutingJob.Abort();
		
		stringstream errorMessage;
		errorMessage << "Unable to " << phase << " [Job backout count of 3 exceeded]";

		TRACE( rex.what() );
		LogManager::Publish( errorMessage.str() );
	}
	catch( AppException& ex )
	{
		EventSeverity severity = ex.getSeverity();
		if( severity.getSeverity() == EventSeverity::Fatal )
			m_RoutingJob.Commit();
		else
			m_RoutingJob.Rollback();

		stringstream errorMessage;
		errorMessage << "Unable to " << phase << ". [A " << severity.ToString() << " error has occured during routing : " << ex.getMessage() << "]";
		
		TRACE( errorMessage.str() );

		if ( userId.length() > 0 )
			ex.addAdditionalInfo( "User id", userId );

		LogManager::Publish( ex );
	}
	catch( const std::exception& ex )
	{
		m_RoutingJob.Rollback();
		
		stringstream errorMessage;
		errorMessage << "Unable to " << phase << ". [" << ex.what() << "]";
		
		TRACE( errorMessage.str() );
		
		AppException aex( errorMessage.str(), EventType::Error );	
		if ( correlationId.length() > 0 )
			aex.setCorrelationId( correlationId );
		if ( userId.length() > 0 )
			aex.addAdditionalInfo( "User id", userId );
		aex.addAdditionalInfo( "Job id", m_RoutingJob.getJobId() );

		LogManager::Publish( aex );
	}
	catch( ... )
	{
		m_RoutingJob.Rollback();

		stringstream errorMessage;
		errorMessage << "Unable to " << phase << ". [unknown reason]";
		
		TRACE( errorMessage.str() );
		
		AppException aex( errorMessage.str(), EventType::Error );
		if ( correlationId.length() > 0 )
			aex.setCorrelationId( correlationId );
		if ( userId.length() > 0 )
			aex.addAdditionalInfo( "User id", userId );
		aex.addAdditionalInfo( "Job id", m_RoutingJob.getJobId() );

		LogManager::Publish( aex );
	}
}

void RoutingJobExecutor::CheckExecute()
{
	int mutexLockResult = pthread_mutex_lock( &m_ExecuteMutex );
	if ( 0 != mutexLockResult )
	{
		TRACE( "Unable to lock ExecuteMutex mutex [" << mutexLockResult << "]" );
	}
	try
	{
		Execute();
	}
	catch( ... ){}

	int mutexUnlockResult = pthread_mutex_unlock( &m_ExecuteMutex );
	if ( 0 != mutexUnlockResult )
	{
		TRACE( "Unable to unlock m_ExecuteMutex mutex [" << mutexUnlockResult << "]" );
	}
	RoutingDbOp::TerminateSelf(); 
	RoutingEngine::getMessagePool().removePoolItem( m_RoutingJob.getJobId(), false );
}

void* RoutingJobExecutor::JobExecutor( void *data )
{
	RoutingJobExecutor* me = static_cast< RoutingJobExecutor* >( data );
	
	me->CheckExecute();

	pthread_exit( NULL ); 
	return NULL;
}
