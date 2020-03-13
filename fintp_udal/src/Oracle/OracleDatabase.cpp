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

#include "OracleDatabase.h"
#include "OracleParameter.h"
#include "OracleColumn.h"
#include "OracleDatabaseProvider.h"

#include "Trace.h"

#include <string>
#include <iomanip>
#include <iostream>
#include <exception>
#include <sstream>

#include "Base64.h"

using namespace std;
using namespace FinTP;

#define FREE_ORA_HANDLE( ora_handle, ora_handle_name, ora_handle_type ) \
	if ( ( ora_handle ) != NULL ) \
	{\
		DEBUG2( "Free " << ora_handle_name << " handle ..." );\
		sword ora_status = 0;\
		try\
		{\
			ora_status = OCIHandleFree( ( dvoid * )( ora_handle ), ( ora_handle_type ) );\
			if ( ora_status != OCI_SUCCESS )\
				throw runtime_error( "free handle failed" ); \
		}\
		catch( ... )\
		{\
			TRACE( "Free [" << ora_handle_name << "] handle failed [" << getErrorInformation( m_hEnv, ora_status, OCI_HTYPE_ENV ) << "]" );\
		}\
		ora_handle = NULL;\
	}

#define FREE_ORA_HANDLE_NOCHECK( ora_handle, ora_handle_name, ora_handle_type ) \
	if ( ( ora_handle ) != NULL ) \
	{\
		DEBUG2( "Free " << ora_handle_name << " handle ..." );\
		sword ora_status = 0;\
		try\
		{\
			ora_status = OCIHandleFree( ( dvoid * )( ora_handle ), ( ora_handle_type ) );\
			if ( ora_status != OCI_SUCCESS )\
				throw runtime_error( "free handle failed" ); \
		}\
		catch( ... )\
		{\
			TRACE( "Free [" << ora_handle_name << "] handle failed [" << getErrorInformation( m_hEnv, ora_status, OCI_HTYPE_ENV, false ) << "]" );\
		}\
		ora_handle = NULL;\
	}

#define FREE_ORA_DESC_NOCHECK( ora_handle, ora_handle_name, ora_handle_type ) \
	if ( ( ora_handle ) != NULL ) \
	{\
		DEBUG2( "Free " << ora_handle_name << " descriptor ..." );\
		sword ora_status = OCIDescriptorFree( ( dvoid* )( ora_handle ), ( ora_handle_type ) );\
		if ( ora_status != OCI_SUCCESS )\
			TRACE( "Free [" << ora_handle_name << "] descriptor failed [" << getErrorInformation( m_StatementHandle, ora_status, OCI_HTYPE_STMT, false ) << "]" );\
		ora_handle = NULL; \
	}

#define FREE_ORA_DESC( ora_handle, ora_handle_name, ora_handle_type ) \
	if ( ( ora_handle ) != NULL ) \
	{\
		DEBUG2( "Free " << ora_handle_name << " descriptor ..." );\
		sword ora_status = OCIDescriptorFree( ( dvoid* )( ora_handle ), ( ora_handle_type ) );\
		if ( ora_status != OCI_SUCCESS )\
			TRACE( "Free [" << ora_handle_name << "] descriptor failed [" << getErrorInformation( m_StatementHandle, ora_status, OCI_HTYPE_STMT ) << "]" );\
		ora_handle = NULL; \
	}

OracleDatabase::OracleDatabase() : Database(),  m_IsConnected( false ), m_IsReconnecting( false ), m_ConnectionString( "" ),
	m_hEnv( NULL ), m_hError( NULL ), m_hServiceContext( NULL ), m_hSession( NULL ), m_hServer( NULL ), m_hTransaction( NULL ),
	m_StatementHandle( NULL ), m_hCursor( NULL ), m_HoldCursorRowId( NULL ), m_XmlContext( NULL ), m_ClobLocator( NULL ),
	m_FetchClobLocator( NULL ), m_BlobLocator( NULL ), m_FetchBlobLocator( NULL ), m_ArrayType( NULL )
{
	// Alocate environment handle
	DEBUG2( "CONSTRUCTOR" );

	// Create OCI Environment
	DEBUG( "Creating OCI environment ... " );
	sword status = OCIEnvCreate( ( OCIEnv ** )&m_hEnv, OCI_THREADED | OCI_OBJECT , ( dvoid * )0, 0, 0, 0, ( size_t ) 0, (dvoid **)0 );

	if ( status != OCI_SUCCESS )
	{
		stringstream errorMessage;
		errorMessage << "Error creating environment. Check environment variables and access rights. [" <<
		             getErrorInformation( m_hEnv, status, OCI_HTYPE_ENV, false ) << "]";

		throw runtime_error( errorMessage.str() );
	}

	// Allocate error handle
	DEBUG( "Allocating error handle ... " );
	status = OCIHandleAlloc( ( dvoid * )m_hEnv, ( dvoid ** )&m_hError, OCI_HTYPE_ERROR, ( size_t )0, ( dvoid ** )0 );

	if ( status != OCI_SUCCESS )
	{
		// may be able to continue and connect later
		TRACE( "Alloc environment error [" << getErrorInformation( m_hError, status, OCI_HTYPE_ERROR, false ) << "]" );
	}
}

OracleDatabase::~OracleDatabase()
{
	// Free environment handle
	DEBUG2( "DESTRUCTOR" );

	try
	{
		if ( m_ArrayType!= NULL )
			OCIObjectUnpin( m_hEnv, m_hError, m_ArrayType );

		OracleDatabase::Disconnect();

		// free error handle
		FREE_ORA_HANDLE( m_hError, "error", OCI_HTYPE_ERROR );

		//Free Environment Handle ( last because all handles depend on this )
		FREE_ORA_HANDLE( m_hEnv, "environment", OCI_HTYPE_ENV );
		m_hEnv = NULL;
	}
	catch( const std::exception& error )
	{
		try
		{
			TRACE( "An error occured during database termination [" << error.what() << "]" );
		} catch( ... ) {};
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured during database termination [unknown error]" );
		} catch( ... ) {};
	}
}

void OracleDatabase::ClearDateTimestampBinds()
{
	try
	{
		for( unsigned int i=0; i<m_Date.size(); i++ )
		{
			FREE_ORA_DESC( *( m_Date[ i ] ), "date", ( ub4 )OCI_DTYPE_DATE );
			delete m_Date[ i ];
		}
	}
	catch( ... )
	{
		TRACE( "An error occured while deleting date binds" );
	}

	// this will memleak if the above statement threw
	m_Date.clear();

	try
	{
		for( unsigned int i=0; i<m_Timestamp.size(); i++ )
		{
			FREE_ORA_DESC( *( m_Timestamp[ i ] ), "timestamp", ( ub4 )OCI_DTYPE_TIMESTAMP );
			delete m_Timestamp[ i ];
		}
	}
	catch( ... )
	{
		TRACE( "An error occured while deleting timestamp binds" );
	}

	// this will memleak if the above statement threw
	m_Timestamp.clear();
}

void OracleDatabase::Connect( const ConnectionString& connectionString )
{
	DEBUG( "Thread [" << pthread_self() << "] start to connect [" << connectionString.getDatabaseName() << "] ... " );
	m_ConnectionString = connectionString;

	sword status;
	// Set operational context

	try
	{
		// Allocate service context ( defines attributes that determine the operational context for OCI calls to a server )
		DEBUG( "Allocating service context handle ..." );
		status = OCIHandleAlloc( ( dvoid * )m_hEnv, ( dvoid ** )&m_hServiceContext, OCI_HTYPE_SVCCTX, ( size_t )0, ( dvoid ** )0 );

		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Alloc service context failed [" << getErrorInformation( m_hEnv, status, OCI_HTYPE_ENV, false ) << "]";
			throw runtime_error( errorMessage.str() );
		}

		// Initialize service context ( allocate and set service context attributes):
		// - server ( identifies a connection to a database )
		// - user session ( defines a user's roles and privileges )
		// - transaction ( defines the transaction in which the SQL operations are performed )

		// Allocate server handle ( identifies a connection to a database )
		DEBUG( "Allocating server handle ..." );
		status = OCIHandleAlloc( ( dvoid * )m_hEnv, ( dvoid ** )&m_hServer, OCI_HTYPE_SERVER, ( size_t )0, (dvoid ** )0 );

		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Alloc server failed [" << getErrorInformation( m_hEnv, status, OCI_HTYPE_ENV, false ) << "]";
			throw runtime_error( errorMessage.str() );
		}

		// Initialize the server
		DEBUG( "Initializing server ... " );
		status = OCIServerAttach( m_hServer, m_hError, ( text * )connectionString.getDatabaseName().data(),
		                          connectionString.getDatabaseName().length(), OCI_DEFAULT );

		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Attach to server failed [" << getErrorInformation( m_hError, status, OCI_HTYPE_ERROR, false ) << "]";
			throw runtime_error( errorMessage.str() );
		}

		// Set Server attribute in Service context
		DEBUG( "Setting server attribute in service context ..." );
		status = OCIAttrSet( ( dvoid * )m_hServiceContext, OCI_HTYPE_SVCCTX, ( dvoid * )m_hServer,
		                     ( ub4 )0, OCI_ATTR_SERVER, ( OCIError * )m_hError );

		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO) )
		{
			stringstream errorMessage;
			errorMessage << "Set server attribute failed [" << getErrorInformation( m_hError, status, OCI_HTYPE_ERROR, false ) << "]";

			// we may be able to continue
			TRACE( errorMessage.str() );
		}

		// Allocate user session handle ( defines a user's roles and privileges )
		DEBUG( "Allocate user session handle ..." );
		status = OCIHandleAlloc( ( dvoid * )m_hEnv, ( dvoid ** )&m_hSession, ( ub4 )OCI_HTYPE_SESSION, ( size_t )0, ( dvoid ** )0 );

		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Alloc user session failed [" << getErrorInformation( m_hEnv, status, OCI_HTYPE_ENV, false ) << "]";
			throw runtime_error( errorMessage.str() );
		}

		// Set user session attributes ( user security domain )
		// - username
		// - password
		DEBUG( "Setting username ..." );
		status = OCIAttrSet( ( dvoid * )m_hSession, ( ub4 )OCI_HTYPE_SESSION,
		                     ( dvoid * )connectionString.getUserName().data(), ( ub4 )connectionString.getUserName().length(),
		                     ( ub4 )OCI_ATTR_USERNAME, m_hError );

		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Setting username failed [" << getErrorInformation( m_hError, status, OCI_HTYPE_ERROR, false ) << "]";
			throw runtime_error( errorMessage.str() );
		}

		DEBUG( "Setting password ..." );
		status = OCIAttrSet( ( dvoid * )m_hSession, ( ub4 ) OCI_HTYPE_SESSION,
		                     ( dvoid * )connectionString.getUserPassword().data(), ( ub4 )connectionString.getUserPassword().length(),
		                     ( ub4 )OCI_ATTR_PASSWORD, m_hError );

		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Setting user password failed [" << getErrorInformation( m_hError, status, OCI_HTYPE_ERROR, false ) << "]";
			throw runtime_error( errorMessage.str() );
		}

		// Initialize the session
