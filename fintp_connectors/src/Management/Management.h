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

#ifndef MANAGEMENT_H
#define MANAGEMENT_H

#include <pthread.h>
#include <string>

#include "../ConnectorMain.h"

using namespace std;

///<summary>
/// Contains an enumeration of possible transaction outcome/status
///</summary>
class ExportedTestObject TransactionStatus
{
	public :
		enum TransactionStatusEnum
		{
			// the transaction has begun
			Begin,	
			// the transaction was commited
			Commit,
			// the transaction was aborted
			Abort
		};
		
		// converts the type specified to a string
		static string ToString( TransactionStatusEnum ttype )
		{
			switch( ttype )
			{
				case TransactionStatus::Begin :
					return "Begin";
				case TransactionStatus::Commit :
					return "Commit";
				case TransactionStatus::Abort : 
					return "Abort";
				default:
					break;
			}
			throw invalid_argument( "type" );
		}
};

class ManagementHelper
{
	private :

		//constructor
		ManagementHelper(){};
		
	public:
			
		//destructor
		~ManagementHelper(){};
		
		static void* m_HeartbeatProvider_Tick( const void* data );
};

#endif
