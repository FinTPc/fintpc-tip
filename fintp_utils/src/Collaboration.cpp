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

#include "Collaboration.h"
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>

#include "TimeUtil.h"
#include "PlatformDeps.h"
#include "Log.h"

#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

using namespace std;
using namespace FinTP;

unsigned int Collaboration::m_Static = 0;
unsigned int Collaboration::m_Contor = 0;

pthread_once_t Collaboration::KeysCreate = PTHREAD_ONCE_INIT;
pthread_key_t Collaboration::CollaborationKey;

Collaboration Collaboration::m_Instance;

Collaboration::Collaboration() 
{
	int onceResult = pthread_once( &Collaboration::KeysCreate, &Collaboration::CreateKeys );
	if ( 0 != onceResult )
	{
		TRACE_LOG( "Unable to create Collaboration keys [" << onceResult << "]" );
	}
}

Collaboration::~Collaboration()
{
}

void Collaboration::CreateKeys()
{
	cout << "Thread [" << pthread_self() << "] creating collaboration keys..." << endl;
	
	int keyCreateResult = pthread_key_create( &Collaboration::CollaborationKey, &Collaboration::DeleteCollaborationIds );
	if ( 0 != keyCreateResult )
	{
		TRACE_LOG( "Unable to create thread key Collaboration::CollaborationKey [" << keyCreateResult << "]" );
	}
}

void Collaboration::setGuidSeed( const unsigned int value ) 
{
	m_Static = value; 
}

void Collaboration::DeleteCollaborationIds( void* data )
{
	string* threadCorrelId = ( string* )data;
	if ( threadCorrelId != NULL )
		delete threadCorrelId;
	
	int setSpecificResult = pthread_setspecific( Collaboration::CollaborationKey, NULL );
	if ( 0 != setSpecificResult )
	{
		TRACE_LOG( "Set thread specific CollaborationKey failed [" << setSpecificResult << "]" );
	}
}

string Collaboration::EmptyGuid()
{
	return "00000000-00000000-00000000";	
}

string Collaboration::GenerateGuid()
{
	stringstream guidStream;
	
	time_t ltime;
	time( &ltime );
		
	string *collaborationId = ( string * )pthread_getspecific( Collaboration::CollaborationKey );
	if( collaborationId == NULL ) 
	{
		stringstream collaborationIdStr;
		
		boost::crc_16_type crcer;

		// compute thread id hash
		pthread_t threadid = pthread_self();
		crcer.process_bytes( ( unsigned char* )&threadid, sizeof( threadid ) );
		boost::uint16_t thread_hash = crcer.checksum();

		// compute pid hash
		long pid = Process::GetPID();
		crcer.process_bytes( ( unsigned char* )&pid, sizeof( pid ) );
		boost::uint16_t pid_hash = crcer.checksum();

		collaborationIdStr << setbase( 16 ) << setfill( '0' ) << setw( 4 ) << thread_hash
			<< setw( 4 ) << Platform::GetUIDHash() << "-" << setw( 4 ) << pid_hash;

		collaborationId = new string( 13, 0 );
		collaborationId->assign( collaborationIdStr.str() );

		int setSpecificResult = pthread_setspecific( Collaboration::CollaborationKey, collaborationId );
		if ( 0 != setSpecificResult )
		{
			TRACE_LOG( "Set thread specific CollaborationKey failed [" << setSpecificResult << "]" );
		}
	}
	
	//int randValue = rand();
		
	// convert GUID to a string
	guidStream << setbase( 16 ) << setfill( '0' ) << setw( 8 ) << ltime << "-" 
	<< setw( 13 ) << *collaborationId << setw( 4 ) << ( ( ++m_Static ) & 0xFFFF );
	
	return guidStream.str();
}

string Collaboration::GenerateMsgID()
{
	stringstream guidStream;

	// rewind contor
	if ( m_Contor > 999999 )
	{
		m_Contor = 0;
	}
	
	guidStream << setbase( 10 ) << setfill( '0' ) << setw( 6 ) << TimeUtil::Get( "%H%M%S", 6 ) << setw( 6 ) << m_Contor;
		
	m_Contor++;
	return guidStream.str();
}