#ifdef COMPAT_ORACLE_8
		DEBUG( "Initializing the session ... " );
		status = OCISessionBegin ( m_hServiceContext, m_hError, m_hSession, OCI_CRED_RDBMS, ( ub4 )OCI_DEFAULT );
#else
		DEBUG( "Initializing the session [caching enabled] ... " );
		status = OCISessionBegin ( m_hServiceContext, m_hError, m_hSession, OCI_CRED_RDBMS, ( ub4 )OCI_STMT_CACHE );
#endif
		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Session init failed [" << getErrorInformation( m_hError, status, OCI_HTYPE_ERROR, false ) << "]";
			throw runtime_error( errorMessage.str() );
		}
		m_IsConnected = true;

		// Set Session attribute in Service context
		DEBUG( "Setting session attribute in service context ..." );
		status = OCIAttrSet( m_hServiceContext, ( ub4 )OCI_HTYPE_SVCCTX, ( dvoid * )m_hSession,
		                     ( ub4 )0, ( ub4 )OCI_ATTR_SESSION, m_hError );
		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Setting session attribute failed [" << getErrorInformation( m_hError, status, OCI_HTYPE_ERROR, false ) << "]";
			TRACE( errorMessage.str() );
		}
	}
	catch( ... )
	{
		FREE_ORA_HANDLE( m_hSession, "session", OCI_HTYPE_SESSION );
		FREE_ORA_HANDLE( m_hServer, "server", OCI_HTYPE_SERVER );
		FREE_ORA_HANDLE( m_hServiceContext, "service context", OCI_HTYPE_SVCCTX );

		throw;
	}

	m_IsReconnecting = false;
	DEBUG( "Connected" );
}

void OracleDatabase::Disconnect()
{
	sword status = 0;
	DEBUG( "Terminating Xml context... " );
	try
	{
		for( CacheManager< string, OCIType* >::const_iterator typeWalker = m_TypeCache.begin(); typeWalker != m_TypeCache.end(); typeWalker++ )
		{
			status = OCIObjectUnpin( m_hEnv, m_hError, typeWalker->second );
			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				TRACE( "Unpin type failed : " << getErrorInformation( m_hError, status, OCI_HTYPE_ERROR, false ) );
			}
		}
		m_TypeCache.clear();

#ifdef ORACLE_HAS_AQ
		if ( m_XmlContext != NULL )
		{
			OCIXmlDbFreeXmlCtx( m_XmlContext );
			m_XmlContext = NULL;
		}
#endif
	}
	catch( const std::exception& error )
	{
		try
		{
			TRACE( "An error occured during XML context termination [" << error.what() << "]" );
		} catch( ... ) {};
	}
	catch( ... )
	{
		try
		{
			TRACE( "An error occured during XML context termination [unknown error]" );
		} catch( ... ) {};
	}

	DEBUG( "Terminating active statements... " );

	// no connection check on released descriptors
	ReleaseCursor( false );

	FREE_ORA_HANDLE_NOCHECK( m_hCursor, "cursor", OCI_HTYPE_STMT );
	FREE_ORA_HANDLE_NOCHECK( m_StatementHandle, "statement", OCI_HTYPE_STMT );

	// dependent on service context handle
	FREE_ORA_HANDLE_NOCHECK( m_hTransaction, "transaction", OCI_HTYPE_TRANS );

	DEBUG( "Disconnecting from database ..." );

	// End the session
	try
	{
		if ( m_hServiceContext == NULL )
		{
			DEBUG( "Handle [service context] already deallocated" );
		}
		else
		{
			status = OCISessionEnd( m_hServiceContext, m_hError, m_hSession, ( ub4 )OCI_DEFAULT );
			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				TRACE( "End session failed : " << getErrorInformation( m_hError, status, OCI_HTYPE_ERROR, false ) );
			}
		}
	}
	catch( ... ) {}

	// dependent on service context handle
	FREE_ORA_HANDLE_NOCHECK( m_hSession, "session", OCI_HTYPE_SESSION );

	// Detach the server
	DEBUG( "Detaching from server ..." );
	try
	{
		if ( m_hServer == NULL )
		{
			DEBUG( "Handle [server] already deallocated" );
		}
		else
		{
			status = OCIServerDetach( m_hServer, m_hError, OCI_DEFAULT );
			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				TRACE( "Detach from server failed : " << getErrorInformation( m_hError, status, OCI_HTYPE_ERROR, false ) );
			}
		}
	}
	catch( ... ) {}

	// dependent on service context handle
	FREE_ORA_HANDLE_NOCHECK( m_hServer, "server", OCI_HTYPE_SERVER );
	FREE_ORA_HANDLE_NOCHECK( m_hServiceContext, "service context", OCI_HTYPE_SVCCTX );

	DEBUG( "No longer connected" );
	m_IsConnected = false;
}

