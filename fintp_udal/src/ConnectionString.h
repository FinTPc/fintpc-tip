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

#ifndef CONNECTIONSTRING_H
#define CONNECTIONSTRING_H

#include <string>
#include "Collections.h"
#include "DllMainUdal.h"

using namespace std;

namespace FinTP
{
	/**
	 * Store informations used by the Database instances to connect.
	**/
	class ExportedUdalObject ConnectionString
	{
		public :

			/**
			 * Constructor.
			 * \param dbName	type string    name of the database.
			 * \param userName	type string    name of the user.
			 * \param userPassword type string	the user password.
			**/
			explicit ConnectionString( const string& dbName = "", const string& userName = "", const string& userPassword = ""  );

			/**
			 * Copy .ctor. Copy connection information from source parameter
			 * \param source type ConnectionString. Source instance containing new connection information
			 * \return A copy of this object.
			**/
			ConnectionString& operator=( const ConnectionString &source );

			void setDatabaseName( const string& databaseName ) {
				m_DatabaseName = databaseName;
			}
			void setUserName( const string& userName ) {
				m_UserName = userName;
			}
			void setUserPassword( const string& userPassword ) {
				m_UserPassword = userPassword;
			}
			string getDatabaseName() const {
				return m_DatabaseName;
			}
			string getUserName() const {
				return m_UserName;
			}
			string getUserPassword() const {
				return m_UserPassword;
			}

			/**
			 * Query if this object is valid.
			 * \return true if every storage member had been set
			**/
			bool isValid() const
			{
				return ( ( m_DatabaseName.length() > 0 ) && ( m_UserName.length() > 0 ) && ( m_UserPassword.length() > 0 ) );
			}

		private :
			/** Database name/id to connect to, as defined by DBMS **/
			string m_DatabaseName;
			/** DBMS defined user, whose privileges will grant DBMS resources access **/
			string m_UserName;
			/** user password defined in DBMS **/
			string m_UserPassword;
	};
}

#endif //CONNECTIONSTRING_H
