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

#ifndef ROUTINGCORRELATIONMANAGER_H
#define ROUTINGCORRELATIONMANAGER_H

#include <map>
#include <vector>
#include <string>
using namespace std;

#include "RoutingAggregationCode.h"

typedef std::map<std::string, std::string> RoutingAggregationFieldArray;
typedef std::map<std::string, std::string> RoutingAggregationConditionArray;

#define DEFAULT_AGGTABLE "FEEDBACKAGG"

class RoutingAggregationManager
{
	public:
	
		typedef enum
		{
			/*
			Tries to update ( based on trim match ), if that fails, it inserts
			*/
			OptimisticUpdateTrimmed,
			/*
			Tries to update, if that fails, it inserts
			*/
			OptimisticUpdate,
			/*
			Tries to insert, if that fails, it updates
			*/
			OptimisticInsert,
			/*
			Finds if the information needs updated/inserted
			*/
			InsertOrUpdate,
			/* 
			Fails if no records to update
			*/
			UpdateOrFail
		} REQUEST_MODE;

	public:

		~RoutingAggregationManager();
		
		//Group: Static methods
		/*
		 Function: Request
		 Stores a request that will eventually receive one/more reply(ies).
		 Matching is done against the aggregation token.
		 
		 Parameters:
		 correlationToken - Key for the request. Any reply having the same correlationToken will match this request.
		 request - A collection of name-value pairs relevant to a certain request.
		 
		 */
		// atentie : nu adauga parametrii cu valori implicite inaintea altor param. cu valori implicite 
		//( altfel, trebuie verificat peste tot unde se folosesc valorile implicite )
		static void AddRequest( const RoutingAggregationCode& request, 
			const REQUEST_MODE requestMode = RoutingAggregationManager::InsertOrUpdate, string aggregationTable = DEFAULT_AGGTABLE );

		static string GetAggregationField( RoutingAggregationCode& request, string requestedToken,
			string aggregationTable = DEFAULT_AGGTABLE );

		static bool GetAggregationFields( RoutingAggregationCode& request, string aggregationTable = DEFAULT_AGGTABLE );
		//static vector< RoutingAggregationFieldArray > GetAggregationRecords( RoutingAggregationCode& request, string aggregationTable = DEFAULT_AGGTABLE );
		
		//static void UpdateAggregationField( const RoutingAggregationCode& request, string aggregationTable = DEFAULT_AGGTABLE );
		
	private:
	
		RoutingAggregationManager();
};

#endif // ROUTINGCORRELATIONMANAGER_H