void OracleDatabase::BindParams( const ParametersVector& vectorOfParameters, const unsigned int startIndex )
{
	DEBUG( "Number of parameters : " << vectorOfParameters.size() );

	sword status;
	unsigned int i = 0;
	try
	{
		for ( i=0; i<vectorOfParameters.size(); i++ )
		{
			DataType::DATA_TYPE paramType = vectorOfParameters[i]->getType();
			DataParameterBase::PARAMETER_DIRECTION paramDirection = vectorOfParameters[ i ]->getDirection();

			DEBUG2( "Param #" << i <<
			        " Direction: " << vectorOfParameters[ i ]->getDirection() <<
			        "; Dimension: " << vectorOfParameters[ i ]->getDimension() <<
			        "; Value: " << ( char* )( ( vectorOfParameters[ i ]->getStoragePointer() ) ) <<
			        "; Type: " << paramType );

			DEBUG2( "Binding parameter " << i + 1 << " ... " );
			unsigned short int sqlType = OracleDatabaseFactory::getOracleSqlType( paramType, vectorOfParameters[ i ]->getDimension() );
			switch( paramType )
			{
				case DataType::DATE_TYPE :
				{
					if( paramDirection != DataParameterBase::PARAM_OUT )
					{

						OCIDate paramDate;
						status = OCIDateFromText ( ( OCIError * )m_hError,
						                           ( CONST text * )vectorOfParameters[ i ]->getStoragePointer(),
						                           ( ub4 )vectorOfParameters[ i ]->getDimension(), ( CONST text * ) "YYYY-MM-DD",
						                           ( ub1 )10, ( CONST text * )0 /* default lang */, ( ub4 )0, &paramDate );
						if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
						{
							stringstream errorMessage;
							errorMessage << "Bind date parameter failed [" << getErrorInformation( m_hError, status ) << "]";
							throw runtime_error( errorMessage.str() );
						}


						status = OCIBindByPos( m_StatementHandle, ( OCIBind ** )( vectorOfParameters[ i ]->getBindHandle() ), m_hError, i + startIndex, ( dvoid * )&paramDate, ( sword )( sizeof( OCIDate ) ), sqlType, vectorOfParameters[ i ]->getIndicatorValue(), ( ub2 * )0, ( ub2 * )0, ( ub4 )0, ( ub4 * )0, OCI_DEFAULT );
					}
					else
					{
						throw logic_error( "Could not return Date as a parameter from a sql statement, use query" );
						/*DEBUG( "Allocate DATE descriptor  ... " );
						status = OCIDescriptorAlloc( ( dvoid * )m_hEnv, ( dvoid ** )&m_Date, ( ub4 )OCI_DTYPE_DATE, ( size_t )0, ( dvoid ** )0 );
						if ( status != OCI_SUCCESS )
						{
							stringstream errorMessage;
							errorMessage << "Alloc CLOB descriptor failed [" << getErrorInformation( m_hError, status ) << "]";
							throw runtime_error( errorMessage.str() );
						}
						status = OCIBindByPos( m_StatementHandle, ( OCIBind ** )( vectorOfParameters[ i ]->getBindHandle() ), m_hError, i + startIndex, ( dvoid * )&m_Date , sizeof( OCIDate* ), sqlType, ( dvoid * )0, ( ub2 * )0, ( ub2 * )0, ( ub4 )0, ( ub4 * )0, OCI_DEFAULT );
						//TODO bind m_Date to storage pointer
						*/
					}
				}
				break;

				case DataType::LARGE_CHAR_TYPE :

					DEBUG2( "Allocate CLOB descriptor  ... " );
					status = OCIDescriptorAlloc( ( dvoid * )m_hEnv, ( dvoid ** )&m_ClobLocator, ( ub4 )OCI_DTYPE_LOB, ( size_t )0, ( dvoid ** )0 );
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Alloc CLOB descriptor failed [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}

					DEBUG2( "Create temporary CLOB ... " );
					status = OCILobCreateTemporary( m_hServiceContext, m_hError, m_ClobLocator, ( ub2 ) 0,
					                                SQLCS_IMPLICIT, OCI_TEMP_CLOB, OCI_ATTR_NOCACHE, OCI_DURATION_SESSION );
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Create CLOB failed [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}

					DEBUG2( "Opening CLOB for write ... " );
					status = OCILobOpen( m_hServiceContext, m_hError, m_ClobLocator, OCI_LOB_READWRITE );
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Open CLOB ( write ) failed [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}

					{
						/* ub2 csid = 0;
						ub1 cform = 0;

						OCILobCharSetId( m_hEnv, m_hError, theClob, &csid );
						OCILobCharSetForm( m_hEnv, m_hError, theClob, &cform ); */

						ub4 noOfBytesWritten = vectorOfParameters[ i ]->getDimension() - 1;
						ub4 bufferLen = vectorOfParameters[ i ]->getDimension() - 1;

						DEBUG( "Writing CLOB ... " );
						status = OCILobWrite( m_hServiceContext, m_hError, m_ClobLocator, &noOfBytesWritten,
						                      1, ( dvoid * )vectorOfParameters[ i ]->getStoragePointer(), bufferLen,
						                      OCI_ONE_PIECE,( dvoid * ) 0, ( sb4 ( * ) ( dvoid *, dvoid *, ub4 *, ub1 * ) )0, 0, SQLCS_IMPLICIT );
						if ( status != OCI_SUCCESS )
						{
							stringstream errorMessage;
							errorMessage << "Write CLOB failed [" << getErrorInformation( m_hError, status ) << "]";
							throw runtime_error( errorMessage.str() );
						}
						DEBUG2( "No of bytes written : " << noOfBytesWritten );

						//OCILobGetLength( m_hServiceContext, m_hError, theClob, &lenp );
					}

					DEBUG2( "Closing CLOB ... " );
					status = OCILobClose( m_hServiceContext, m_hError, m_ClobLocator );
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Close CLOB failed [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}

					DEBUG2( "Binding CLOB Parameter ... " );

					status = OCIBindByPos( m_StatementHandle, ( OCIBind ** )( vectorOfParameters[ i ]->getBindHandle() ), m_hError, i + startIndex,
					                       ( dvoid * )&m_ClobLocator, sizeof( OCIClobLocator* ), sqlType,
					                       vectorOfParameters[ i ]->getIndicatorValue(), ( ub2 * )0, ( ub2 * )0, ( ub4 )0, ( ub4 * )0, OCI_DEFAULT );

					break;

				case DataType::BINARY :

					DEBUG2( "Allocate BLOB descriptor  ... " );
					status = OCIDescriptorAlloc( ( dvoid * )m_hEnv, ( dvoid ** )&m_BlobLocator, ( ub4 )OCI_DTYPE_LOB, ( size_t )0, ( dvoid ** )0 );
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Alloc BLOB descriptor failed [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}

					DEBUG2( "Create temporary BLOB ... " );
					status = OCILobCreateTemporary( m_hServiceContext, m_hError, m_BlobLocator, ( ub2 ) 0,
					                                SQLCS_IMPLICIT, OCI_TEMP_BLOB, OCI_ATTR_NOCACHE, OCI_DURATION_SESSION );
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Create BLOB failed [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}

					DEBUG2( "Opening BLOB for write ... " );
					status = OCILobOpen( m_hServiceContext, m_hError, m_BlobLocator, OCI_LOB_READWRITE );
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Open BLOB ( write ) failed [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}

					{
						ub4 noOfBytesWritten = vectorOfParameters[ i ]->getDimension() - 1;
						ub4 bufferLen = vectorOfParameters[ i ]->getDimension() - 1;

						DEBUG( "Writing BLOB ... " );
						status = OCILobWrite( m_hServiceContext, m_hError, m_BlobLocator, &noOfBytesWritten,
						                      1, ( dvoid * )vectorOfParameters[ i ]->getStoragePointer(), bufferLen,
						                      OCI_ONE_PIECE,( dvoid * ) 0, ( sb4 ( * ) ( dvoid *, dvoid *, ub4 *, ub1 * ) )0, 0, SQLCS_IMPLICIT );
						if ( status != OCI_SUCCESS )
						{
							stringstream errorMessage;
							errorMessage << "Write BLOB failed [" << getErrorInformation( m_hError, status ) << "]";
							throw runtime_error( errorMessage.str() );
						}
						DEBUG2( "No of bytes written : " << noOfBytesWritten );
					}

					DEBUG2( "Closing BLOB ... " );
					status = OCILobClose( m_hServiceContext, m_hError, m_BlobLocator );
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Close BLOB failed [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}

					DEBUG2( "Binding BLOB Parameter ... " );

					status = OCIBindByPos( m_StatementHandle, ( OCIBind ** )( vectorOfParameters[ i ]->getBindHandle() ), m_hError, i + startIndex,
					                       ( dvoid * )&m_BlobLocator, sizeof( OCIBlobLocator* ), sqlType,
					                       vectorOfParameters[ i ]->getIndicatorValue(), ( ub2 * )0, ( ub2 * )0, ( ub4 )0, ( ub4 * )0, OCI_DEFAULT );

					break;

				case DataType::ARRAY :
				{
#define TEST if ( ! ( status == OCI_SUCCESS || status == OCI_SUCCESS_WITH_INFO ) ) throw runtime_error( getErrorInformation( m_hError, status ) );

					status = OCIBindByPos( m_StatementHandle,
						( OCIBind ** )( vectorOfParameters[ i ]->getBindHandle() ), m_hError, i + startIndex,
						( dvoid * )0,
						( sword )sizeof (OCIArray *),  sqlType,
						( dvoid * )0, ( ub2 * )0, ( ub2 * )0, ( ub4 )0, ( ub4 * )0, OCI_DEFAULT );
					TEST

					if ( m_ArrayType == NULL )
					{
						string findata("FINDATA");
						string name("KWTYPE");

						status = OCITypeByName(m_hEnv, m_hError, m_hServiceContext,
								(CONST text *) findata.c_str(), (ub4) findata.length() ,
									(CONST text *) name.c_str(), (ub4) name.length(),
								(CONST text *) 0, (ub4) 0, OCI_DURATION_SESSION, OCI_TYPEGET_HEADER,
								&m_ArrayType);
						TEST
					}

					const OracleArrayParameter * oracleParameter = static_cast<  OracleArrayParameter*  > ( vectorOfParameters[i] );

					if ( *oracleParameter->getOCIArray() != NULL )
					{
						status = OCIObjectFree( m_hEnv, m_hError, *oracleParameter->getOCIArray(), OCI_OBJECTFREE_FORCE );
						TEST
					}

					status = OCIObjectNew( m_hEnv, m_hError, m_hServiceContext, OCI_TYPECODE_VARRAY, m_ArrayType, (dvoid*) 0, OCI_DURATION_STATEMENT, TRUE, (dvoid**)oracleParameter->getOCIArray());
					TEST

					//fill the array with OCIStrings
					for ( size_t it = 0; it < oracleParameter->getDimension(); ++it )
					{
						const string& textData = oracleParameter->getElement( it );

						OCIString* ociString = (OCIString*)0; 
						status = OCIStringAssignText( m_hEnv, m_hError,( CONST text * )textData.c_str(), textData.size(), &ociString );
						TEST

						status = OCICollAppend( m_hEnv, m_hError, (CONST dvoid *)ociString, (CONST dvoid *) 0, (OCIArray*) *oracleParameter->getOCIArray());
						TEST

						//deallocate ociString
						status = OCIStringResize( m_hEnv, m_hError, 0, &ociString );
						TEST
					}

					status = OCIBindObject(( OCIBind *)*vectorOfParameters[ i ]->getBindHandle(), m_hError,
							(OCIType *)m_ArrayType, (dvoid **)oracleParameter->getOCIArray(),	(ub4 *) 0, (dvoid **) 0, (ub4 *) 0);
					break;
				}
				default :
					status = OCIBindByPos( m_StatementHandle,
					                       ( OCIBind ** )( vectorOfParameters[ i ]->getBindHandle() ), m_hError, i + startIndex,
					                       ( dvoid * )vectorOfParameters[ i ]->getStoragePointer(),
					                       ( sword )vectorOfParameters[ i ]->getDimension(), sqlType,
					                       vectorOfParameters[ i ]->getIndicatorValue(), ( ub2 * )0, ( ub2 * )0, ( ub4 )0, ( ub4 * )0, OCI_DEFAULT );
					break;
			}

			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "Bind parameter #" << i + 1 << " failed [" << getErrorInformation( m_hError, status ) << "]";
				throw runtime_error( errorMessage.str() );
			}
		}
	}
	catch( const std::exception& ex )
	{
		TRACE( typeid( ex ).name() << " exception binding param [" << ex.what() << "]" );
		try
		{
			DataType::DATA_TYPE paramType = vectorOfParameters[ i ]->getType();
			TRACE( "Param #" << i << " Direction: " << vectorOfParameters[ i ]->getDirection() <<
			       "; Dimension: " << vectorOfParameters[ i ]->getDimension() <<
			       "; Value: " << ( char* )( ( vectorOfParameters[ i ]->getStoragePointer() ) ) <<
			       "; Type: " << paramType );
		}
		catch( ... ) {}

		if ( m_ClobLocator != NULL )
		{
			DEBUG2( "About to free temporary clob... " );
			status = OCILobFreeTemporary( m_hServiceContext, m_hError, m_ClobLocator );
			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				TRACE( "Free temporary clob failed [" << getErrorInformation( m_hError, status ) << "]" );
			}
		}

		FREE_ORA_DESC( m_ClobLocator, "clob", ( ub4 )OCI_DTYPE_LOB );

		if ( m_BlobLocator != NULL )
		{
			DEBUG2( "About to free temporary blob... " );
			status = OCILobFreeTemporary( m_hServiceContext, m_hError, m_BlobLocator );
			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				TRACE( "Free temporary blob failed [" << getErrorInformation( m_hError, status ) << "]" );
			}
		}

		FREE_ORA_DESC( m_BlobLocator, "blob", ( ub4 )OCI_DTYPE_LOB );
	}
	catch( ... )
	{
		TRACE( "Unknown exception binding param." );
		try
		{
			DataType::DATA_TYPE paramType = vectorOfParameters[ i ]->getType();
			TRACE( "Param #" << i << " Direction: " << vectorOfParameters[ i ]->getDirection() <<
			       "; Dimension: " << vectorOfParameters[ i ]->getDimension() <<
			       "; Value: " << ( char* )( ( vectorOfParameters[ i ]->getStoragePointer() ) ) <<
			       "; Type: " << paramType );
		}
		catch( ... ) {}
		if ( m_ClobLocator != NULL )
		{
			DEBUG2( "About to free temporary clob... " );
			status = OCILobFreeTemporary( m_hServiceContext, m_hError, m_ClobLocator );
			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				TRACE( "Free temporary clob failed [" << getErrorInformation( m_hError, status ) << "]" );
			}
		}
		FREE_ORA_DESC( m_ClobLocator, "clob", ( ub4 )OCI_DTYPE_LOB );

		if ( m_BlobLocator != NULL )
		{
			DEBUG2( "About to free temporary blob... " );
			status = OCILobFreeTemporary( m_hServiceContext, m_hError, m_BlobLocator );
			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				TRACE( "Free temporary blob failed [" << getErrorInformation( m_hError, status ) << "]" );
			}
		}
		FREE_ORA_DESC( m_BlobLocator, "blob", ( ub4 )OCI_DTYPE_LOB );
	}
}

void OracleDatabase::ReleaseStatement( const bool isCommandCached, const string& key )
{
#ifndef COMPAT_ORACLE_8
	if ( isCommandCached )
	{
		sword status;

		// A call to the OCIStmtPrepare2() must be followed with OCIStmtRelease()
		// Do not call OCIHandleFree() to free the memory.
		status = OCIStmtRelease( m_StatementHandle, m_hError, ( text * )key.data(), ( ub4 )key.length(), OCI_DEFAULT );

		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Release statement failed [" << getErrorInformation( m_hError, status ) << "]";
			TRACE( errorMessage.str() );
		}
	}
	else
#endif
	{
		FREE_ORA_HANDLE( m_StatementHandle, "statement", OCI_HTYPE_STMT );
	}
}

