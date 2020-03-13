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

#include "Trace.h"
#include "StringUtil.h"

//#include <ctime>

#include "RoutingCOT.h"
#include "RoutingDbOp.h"

//RoutingCOTMarker implementation
RoutingCOTMarker::RoutingCOTMarker() : 	m_MarkerId( -1 ), m_Name( "<unnamed>" ), m_Changed( false )
{
	struct tm ptr;
	time_t tim;
	
	tim = time( NULL );
	ptr = *localtime( &tim );
	
	m_ActTime = mktime( &ptr );
}

RoutingCOTMarker::RoutingCOTMarker( int markerId, const string& acttime, const string& name ) : 
	m_MarkerId( markerId ), m_Name( name ), m_Changed( false )
{
	struct tm ptr;
	time_t tim;
	
	tim = time( NULL );
	ptr = *localtime( &tim );
	ptr.tm_hour = StringUtil::ParseInt( acttime.substr( 0, 2 ) );
	ptr.tm_min = StringUtil::ParseInt( acttime.substr( 3, 2 ) );
	ptr.tm_sec = StringUtil::ParseInt( acttime.substr( 6, 2 ) );
	
	m_ActTime = mktime( &ptr );
	
	string cotTime = getStrTime();
	DEBUG( "COT Marker [" << m_Name << "] set at " << cotTime << "; database value : " << acttime );
}

string RoutingCOTMarker::getStrTime() const
{
	char retstr[ 7 ];
	
	struct tm ptr = *localtime( &m_ActTime );
	strftime( retstr, 7, "%H%M%S", &ptr );
	
	return retstr;
}

bool RoutingCOTMarker::operator != ( const RoutingCOTMarker& source )
{
	if( m_MarkerId != source.getId() )
		return true;
		
	bool changed = false;
	if( m_Name != source.getName() )
	{
		DEBUG( "Marker [" << m_Name << "] changed [NAME] to " << source.getName() );
		changed = true;
	}
		
	if( difftime( m_ActTime, source.getTime() ) != 0 )
	{
		DEBUG( "Marker [" << m_Name << "] changed [TIME] from " << getStrTime() << " to " << source.getStrTime() );
		DEBUG( "Time diff is " << difftime( m_ActTime, source.getTime() ) );
		changed = true;
	}
	
	return changed;
}

RoutingCOTMarker& RoutingCOTMarker::operator = ( const RoutingCOTMarker &source )
{
	if ( this == &source )
		return *this;

	m_MarkerId = source.getId();
	m_ActTime = source.getTime();
	m_Name = source.getName();
	m_Changed = source.m_Changed;
	
	return *this;
}

//RoutingCOT implementation
RoutingCOT::RoutingCOT() : m_Changed( false )
{
}

RoutingCOT::~RoutingCOT()
{
}

bool RoutingCOT::Update()
{
	DataSet* markers = NULL;
	m_Changed = false;
	try
	{
		markers = RoutingDbOp::GetCOTMarkers();
		
		if( ( markers == NULL ) || ( markers->size() == 0 ) )
			throw runtime_error( "No COT markers defined." );
			
		if( markers->size() != m_Markers.size() )
		{
			// Number of markers changed
			unsigned int initMarkersNo = m_Markers.size(), crtMarkersNo = markers->size();
			DEBUG( "Number of COT markers changed from " << initMarkersNo << " to " << crtMarkersNo );
			m_Changed = true;
			m_Markers.clear();
		}
		for( unsigned int i=0; i<markers->size(); i++ )
		{
			int markerId = markers->getCellValue( i, "ID" )->getLong();
			string markerName = StringUtil::Trim( markers->getCellValue( i, "NAME" )->getString() );
			string markerTime = StringUtil::Trim( markers->getCellValue( i, "LIMITTIME" )->getString() );

			RoutingCOTMarker marker( markerId, markerTime, markerName );
			map< int, RoutingCOTMarker >::iterator finder = m_Markers.find( markerId );
						
			// found
			if ( finder != m_Markers.end() )
			{
				if( finder->second != marker )
				{
					finder->second = marker;
					m_Changed = true;
				}
			}
			else
				m_Markers.insert( pair< int, RoutingCOTMarker >( markerId, marker ) );
		}
		if( markers != NULL )
		{
			delete markers;
			markers = NULL;
		}
	}
	catch( const std::exception& ex )
	{
		TRACE( "Unable to update COT markers [" << ex.what() << "]" );
		if( markers != NULL )
			delete markers;
			
		throw AppException( "Unable to update COT markers", ex );
	}
	catch( ... )
	{
		TRACE( "Unable to update COT markers [Unknown exception]" );
		if( markers != NULL )
			delete markers;
		throw;
	}
	
	return m_Changed;
}

RoutingCOTMarker RoutingCOT::ActiveMarker() const
{
	if( m_Markers.size() == 0 )
		throw runtime_error( "No COT markers defined." );
			
	time_t tim;
	tim = time( NULL );
	
	char strtime[ 7 ];
	
	struct tm ptr = *localtime( &tim );
	strftime( strtime, 7, "%H%M%S", &ptr );
	DEBUG( "Active marker requested at " << strtime );
	
	map< int, RoutingCOTMarker >::const_iterator finder = m_Markers.begin();
	
	double minTimeDiff = 100000;
	RoutingCOTMarker returnMarker;
	
	while( finder != m_Markers.end() )
	{
		double crtTimeDiff = difftime( tim, finder->second.getTime() );
		DEBUG( "Time diff for [" << finder->second.getName() << "] is " << crtTimeDiff );
		
		if ( ( crtTimeDiff > 0 ) && ( crtTimeDiff < minTimeDiff ) )
		{
			minTimeDiff = crtTimeDiff;
			returnMarker = finder->second;
		}
		finder++;
	}
	
	return returnMarker;
}
