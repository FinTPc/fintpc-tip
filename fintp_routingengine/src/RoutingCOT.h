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

#ifndef ROUTINGCOT_H
#define ROUTINGCOT_H

#include <map>
#include "RoutingEngineMain.h"

class ExportedTestObject RoutingCOTMarker
{
	public :
	
		RoutingCOTMarker();
		RoutingCOTMarker( int markerId, const string& acttime, const string& name );
		
		string getName() const { return m_Name; }
		int getId() const { return m_MarkerId; }
		time_t getTime() const { return m_ActTime; }
		string getStrTime() const;
		
		bool operator != ( const RoutingCOTMarker& source );
		RoutingCOTMarker& operator = ( const RoutingCOTMarker &source );
		
	private :
	
		int m_MarkerId;
		time_t m_ActTime;
		string m_Name;
		bool m_Changed;
};

class RoutingCOT
{
	public:
	
		RoutingCOT();
		~RoutingCOT();
		
		bool Update();
		RoutingCOTMarker ActiveMarker() const;
		
	private :
	
		std::map< int, RoutingCOTMarker > m_Markers;
		bool m_Changed;
};

#endif // ROUTINGCOT_H