DataSet* OracleDatabase::innerExecuteCommand( const DataCommand& command, const ParametersVector& vectorOfParameters, const bool onCursor, const unsigned int fetchRows )
{
	m_LastErrorCode = "";

	// Determine Statement Type : Select or Store Procedure
	string modStatementString = "", statementString = command.getStatementString();
	DataCommand cachedCommand = command;
	bool isCommandCached = m_StatementCache.Contains( statementString );

	// paramCount is +1 for queries
	unsigned int paramCount = vectorOfParameters.size() + ( ( command.isQuery() ) ? 1 : 0 );

	// return comand from cache
	if ( isCommandCached )
	{
		cachedCommand = m_StatementCache[ statementString ];
		modStatementString = cachedCommand.getModifiedStatementString();
	}
	else
	{
		stringstream paramBuffer;
		switch( command.getCommandType() )
		{
			case DataCommand::SP :

				// Compose the Stored Procedure CALL statement
				// TODO:  Verify the SP CALL format
				paramBuffer << "CALL " << statementString << "( ";
				for ( unsigned int i=0; i<paramCount; i++ )
				{
					paramBuffer << ":" << i+1;
					if ( i < paramCount - 1 )
						paramBuffer << ",";
				}
				paramBuffer << " )";
				break;

			case DataCommand::INLINE :

				paramBuffer << statementString;
				if ( fetchRows != 0 )
				{
					// if we have a where clause, add another condition
					// like : where guid=' ' and rownum < n
					if ( StringUtil::CaseInsensitiveFind( statementString, "WHERE" ) != string::npos )
					{
						paramBuffer << " AND ROWNUM < " << fetchRows + 1;
					}
					else
					{
						paramBuffer << " WHERE ROWNUM < " << fetchRows + 1;
					}
				}
				else if ( !command.isQuery() && onCursor )
				{
					paramBuffer << " WHERE ROWID = :1";
				}
				break;

			default :
				throw runtime_error( "Command types supported by ExecuteQuery : SP|TEXT" );
		}

		modStatementString = paramBuffer.str();
		cachedCommand.setModifiedStatementString( modStatementString );
	}

	DEBUG( "Executing statement [" << modStatementString << "]" );

	bool isCommandCacheable = command.isCacheable();
	bool statementPrepared = false;
	sword status;
	try
	{
		// A call to OCIStmtPrepare2(), even if the session does not have a statement cache,
		// will also allocate the statement handle. Since we don't have OCIStmtPrepare2 in ORACLE8, perform alloc anyway
#ifndef COMPAT_ORACLE_8
		if ( !isCommandCacheable )
#endif
		{
			// Allocate statement handle
			DEBUG( "Allocating statement handle ..." );
			status = OCIHandleAlloc( ( dvoid * )m_hEnv, ( dvoid ** )&m_StatementHandle,	OCI_HTYPE_STMT,
			                         ( size_t )0, ( dvoid ** )0 );

			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "Alloc statement handle failed [" <<
				             getErrorInformation( m_hEnv, status, OCI_HTYPE_ENV ) << "]";
				throw runtime_error( errorMessage.str() );
			}
		}

#ifndef COMPAT_ORACLE_8

		// Use the function OCIStmtPrepare2() instead of OCIStmtPrepare() when caching
		if ( isCommandCacheable )
		{
			DEBUG2( "Preparing statement [cached] ... " );
#ifdef DEBUG_ENABLED
			if ( m_hTransaction == NULL )
				throw runtime_error( "Attempted to prepare a statement outside a transaction" );
#endif
			status = OCIStmtPrepare2( m_hServiceContext, &m_StatementHandle, m_hError,
			                          ( text * )modStatementString.data(), ( ub4 )modStatementString.length(),
			                          ( text * )modStatementString.data(), ( ub4 )modStatementString.length(), // key is the text itself
			                          ( ub4 )OCI_NTV_SYNTAX, ( ub4 )OCI_DEFAULT );
		}
		else
#endif
		{
			DEBUG( "Preparing statement ... " );
			status = OCIStmtPrepare( m_StatementHandle, m_hError, ( text * )modStatementString.c_str(),
			                         ( ub4 )modStatementString.length(), ( ub4 )OCI_NTV_SYNTAX, ( ub4 )OCI_DEFAULT );
		}

		switch( status )
		{
			case OCI_SUCCESS_WITH_INFO :
			{
				stringstream errorMessage;
				errorMessage << "Prepare " << ( ( isCommandCacheable ) ? "cache" : "no-cache" ) << " statement succeded, but with warnings [" << getErrorInformation( m_hError, status ) << "]";
				DEBUG( errorMessage.str() << " in [" << modStatementString << "]" );
			}

			break;

			case OCI_SUCCESS :
				break;

			default :
			{
				stringstream errorMessage;
				errorMessage << "Prepare " << ( ( isCommandCacheable ) ? "cache" : "no-cache" ) << " statement failed [" << getErrorInformation( m_hError, status ) << "]";

				DBErrorException errorEx( errorMessage.str() );
				errorEx.addAdditionalInfo( "statement", modStatementString );
				errorEx.addAdditionalInfo( "location", "OracleDatabase::Prepare" );
				errorEx.setCode( m_LastErrorCode );

				TRACE( errorMessage.str() << " in [" << modStatementString << "]" );
				throw errorEx;
			}
			break;
		}

		// release statement doesn't work if the statement was not prepared, so , if we throw before this point
		// free handle will be called instead of release statement
		statementPrepared = true;

		// bind a cursor on the first position for queries of SP type
		if ( command.isQuery() && ( command.getCommandType() == DataCommand::SP ) )
		{
			//Allocate cursor handle
			DEBUG2( "Allocating cursor handle..." );
			status = OCIHandleAlloc( ( dvoid * )m_hEnv, ( dvoid ** )&m_hCursor, OCI_HTYPE_STMT, ( size_t )0, ( dvoid ** )0 );

			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "Alloc cursor handle failed [" << getErrorInformation( m_hEnv, status, OCI_HTYPE_ENV ) << "]";
				throw runtime_error( errorMessage.str() );
			}

			OCIBind *cBind;
			// Bind cursor
			DEBUG2( "Binding cursor ..." );
			status = OCIBindByPos( m_StatementHandle, ( OCIBind ** )&cBind, m_hError, 1, &m_hCursor, 0, SQLT_RSET,
			                       ( dvoid * )0, ( ub2 * )0, ( ub2 * )0, ( ub4 )0, ( ub4 * )0, OCI_DEFAULT );

			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "Bind cursor failed [" << getErrorInformation( m_hError, status ) << "]";
				throw runtime_error( errorMessage.str() );
			}

			if ( vectorOfParameters.size() > 0 )
				BindParams( vectorOfParameters, 2 );
		}
		else if ( !command.isQuery() && onCursor )
		{
			OCIBind *rowidBind = ( OCIBind * )0;
			status = OCIBindByPos( m_StatementHandle, ( OCIBind ** )&rowidBind, m_hError, 1, ( dvoid * )&m_HoldCursorRowId,
			                       ( sword )0, SQLT_RDD, ( dvoid * )0, ( ub2 * )0, ( ub2 * )0, ( ub4 )0, ( ub4 * )0, OCI_DEFAULT );

			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "Bind parameter [rowid] failed [" << getErrorInformation( m_hError, status ) << "]";
				throw runtime_error( errorMessage.str() );
			}
			if ( vectorOfParameters.size() > 0 )
				BindParams( vectorOfParameters, 2 );
		}
		else
		{
			if ( vectorOfParameters.size() > 0 )
				BindParams( vectorOfParameters );
		}
	}
	catch( ... )
	{
		vectorOfParameters.Dump();

		freeOCIArrays( vectorOfParameters );

		if( command.isQuery() )
			FREE_ORA_HANDLE( m_hCursor, "cursor", OCI_HTYPE_STMT );

		if ( m_ClobLocator != NULL )
		{
			DEBUG2( "About to free temporary clob... " );
			status = OCILobFreeTemporary( m_hServiceContext, m_hError, m_ClobLocator );
			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				TRACE( "Free temporary clob failed [" << getErrorInformation( m_hError, status ) << "]" );
			}
		}
		FREE_ORA_DESC( m_ClobLocator, "clob", ( ub4 )OCI_DTYPE_LOB );

		if ( m_BlobLocator != NULL )
		{
			DEBUG2( "About to free temporary blob... " );
			status = OCILobFreeTemporary( m_hServiceContext, m_hError, m_BlobLocator );
			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				TRACE( "Free temporary blob failed [" << getErrorInformation( m_hError, status ) << "]" );
			}
		}
		FREE_ORA_DESC( m_BlobLocator, "clob", ( ub4 )OCI_DTYPE_LOB );

		ClearDateTimestampBinds();

		if ( statementPrepared )
			ReleaseStatement( isCommandCacheable, modStatementString );
		else
			ReleaseStatement( false, modStatementString );
		throw;
	}

	//Get Data
	DataSet* returnValue = NULL;
	try
	{
		if ( command.isQuery() )
			returnValue = executeQuery( cachedCommand, isCommandCached, onCursor );
		else
			executeNonQuery( cachedCommand, isCommandCached, onCursor );

		// add it to cache if cacheable
		if( !isCommandCached && isCommandCacheable )
			m_StatementCache.Add( statementString, cachedCommand );
	}
	catch( ... )
	{
		vectorOfParameters.Dump();

		freeOCIArrays( vectorOfParameters );

		// this may fail on oracle 8.1 with no info
		if ( command.isQuery() )
		{
			try
			{
				FREE_ORA_HANDLE( m_hCursor, "cursor", OCI_HTYPE_STMT );
			} catch( ... ) {}

			FREE_ORA_DESC( m_FetchClobLocator, "fetch clob", ( ub4 )OCI_DTYPE_LOB );
			FREE_ORA_DESC( m_FetchBlobLocator, "fetch blob", ( ub4 )OCI_DTYPE_LOB );
		}

		if ( m_ClobLocator != NULL )
		{
			DEBUG2( "About to free temporary clob... " );
			status = OCILobFreeTemporary( m_hServiceContext, m_hError, m_ClobLocator );
			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				TRACE( "Free temporary clob failed [" << getErrorInformation( m_hError, status ) << "]" );
			}
		}
		FREE_ORA_DESC( m_ClobLocator, "clob", ( ub4 )OCI_DTYPE_LOB );

		if ( m_BlobLocator != NULL )
		{
			DEBUG2( "About to free temporary blob... " );
			status = OCILobFreeTemporary( m_hServiceContext, m_hError, m_BlobLocator );
			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				TRACE( "Free temporary blob failed [" << getErrorInformation( m_hError, status ) << "]" );
			}
		}
		FREE_ORA_DESC( m_BlobLocator, "blob", ( ub4 )OCI_DTYPE_LOB );

		ClearDateTimestampBinds();

		ReleaseStatement( isCommandCacheable, modStatementString );

		if ( returnValue != NULL )
		{
			try
			{
				delete returnValue;
				returnValue = NULL;
			} catch( ... ) {}
		}
		throw;
	}

	DEBUG( "Statement executed successfully." );

	freeOCIArrays( vectorOfParameters );

	// this may fail on oracle 8.1 with no info
	if ( command.isQuery() )
	{
		try
		{
			FREE_ORA_HANDLE( m_hCursor, "cursor", OCI_HTYPE_STMT );
		} catch( ... ) {}
		FREE_ORA_DESC( m_FetchClobLocator, "fetch clob", ( ub4 )OCI_DTYPE_LOB );
		FREE_ORA_DESC( m_FetchBlobLocator, "fetch blob", ( ub4 )OCI_DTYPE_LOB );
	}

	if ( m_ClobLocator != NULL )
	{
		DEBUG2( "About to free temporary clob... " );
		status = OCILobFreeTemporary( m_hServiceContext, m_hError, m_ClobLocator );
		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			TRACE( "Free temporary clob failed [" << getErrorInformation( m_hError, status ) << "]" );
		}
	}
	FREE_ORA_DESC( m_ClobLocator, "clob", ( ub4 )OCI_DTYPE_LOB );
	if ( m_BlobLocator != NULL )
	{
		DEBUG2( "About to free temporary blob... " );
		status = OCILobFreeTemporary( m_hServiceContext, m_hError, m_BlobLocator );
		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			TRACE( "Free temporary blob failed [" << getErrorInformation( m_hError, status ) << "]" );
		}
	}
	FREE_ORA_DESC( m_BlobLocator, "blob", ( ub4 )OCI_DTYPE_LOB );

	ClearDateTimestampBinds();

	ReleaseStatement( isCommandCacheable, modStatementString );

	return returnValue;
}

void OracleDatabase::executeNonQuery( DataCommand& command, const bool isCommandCached, const bool holdCursor )
{
	ub4 numberOfExecution = 1;
	ub4 rowCount = 0;

	sword status;

	// Execute Statement
	DEBUG( "Calling execute on non-query ... " );
	status = OCIStmtExecute( m_hServiceContext, m_StatementHandle, m_hError, ( ub4 )numberOfExecution, ( ub4 )0,
	                         ( CONST OCISnapshot * )NULL, ( OCISnapshot * )NULL, OCI_DEFAULT );
	switch( status )
	{
		case OCI_SUCCESS :
			break;

		case OCI_SUCCESS_WITH_INFO :
		{
			// statement executed, but no data was retrieved, or another warning
			stringstream errorMessage;
			errorMessage << "Execute statement failed [" << getErrorInformation( m_hError, status ) << "]";

			DBWarningException warningEx( errorMessage.str() );
			warningEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
			warningEx.addAdditionalInfo( "location", "OracleDatabase::ExecuteNonQuery" );

			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );

			throw warningEx;
		}

		default :
		{
			stringstream errorMessage;
			errorMessage << "Execute statement failed [" << getErrorInformation( m_hError, status ) << "]";

			DBErrorException errorEx( errorMessage.str() );
			errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
			errorEx.addAdditionalInfo( "location", "OracleDatabase::ExecuteNonQuery" );
			errorEx.setCode( m_LastErrorCode );

			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );

			throw errorEx;
		}
		break;
	}

	//number of processed records
	status = OCIAttrGet ( ( dvoid * )m_StatementHandle, ( ub4 )OCI_HTYPE_STMT, ( dvoid * )&rowCount, ( ub4 * )0, ( ub4 )OCI_ATTR_ROW_COUNT, m_hError );
	if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
	{
		stringstream errorMessage;
		errorMessage << "Getting the number of updated rows failed [" << getErrorInformation( m_hError, status ) << "]";
		throw runtime_error( errorMessage.str() );
	}
	m_LastNumberofAffectedRows = rowCount;
}

DataSet* OracleDatabase::executeQuery( DataCommand& command, const bool isCommandCached, const bool holdCursor )
{
	DataSet *oracleDataSet = NULL;
	ub4 numberOfExecution = 0;
	OCIStmt *hStatement = NULL;

	sword status;

	switch ( command.getCommandType() )
	{
		case DataCommand::SP :
			numberOfExecution = 1;
			hStatement = m_hCursor;
			break;

		case DataCommand::INLINE :
			numberOfExecution = 0;
			hStatement = m_StatementHandle;
			break;

		default :
			throw runtime_error( "Command type invalid. Supported values SP|TEXT" );
	}

	// Execute Statement
	DEBUG( "Calling execute on query ... " );
	status = OCIStmtExecute( m_hServiceContext, m_StatementHandle, m_hError, ( ub4 )numberOfExecution, ( ub4 )0,
	                         ( CONST OCISnapshot * )NULL, ( OCISnapshot * )NULL, OCI_DEFAULT );

	switch( status )
	{
		case OCI_SUCCESS :
			break;

		case OCI_SUCCESS_WITH_INFO :
		{
			// statement executed, but no data was retrieved, or another warning
			stringstream errorMessage;
			errorMessage << "Execute statement warning [" << getErrorInformation( m_hError, status ) << "]";
			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
		}
		break;

		default :
		{
			stringstream errorMessage;
			errorMessage << "Execute statement failed [" << getErrorInformation( m_hError, status ) << "]";

			DBErrorException errorEx( errorMessage.str() );
			errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
			errorEx.addAdditionalInfo( "location", "OracleDatabase::ExecuteQuery" );
			errorEx.setCode( m_LastErrorCode );

			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );

			throw errorEx;
		}
	}

	oracleDataSet = getDataSet( command, isCommandCached, hStatement, holdCursor );

	if ( oracleDataSet == NULL )
		throw runtime_error( "Empty ( NULL ) resultset returned." );

#ifdef EXTENDED_DEBUG
	if ( oracleDataSet->size() != 0 )
		DisplayDataSet( oracleDataSet );
#endif

	return oracleDataSet;
}

DataSet* OracleDatabase::getDataSet( DataCommand& command, const bool isCommandCached, OCIStmt *hStatement, bool holdCursor )
{
	sword status;
	ub4 nResultCols = 0;

	if ( isCommandCached )
	{
		nResultCols = command.getResultColumnCount();
		DEBUG( "Cached no of columns : " << nResultCols );
	}
	else
	{
		//Get number of result columns
		DEBUG( "Getting the number of result set columns ..." );
		status = OCIAttrGet( ( dvoid * )hStatement, ( ub4 )OCI_HTYPE_STMT, ( dvoid * )&nResultCols, ( ub4 * )0, ( ub4 )OCI_ATTR_PARAM_COUNT, m_hError );

		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Getting the number of result columns failed [" << getErrorInformation( m_hError, status ) << "]";
			throw runtime_error( errorMessage.str() );
		}
		DEBUG( "No of columns : " << nResultCols );
	}

	//Define OracleRow as map of Result Columns
	DataRow oracleRow;
	if ( m_FetchClobLocator != NULL )
	{
		FREE_ORA_DESC( m_FetchClobLocator, "fetch clob", ( ub4 )OCI_DTYPE_LOB );
	}
	if ( m_FetchBlobLocator != NULL )
	{
		FREE_ORA_DESC( m_FetchBlobLocator, "fetch blob", ( ub4 )OCI_DTYPE_LOB );
	}

	string clobColumnName = "";
	string blobColumnName = "";
	vector< string > dateColumnName, timestampColumnName;
	int dateColumnNumber=-1, timestampColumnNumber=-1;

	//use of type OCIDateType ** , because OCI functions use address of pointer and we must allocate new			pointer each time we find a new element of this type
	OCIDateTime** timestamp = NULL;
	OCIDate** date = NULL;

	//For each column in the result set
	for ( unsigned int i=0; i<nResultCols; i++ )
	{
		OCIParam *hColumn = NULL;
		string columnName = "";
		ub2	columnDataType = 0;

		DataType::DATA_TYPE columnType = DataType::INVALID_TYPE;

		if ( isCommandCached )
		{
			columnName = command.getResultColumn( i ).getName();
			columnType = command.getResultColumn( i ).getType();
		}
		else
		{
			DEBUG( "Getting column [" << i << "] description ..." );

			//Request desciption for first column
			status = OCIParamGet( ( dvoid * )hStatement, OCI_HTYPE_STMT, m_hError, ( dvoid ** )&hColumn, i + 1 );

			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "Get column description failed [" << getErrorInformation( m_hError, status ) << "] for column " << i + 1;
				throw runtime_error( errorMessage.str() );
			}

			//Retrieve column attributes
			//Retrieve data type attributes
			DEBUG( "Get data type column attribute ... " );

			status = OCIAttrGet( ( dvoid * )hColumn, ( ub4 )OCI_DTYPE_PARAM, ( dvoid * )&columnDataType, ( ub4* )0,
			                     ( ub4 )OCI_ATTR_DATA_TYPE, m_hError );

			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "Get column data type failed [" << getErrorInformation( m_hError, status ) << "] for column " << i + 1;
				throw runtime_error( errorMessage.str() );
			}

			DEBUG( "Column data type: " << columnDataType );
			columnType = OracleDatabaseFactory::getDataType( columnDataType, 0 );

			//Retrieve column name attribute

			DEBUG( "Get column name attribute ... " );

			ub4 columnNameLength = 0;
			char *colName = NULL;
			status = OCIAttrGet( ( dvoid * )hColumn, ( ub4 )OCI_DTYPE_PARAM, ( dvoid ** )&colName, ( ub4 *)&columnNameLength,
			                     ( ub4 )OCI_ATTR_NAME, m_hError);

			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "Get column name failed [" << getErrorInformation( m_hError, status ) << "] for column " << i + 1;
				throw runtime_error( errorMessage.str() );
			}

			if ( columnNameLength > MAX_ORACLE_COLUMN_NAME )
			{
				columnName = string( colName, MAX_ORACLE_COLUMN_NAME );
				TRACE( "Column name too long [" << columnNameLength << "]. Truncated to [" << columnName << "]" );
			}
			else
				columnName = string( colName, columnNameLength );

			DEBUG( "Column name [" << columnName << "] name length : " << columnNameLength );
		} // if not cached

		unsigned int columnDimension = 0;
		unsigned int columnScale = 0;

		// allocate clob
		if ( columnType == DataType::LARGE_CHAR_TYPE )
		{
			if ( m_FetchClobLocator != NULL )
				throw logic_error( "Multiple clobs in resultset not supported." );

			clobColumnName = columnName;

			DEBUG2( "Allocate CLOB descriptor  ... " );
			status = OCIDescriptorAlloc( ( dvoid * )m_hEnv, ( dvoid ** )&m_FetchClobLocator, ( ub4 )OCI_DTYPE_LOB, ( size_t )0, ( dvoid ** )0 );
			if ( status != OCI_SUCCESS )
			{
				stringstream errorMessage;
				errorMessage << "Alloc CLOB descriptor failed [" << getErrorInformation( m_hError, status ) << "]";
				throw runtime_error( errorMessage.str() );
			}

			// no need to create the temporary lob ( OCIStmtFetch will overwrite this with a permanent LOB )
			/*DEBUG2( "Create temporary CLOB ... ");
			status = OCILobCreateTemporary( m_hServiceContext, m_hError, m_FetchClobLocator, ( ub2 ) 0,
				SQLCS_IMPLICIT, OCI_TEMP_CLOB, OCI_ATTR_NOCACHE, OCI_DURATION_SESSION );
			if ( status != OCI_SUCCESS )
			{
				stringstream errorMessage;
				errorMessage << "Create CLOB failed [" << getErrorInformation( m_hError, status ) << "]";
				throw runtime_error( errorMessage.str() );
			}*/
		}
		// allocate blob
		else if ( columnType == DataType::BINARY )
		{
			if ( m_FetchBlobLocator != NULL )
				throw logic_error( "Multiple blobs in resultset not supported." );

			blobColumnName = columnName;

			DEBUG2( "Allocate BLOB descriptor  ... " );
			status = OCIDescriptorAlloc( ( dvoid * )m_hEnv, ( dvoid ** )&m_FetchBlobLocator, ( ub4 )OCI_DTYPE_LOB, ( size_t )0, ( dvoid ** )0 );
			if ( status != OCI_SUCCESS )
			{
				stringstream errorMessage;
				errorMessage << "Alloc BLOB descriptor failed [" << getErrorInformation( m_hError, status ) << "]";
				throw runtime_error( errorMessage.str() );
			}
		}
		else if( columnType == DataType::DATE_TYPE ) //date
		{
			//OCIDate**
			date = new OCIDate*;
			//if( ( m_Date.empty() ) || ( m_Date[ dateColumnNumber ] != NULL ) )
			//{
			DEBUG( "Allocate DATE descriptor  ... " );
			status = OCIDescriptorAlloc( ( dvoid * )m_hEnv, ( dvoid ** )date, ( ub4 )OCI_DTYPE_DATE, ( size_t )0, ( dvoid ** )0 );
			if ( status != OCI_SUCCESS )
			{
				stringstream errorMessage;
				errorMessage << "Alloc DATE descriptor failed [" << getErrorInformation( m_hError, status ) << "]";
				throw runtime_error( errorMessage.str() );
			}

			dateColumnName.push_back( columnName );
			dateColumnNumber++;
			//}

			//TRACE( "Column is of type DATE, use to_char in stored procedure to convert date to char" )

		}
		else if( columnType == DataType::TIMESTAMP_TYPE ) //timestamp
		{
			//OCIDateTime**
			timestamp = new OCIDateTime*;
			//if( ( m_Timestamp.empty() ) || ( m_Timestamp[ timestampColumnNumber ] != NULL ) )
			//{
			DEBUG( "Allocate TIMESTAMP descriptor  ... " );
			status = OCIDescriptorAlloc( ( dvoid * )m_hEnv, ( dvoid ** )timestamp , ( ub4 )OCI_DTYPE_TIMESTAMP, ( size_t )0, ( dvoid ** )0 );
			if ( status != OCI_SUCCESS )
			{
				stringstream errorMessage;
				errorMessage << "Alloc TIMESTAMP descriptor failed [" << getErrorInformation( m_hError, status ) << "]";
				throw runtime_error( errorMessage.str() );
			}

			timestampColumnNumber++;
			timestampColumnName.push_back( columnName );
			//}

			//TRACE( "Column is of type TIMESTAMP, use to_char in stored procedure to convert timestamp to char" )
		}
		else //not clob, nor date
		{
			if ( isCommandCached )
			{
				columnDimension = command.getResultColumn( i ).getDimension();
				columnScale = command.getResultColumn( i ).getScale();
			}
			else
			{
				ub2 columnSize = 0;
				ub4	columnLength = 0;
				ub4 columnPrecision = 0;
				ub4 columnPrecision1 = 0;
				ub4 columnScale1 = 0;
				ub4 columnScale2 = 0;

				// Retrieve column size attribute
				DEBUG( "Get column size attribute ... " );
				status = OCIAttrGet( ( dvoid * )hColumn, ( ub4 )OCI_DTYPE_PARAM, ( dvoid * )&columnSize, ( ub4 *)&columnLength,
				                     ( ub4 )OCI_ATTR_DATA_SIZE, m_hError );

				if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
				{
					stringstream errorMessage;
					errorMessage << "Get column size failed [" << getErrorInformation( m_hError, status )
					             << "] for column " << i + 1 << " column name [" << columnName << "]";
					throw runtime_error( errorMessage.str() );
				}
				DEBUG( "Column size : " << columnSize << " length : " << columnLength );
				columnDimension = columnSize + 1;

				//Retrieve column precision attribute
				DEBUG( "Get precision attribute ... " );
				status = OCIAttrGet( ( dvoid * )hColumn, ( ub4 )OCI_DTYPE_PARAM, ( dvoid * )&columnPrecision, ( ub4 * )&columnPrecision1,
				                     ( ub4 )OCI_ATTR_PRECISION, m_hError );

				if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
				{
					stringstream errorMessage;
					errorMessage << "Get column precision failed [" << getErrorInformation( m_hError, status )
					             << "] for column " << i + 1 << " column name [" << columnName << "]";
					TRACE( errorMessage.str() );
				}
				DEBUG( "Column precision : " << ( int )columnPrecision  << "; precision1 : " << ( int )columnPrecision1 );

				//Retrieve column scale attribute
				DEBUG( "Get scale attribute ... " );
				status = OCIAttrGet( ( dvoid * )hColumn, ( ub4 )OCI_DTYPE_PARAM, ( dvoid * )&columnScale1, ( ub4 * )&columnScale2,
				                     ( ub4 )OCI_ATTR_SCALE, m_hError );

				if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
				{
					stringstream errorMessage;
					errorMessage << "Get column scale failed [" << getErrorInformation( m_hError, status )
					             << "] for column " << i + 1 << " column name [" << columnName << "]";
					TRACE( errorMessage.str() );
				}

				columnScale = columnScale1;
				DEBUG( "Column scale: " << ( int )columnScale1 << "; Column scale1: " << ( int )columnScale2 );

				columnType = OracleDatabaseFactory::getDataType( columnDataType, columnDimension );
			}
		}

		// Allocate data buffer for every column, here will be fetch the cuurent data for the column
		DataColumnBase* oracleColumn = NULL;

		try
		{
			oracleColumn = OracleDatabaseFactory::internalCreateColumn( columnType, columnDimension, columnScale, columnName );

			// add it to cache ( if it is cacheable )
			if ( !isCommandCached && command.isCacheable() )
			{
				DEBUG( "Command is not cached, but cacheable. Adding result column ..." );
				command.addResultColumn( i, oracleColumn );
			}

			//Define result column
			OCIDefine *hDefine;
			DEBUG2( "Define result column ... " );
			sb2 indicator;

			if ( columnType == DataType::LARGE_CHAR_TYPE )
			{
				status = OCIDefineByPos( hStatement, &hDefine, m_hError, i + 1,	( dvoid * )&m_FetchClobLocator, sizeof( OCIClobLocator * ),
				                         OracleDatabaseFactory::getOracleDataType( columnType ), ( dvoid * )&indicator, ( ub2 * )0, ( ub2 * )0, OCI_DEFAULT );
			}
			else if ( columnType == DataType::BINARY )
			{
				status = OCIDefineByPos( hStatement, &hDefine, m_hError, i + 1,	( dvoid * )&m_FetchBlobLocator, sizeof( OCIBlobLocator * ),
				                         OracleDatabaseFactory::getOracleDataType( columnType ), ( dvoid * )&indicator, ( ub2 * )0, ( ub2 * )0, OCI_DEFAULT );
			}
			else if( columnType == DataType::DATE_TYPE )
			{
				status = OCIDefineByPos( hStatement, &hDefine, m_hError, i + 1,	( dvoid * )date, sizeof( OCIDate * ), OracleDatabaseFactory::getOracleDataType( columnType ), ( dvoid * )&indicator, ( ub2 * )0, ( ub2 * )0, OCI_DEFAULT );
				m_Date.push_back( date );
				//delete date;
			}
			else if( columnType == DataType::TIMESTAMP_TYPE )
			{
				status = OCIDefineByPos( hStatement, &hDefine, m_hError, i + 1,	( dvoid * )timestamp, sizeof( OCIDateTime * ), OracleDatabaseFactory::getOracleDataType( columnType ), ( dvoid * )&indicator, ( ub2 * )0, ( ub2 * )0, OCI_DEFAULT );
				m_Timestamp.push_back( timestamp );
				//add in vector the pointer to OCIDateTime, so we can find later the value
				//delete timestamp;
			}
			else
			{
				status = OCIDefineByPos( hStatement, &hDefine, m_hError, i + 1, ( dvoid * )oracleColumn->getStoragePointer(),
				                         oracleColumn->getDimension(), OracleDatabaseFactory::getOracleDataType( columnType ),
				                         ( dvoid * )&indicator, ( ub2 * )0, ( ub2 * )0, OCI_DEFAULT );
			}

			if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
			{
				stringstream errorMessage;
				errorMessage << "Define column by position failed [" << getErrorInformation( m_hError, status ) <<
				             "] for column " << i + 1 << " column name [" << columnName << "]";
				throw runtime_error( errorMessage.str() );
			}
			DEBUG2( "Define column by position successful." );

			// Insert in the OracleRow the current OracleResultColumn pair : columnName, OracleColumn (column data and description)
			// OracleRow will be the buffer that will receive data after fetch
			oracleRow.insert( makeColumn( columnName, oracleColumn ) );
		}
		catch( ... )
		{
			if ( oracleColumn != NULL )
			{
				delete oracleColumn;
				oracleColumn = NULL;
			}
			throw;
		}
	}

	DEBUG2( "Creating result dataset ... " );
	DataSet* oracleDataSet = NULL;

	try
	{
		oracleDataSet = new DataSet();
		DEBUG( "Fetching first row ..." );

		// changed from ocistmtfetch ( TODO: check for changes )
		status = OCIStmtFetch2( hStatement, m_hError, 1, OCI_FETCH_NEXT, 0, OCI_DEFAULT );
		if ( status == OCI_NO_DATA )
		{
			DEBUG( "No result data" );
		}
		else if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Fetch data failed [" << getErrorInformation( m_hError, status ) << "]";

			DBErrorException errorEx( errorMessage.str() );
			errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
			errorEx.addAdditionalInfo( "location", "OracleDatabase::ExecuteQuery" );
			errorEx.setCode( m_LastErrorCode );

			TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
			throw errorEx;
		}
		else
		{
			//oracleRow.DumpHeader();

			if ( holdCursor )
			{
				stringstream errorMessage;

				/* allocate descriptor with OCIDescriptorAlloc() */
				sword err = OCIDescriptorAlloc( ( dvoid * )m_hEnv, ( dvoid ** )&m_HoldCursorRowId,
				                                ( ub4 )OCI_DTYPE_ROWID, ( size_t )0, ( dvoid ** ) 0 );

				if ( ( err != OCI_SUCCESS ) && ( err != OCI_SUCCESS_WITH_INFO ) )
				{
					errorMessage << "Alloc rowid descriptor failed [" << getErrorInformation( m_hError, status ) << "]";
					throw runtime_error( errorMessage.str() );
				}

				err = OCIAttrGet( ( dvoid* )hStatement, OCI_HTYPE_STMT,
				                  ( dvoid* )m_HoldCursorRowId, ( ub4 * )0, OCI_ATTR_ROWID, ( OCIError * )m_hError );
				if ( ( err != OCI_SUCCESS ) && ( err != OCI_SUCCESS_WITH_INFO ) )
				{
					errorMessage << "Get rowid failed [" << getErrorInformation( m_hError, status ) << "]";
					throw runtime_error( errorMessage.str() );
				}
			}

			if ( m_FetchClobLocator != NULL )
			{
				ub4 lenp = 0;

				status = OCILobGetLength( m_hServiceContext, m_hError, m_FetchClobLocator, &lenp );
				if ( status == OCI_INVALID_HANDLE )
				{
					DEBUG( "Read a NULL CLOB" );
					oracleRow[ clobColumnName ]->setDimension( 0 );
				}
				else
				{
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Error obtaining CLOB length [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}


					DEBUG2( "Opening CLOB for read - size [" << lenp << "] ... ");

					status = OCILobOpen( m_hServiceContext, m_hError, m_FetchClobLocator, OCI_LOB_READONLY );
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Open CLOB ( read ) failed [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}

					try
					{
						oracleRow[ clobColumnName ]->setDimension( lenp );
						DEBUG2( "Reading CLOB ... " );

						string tempLobStr;
						vector<char> tempLob( lenp );

						do
						{
							ub4 numberOfReadBytes = lenp;
							status = OCILobRead( m_hServiceContext, m_hError, m_FetchClobLocator, &numberOfReadBytes,
												1, &tempLob[0], lenp, ( dvoid * )0, 0, 0, SQLCS_IMPLICIT );
							tempLobStr = tempLobStr + string( &tempLob[0], numberOfReadBytes );
						}
						while ( status == OCI_NEED_DATA );

						if ( status != OCI_SUCCESS )
						{

							stringstream errorMessage;
							errorMessage << "Read CLOB failed [" << getErrorInformation( m_hError, status ) << "]";
							throw runtime_error( errorMessage.str() );
						}

						static_cast< OracleColumn< string >* >( oracleRow[ clobColumnName ] )->setValue( tempLobStr );

						DEBUG2( "No of bytes read : " << tempLobStr.size() );
					}
					catch ( ... )
					{
						status = OCILobClose( m_hServiceContext, m_hError, m_FetchClobLocator );
						if ( status != OCI_SUCCESS )
							DEBUG( getErrorInformation( m_hError, status ) )
						throw;
					}

					DEBUG2( "Closing CLOB ... " );
					status = OCILobClose( m_hServiceContext, m_hError, m_FetchClobLocator );
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Close CLOB failed [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}
				}
			}

			if ( m_FetchBlobLocator != NULL )
			{
				ub4 lenp = 0;

				status = OCILobGetLength( m_hServiceContext, m_hError, m_FetchBlobLocator, &lenp );
				if ( status == OCI_INVALID_HANDLE )
				{
					DEBUG( "Read a NULL BLOB" );
					oracleRow[ blobColumnName ]->setDimension( 0 );
				}
				else
				{
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Error obtaining BLOB length [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}

					DEBUG2( "Opening BLOB for read - size [" << lenp << "] ... ");

					status = OCILobOpen( m_hServiceContext, m_hError, m_FetchBlobLocator, OCI_LOB_READONLY );
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Open BLOB ( read ) failed [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}

					oracleRow[ blobColumnName ]->setDimension( lenp );
					//TODO POate fi eliminata e deja setat tipul
					oracleRow[ blobColumnName ]->setType( DataType::BINARY );

					DEBUG2( "Reading BLOB ... " );

					unsigned char *tempLob = NULL;
					try
					{
						tempLob = new unsigned char[ lenp ];
						status = OCILobRead( m_hServiceContext, m_hError, m_FetchBlobLocator, &lenp,
						                     1, tempLob, lenp, ( dvoid * )0, 0, 0, SQLCS_IMPLICIT );

						string tempLobStr = Base64::encode( tempLob, lenp );
						dynamic_cast< OracleColumn< string >* >( oracleRow[ blobColumnName ] )->setValue( tempLobStr );

						if ( status != OCI_SUCCESS )
						{
							stringstream errorMessage;
							errorMessage << "Read BLOB failed [" << getErrorInformation( m_hError, status ) << "]";
							throw runtime_error( errorMessage.str() );
						}
						DEBUG2( "No of bytes read : " << lenp );
					}
					catch( ... )
					{
						if ( tempLob != NULL )
						{
							delete[] tempLob;
							tempLob = NULL;
						}
						throw;
					}
					if ( tempLob != NULL )
					{
						delete[] tempLob;
						tempLob = NULL;
					}

					DEBUG2( "Closing BLOB ... " );
					status = OCILobClose( m_hServiceContext, m_hError, m_FetchBlobLocator );
					if ( status != OCI_SUCCESS )
					{
						stringstream errorMessage;
						errorMessage << "Close BLOB failed [" << getErrorInformation( m_hError, status ) << "]";
						throw runtime_error( errorMessage.str() );
					}
				}
			}

			for( unsigned int i = 0; i< m_Date.size(); i++ )
			{
				if( *(m_Date[ i ]) != NULL )
				{
					ub4 bufflen = DateFormat.length();
					text* buffer = new text[ bufflen ];
					string lang = "American";

					try
					{
						//OCIDate* date = m_Date[ i ];
						status = OCIDateToText( m_hError, *(m_Date[ i ]), (text*)DateFormat.c_str(), DateFormat.length(), (text*)lang.c_str(), lang.length(), &bufflen, buffer );

						if( status != OCI_SUCCESS )
						{
							stringstream errorMessage;
							errorMessage << "Convert DATE to TEXT failed : [" << getErrorInformation( m_hError, status ) << "]";
							throw runtime_error( errorMessage.str() );
						}
						string currentDate( (char*)buffer, bufflen );
						oracleRow[ dateColumnName[ i ] ]->setDimension( 0 );
						dynamic_cast< OracleColumn< string >* >( oracleRow[ dateColumnName[ i ] ] )->setValue( currentDate );
					}
					catch( ... )
					{
						if( buffer != NULL )
						{
							delete[] buffer;
							buffer = NULL;
						}
						throw;
					}
					if( buffer != NULL )
					{
						delete[] buffer;
						buffer = NULL;
					}
				}
			}

			for( unsigned int i = 0; i< m_Timestamp.size(); i++ )
			{
				if( *(m_Timestamp[ i ]) != NULL )
				{
					ub4 bufflen = TimestampFormat.length();
					text* buffer = new text[ bufflen ];
					string lang = "American";
					ub1 fsprec = 100;

					try
					{
						//OCIDateTime* timestamp = m_Timestamp[ i ];
						status = OCIDateTimeToText( m_hEnv, m_hError, *(m_Timestamp[ i ]), (text*)TimestampFormat.c_str(), TimestampFormat.length(), fsprec, (text*)lang.c_str(), lang.length(), &bufflen, buffer );

						if( status != OCI_SUCCESS )
						{
							stringstream errorMessage;
							errorMessage << "Convert TIMESTAMP to TEXT failed : [" << getErrorInformation( m_hError, status ) << "]";
							throw runtime_error( errorMessage.str() );
						}
						string currentTimestamp( (char*)buffer, bufflen );
						oracleRow[ timestampColumnName[ i ] ]->setDimension( 0 );
						dynamic_cast< OracleColumn< string >* >( oracleRow[ timestampColumnName[ i ] ] )->setValue( currentTimestamp );
					}
					catch( ... )
					{
						if( buffer != NULL )
						{
							delete[] buffer;
							buffer = NULL;
						}
						throw;
					}
					if( buffer != NULL )
					{
						delete[] buffer;
						buffer = NULL;
					}
				}
			}
			//unsigned int rowsFetched = 1;

			//While more data in the result set
			while ( status != OCI_NO_DATA )
			{
				DEBUG2( "Copying row to dataset" );
				oracleRow.Sync();

				//Copy the OracleRow buffer into a OracleNewRow
				DataRow* oracleNewRow = new DataRow( oracleRow );

				DEBUG2( "Appending row to dataset" );
				oracleDataSet->push_back( oracleNewRow );

				oracleRow.Clear();

				// Fetch next row in the OracleRow buffer
				DEBUG( "Fetching another row" );
				//status = OCIStmtFetch2( hStatement, m_hError, 1, OCI_FETCH_NEXT, 1, OCI_DEFAULT );
				status = OCIStmtFetch( hStatement, m_hError, 1, OCI_FETCH_NEXT, OCI_DEFAULT );
				if ( ( status != OCI_SUCCESS ) && ( status != OCI_NO_DATA ) && ( status != OCI_SUCCESS_WITH_INFO ) )
				{
					stringstream errorMessage;
					errorMessage << "Fetch data failed [" << getErrorInformation( m_hError, status ) << "]";

					DBErrorException errorEx( errorMessage.str() );
					errorEx.addAdditionalInfo( "statement", command.getModifiedStatementString() );
					errorEx.addAdditionalInfo( "location", "Db2Database::ExecuteQuery" );
					errorEx.setCode( m_LastErrorCode );

					TRACE( errorMessage.str() << " in [" << command.getModifiedStatementString() << "]" );
					throw errorEx;
				}

				//oracleRow.DumpHeader();
				//rowsFetched++;
			}
		}

		DEBUG2( "Fetch successfull." );
	}
	catch( ... )
	{
		if ( oracleDataSet != NULL )
		{
			try
			{
				delete oracleDataSet;
				oracleDataSet = NULL;
			} catch( ... ) {}
		}
		throw;
	}
	return oracleDataSet;
}

void OracleDatabase::ReleaseCursor( const bool checkConn )
{
	if ( m_HoldCursorRowId == NULL )
		return;

	sword ora_status = 0;
	try
	{
		ora_status = OCIDescriptorFree( ( dvoid* )m_HoldCursorRowId, ( ub4 )OCI_DTYPE_ROWID  );
		if ( ora_status != OCI_SUCCESS )
			throw runtime_error( "Release cursor failed" );
	}
	catch( ... )
	{
		TRACE( "Free descriptor [rowid] failed [" << getErrorInformation( m_StatementHandle, ora_status, OCI_HTYPE_STMT, checkConn ) << "]" );
	}

	m_HoldCursorRowId = NULL;
}

//-------------------------------
//Begin Transaction
//-------------------------------
void OracleDatabase::BeginTransaction( const bool readonly )
{
	sword status;

	if ( m_hServiceContext == NULL )
		throw runtime_error( "Service context is NULL ..." );

	if ( m_hError == NULL )
		throw runtime_error( "Error handle is NULL ..." );

	try
	{
		DEBUG2( "Allocating transaction handle " );
		status = OCIHandleAlloc( ( dvoid * )m_hEnv, ( dvoid ** )&m_hTransaction, OCI_HTYPE_TRANS, ( size_t )0, ( dvoid ** )0 );
		getErrorInformation( m_hError, status );

		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "BeginTransaction - alloc handle failed [" << getErrorInformation( m_hEnv, status, OCI_HTYPE_ENV ) << "]";
			throw runtime_error( errorMessage.str() );
		}

		// Set Transaction attribute in Service context
		DEBUG2( "Set transaction attribute ... " );
		status = OCIAttrSet( ( dvoid * )m_hServiceContext, OCI_HTYPE_SVCCTX, ( dvoid * )m_hTransaction,
		                     ( ub4 ) 0, OCI_ATTR_TRANS, ( OCIError * )m_hError );

		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "BeginTransaction - set transaction attribute failed [" << getErrorInformation( m_hError, status ) << "]";
			TRACE( errorMessage.str() );
		}

		DEBUG( "Starting transaction ..." );

		if ( readonly )
			status = OCITransStart( m_hServiceContext, m_hError, 60, OCI_TRANS_READONLY );
		else
			status = OCITransStart( m_hServiceContext, m_hError, 60, OCI_TRANS_READWRITE );

		if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
		{
			stringstream errorMessage;
			errorMessage << "Start transaction failed [" << getErrorInformation( m_hError, status ) << "]";
			throw runtime_error( errorMessage.str() );
		}
	}
	catch( ... )
	{
		FREE_ORA_HANDLE( m_hTransaction, "transaction", OCI_HTYPE_TRANS );
		throw;
	}
}

//End Transaction
void OracleDatabase::EndTransaction( const TransactionType::TRANSACTION_TYPE transactionType, const bool throwOnError )
{
	sword status;
	//int transactionType = getOracleTransactionType( trType );

	try
	{
		DEBUG( "Ending transaction ..." );

		switch( transactionType )
		{
			case TransactionType::COMMIT :

				status = OCITransCommit( m_hServiceContext, m_hError, OCI_DEFAULT );

				if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
				{
					stringstream errorMessage;
					errorMessage << "Commit transaction failed [" << getErrorInformation( m_hError, status ) << "]";
					throw runtime_error( errorMessage.str() );
				}
				break;

			case TransactionType::ROLLBACK :

				status = OCITransRollback( m_hServiceContext, m_hError, OCI_DEFAULT );

				if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
				{
					stringstream errorMessage;
					errorMessage << "Rollback transaction failed [" << getErrorInformation( m_hError, status ) << "]";
					throw runtime_error( errorMessage.str() );
				}
				break;

			default :
				throw invalid_argument( "transaction type" );
				break;
		}
	}
	catch( ... )
	{
		FREE_ORA_HANDLE( m_hTransaction, "transaction", OCI_HTYPE_TRANS );
		if ( throwOnError )
			throw;
	}
	FREE_ORA_HANDLE( m_hTransaction, "transaction", OCI_HTYPE_TRANS );
}

int OracleDatabase::getOracleTransactionType( TransactionType::TRANSACTION_TYPE transactiontype )
{
	/*
	switch( transactiontype )
	{
		case TransactionType::COMMIT :
			return ORACLE_COMMIT;
		case TransactionType::ROLLBACK :
			return ORACLE_ROLLBACK;
	}
	throw invalid_argument( "type" );
	*/
	return 0;
}

bool OracleDatabase::IsConnected()
{
	if ( !m_IsConnected )
	{
		if( m_IsReconnecting )
		{
			DEBUG( "Sleeping 10 seconds before trying to reconnect to Oracle... " );
			sleep( 10 );

			try
			{
				Connect( m_ConnectionString );
			}
			catch( const std::exception& ex )
			{
				DEBUG( "Failed to reconnect. [" << ex.what() << "]" );
			}
			catch( ... )
			{
				DEBUG( "Failed to reconnect." );
			}
		}
	}

	ub4 serverStatus = OCI_SERVER_NOT_CONNECTED;
	sword status = OCIAttrGet( ( dvoid * )m_hServer, OCI_HTYPE_SERVER, ( dvoid * )&serverStatus, ( ub4 * )0, OCI_ATTR_SERVER_STATUS, m_hError );

	if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
	{
		stringstream errorMessage;
		errorMessage << "Getting the server status failed [" << getErrorInformation( m_hError, status ) << "]";

		TRACE( errorMessage.str() );
	}

	m_IsConnected = ( serverStatus == OCI_SERVER_NORMAL );

	return m_IsConnected;
}

//-----------------------------------------------------------
//Display Error Information after each Oracle CLI - SQL function
//-----------------------------------------------------------
string OracleDatabase::getErrorInformation( dvoid *m_hError, sword status, ub4 handleType, const bool connCheck )
{
	if ( status == OCI_SUCCESS )
		return "OCI_SUCCESS";

	const unsigned int errorBufferLength = 512;

	text errbuf[ errorBufferLength ];
	sb4 errCode = 0;
	sword moreInfo;
	int recordNo = 1;
	stringstream errorBuffer;

	switch( status )
	{
		case OCI_SUCCESS:
			errorBuffer << "OCI_SUCCESS";
			break;

		case OCI_SUCCESS_WITH_INFO:
			memset( errbuf, 0, errorBufferLength );
			moreInfo = OCIErrorGet( m_hError, ( ub4 )recordNo, ( text * ) NULL, &errCode,
			                        errbuf, ( ub4 )sizeof( errbuf ), handleType );

			errorBuffer << "Error - OCI_SUCCESS_WITH_INFO. Error code [" << errCode << "] message [" << errbuf << "]";
			break;

		case OCI_NEED_DATA:
			errorBuffer << "Error - OCI_NEED_DATA";
			break;

		case OCI_NO_DATA:
			errorBuffer << "Error - OCI_NODATA";
			break;

		case OCI_ERROR:
			memset( errbuf, 0, errorBufferLength );
			moreInfo = OCIErrorGet( m_hError, ( ub4 )recordNo, ( text * )NULL, &errCode, errbuf, ( ub4 )sizeof( errbuf ), handleType );
			errorBuffer << "Error - OCI_ERROR. Error code [" << errCode << "]";
			m_LastErrorCode = string( "ORA-" ) + StringUtil::ToString( errCode );

			while ( moreInfo == OCI_SUCCESS )
			{
				errorBuffer << ". More info #" << recordNo << " [" << errbuf << "]";
				recordNo++;
				memset( errbuf, 0, errorBufferLength );
				moreInfo = OCIErrorGet( m_hError, ( ub4 )recordNo, ( text * )NULL, &errCode, errbuf, ( ub4 )sizeof( errbuf ), handleType );
			}

			// don't check connection ( maybe the error occured in connect )
			if ( !connCheck )
				break;

			// find if we were disconnected
			if ( IsConnected() )
			{
				DEBUG( "Connection is still up ..." );
			}
			else // serverStatus == OCI_SERVER_NOT_CONNECTED
			{
				if ( !m_IsReconnecting )
				{
					TRACE( "Connection is down ..." );
					m_IsReconnecting = true;

					Disconnect();

					throw DBConnectionLostException( errorBuffer.str() );
				}
			}

			break;

		case OCI_INVALID_HANDLE:
			errorBuffer << "Error - OCI_INVALID_HANDLE";
			break;

		case OCI_STILL_EXECUTING:
			errorBuffer << "Error - OCI_STILL_EXECUTE";
			break;

		case OCI_CONTINUE:
			errorBuffer << "Error - OCI_CONTINUE";
			break;

		default:
			errorBuffer << "Error - OCI return code [" << status << "]" ;
			break;
	}

	return errorBuffer.str();
}

//-------------------------------------------------------------
// Assigning message value
//-------------------------------------------------------------
/*void OracleDatabase::AssignMessageVal( const string& value, void **messageField )
{
	// OBS : variabilele trebuie initializate cu o valoare
	sword status = 0;

	// OBS : message folosit ca parametru si variabila locala
	DEBUG( "Assigning value [" << value << "] ..." );

	status = OCIStringAssignText( m_hEnv, m_hError,( CONST text * )value.c_str(), value.length(), ( OCIString** )messageField );
    if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
	{
		stringstream errorMessage;
		errorMessage << "Assigning string value to message failed [" << getErrorInformation( m_hError, status ) << "]";
		throw runtime_error( errorMessage.str() );
	}
}*/

string OracleDatabase::RawToString( const OCIRaw* value )
{
	if ( value == NULL )
		return "";

	return Base64::encode( ( unsigned char * )OCIRawPtr( m_hEnv, value ), OCIRawSize( m_hEnv, value ) );
}

// convert string -> OCIRaw *
OCIRaw* OracleDatabase::StringToRaw( const string& value )
{
	OCIRaw* result = NULL;
	string decodedValue = Base64::decode( value );

	sword status = OCIRawAssignBytes( m_hEnv, m_hError, ( const ub1* )decodedValue.c_str(), decodedValue.length(), &result );
	if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
	{
		stringstream errorMessage;
		errorMessage << "Unable to decode string to raw [" << getErrorInformation( m_hError, status ) << "]";
		TRACE( errorMessage.str() );
		throw runtime_error( errorMessage.str() );
	}
	return result;
}

// obtain a type from a typename ( AQ )
OCIType* OracleDatabase::getMessageType( const string& typeName, const string& userName )
{
	sword status = 0;
	OCIType *messageType = ( OCIType * )0;

	string typeKey = typeName + userName;
	if ( m_TypeCache.Contains( typeKey ) )
		return m_TypeCache[ typeKey ];

	DEBUG( "Obtaining message type..." );

	// OBS : citat din documentatie : To free the type obtained by this function, OCIObjectUnpin()
	//		or OCIObjectPinCountReset() may be called.
	// TODO : eliberare tip
	if ( userName.length() > 0 )
	{
		status = OCITypeByName( m_hEnv, m_hError, m_hServiceContext, ( CONST text * )userName.c_str(),
		                        ( ub4 )userName.length(), ( CONST text * )typeName.c_str(),
		                        ( ub4 )typeName.length(), ( text * )0, 0, OCI_DURATION_SESSION, OCI_TYPEGET_ALL, &messageType );
	}
	else
	{
		status = OCITypeByName( m_hEnv, m_hError, m_hServiceContext, ( CONST text * )m_ConnectionString.getUserName().c_str(),
		                        ( ub4 )m_ConnectionString.getUserName().length(), ( CONST text * )typeName.c_str(),
		                        ( ub4 )typeName.length(), ( text * )0, 0, OCI_DURATION_SESSION, OCI_TYPEGET_ALL, &messageType );
	}

	if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
	{
		stringstream errorMessage;
		errorMessage << "Obtaining message type failed [" << getErrorInformation( m_hError, status ) << "]";
		TRACE( errorMessage.str() );
		throw runtime_error( errorMessage.str() );
	}

	m_TypeCache.Add( typeKey, messageType );
	return messageType;
}

void OracleDatabase::freeOCIArrays( const ParametersVector& vectorOfParameters )
{
	for (size_t i = 0; i < vectorOfParameters.size(); ++i )
	{
		if ( vectorOfParameters[i]->getType() == DataType::ARRAY )
		{
			const OracleArrayParameter& oracleParameter = static_cast< const OracleArrayParameter &>( *vectorOfParameters[i] );	
			if ( *oracleParameter.getOCIArray() != NULL )
			{
				sword status = OCIObjectFree( m_hEnv, m_hError, *oracleParameter.getOCIArray(), OCI_OBJECTFREE_FORCE );
				*oracleParameter.getOCIArray() = NULL;
				if ( ( status != OCI_SUCCESS ) && ( status != OCI_SUCCESS_WITH_INFO ) )
				{
					TRACE( "Free temporary arrary failed [" << getErrorInformation( m_hError, status ) << "]" );
				}
			}
		}
	}
}

void OracleDatabase::initXmlContext()
{
#ifdef ORACLE_HAS_AQ
	if ( m_XmlContext == NULL )
	{
		// initialize xml context
		OCIDuration duration = OCI_DURATION_SESSION;
		ocixmldbparam params[ 1 ];

		params[ 0 ].name_ocixmldbparam = XCTXINIT_OCIDUR;
		params[ 0 ].value_ocixmldbparam = &duration;

		m_XmlContext = OCIXmlDbInitXmlCtx( m_hEnv, m_hServiceContext, m_hError, params, 1 );
		if ( m_XmlContext == NULL )
		{
			stringstream errorMessage;
			errorMessage << "Failed to create the XML context [" << getErrorInformation( m_hError, OCI_ERROR ) << "]";
			TRACE( errorMessage.str() );
			throw runtime_error( errorMessage.str() );
		}
	}
#else
	throw logic_error( "Not implemented : Built without AQ support" );
#endif
}
