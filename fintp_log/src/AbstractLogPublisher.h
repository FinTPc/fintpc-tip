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

#ifndef ABSTRACTLOGPUBLISHER_H
#define ABSTRACTLOGPUBLISHER_H

#include "AppExceptions.h"
#include <xercesc/dom/DOM.hpp>

using namespace std;
XERCES_CPP_NAMESPACE_USE

namespace FinTP
{
	/// base class for log publishers
	class ExportedLogObject AbstractLogPublisher
	{
		protected :
			// .ctor
			AbstractLogPublisher();

			bool m_Default;	
			int m_EventFilter;
			
		public:
			// destructor
			virtual ~AbstractLogPublisher();
			
			// methods
			virtual void Publish( const AppException& exception ) = 0;
			
			static string FormatException( const AppException& exception );

			// serializes the exception to an XMLDocument
			static string SerializeToXmlStr( const AppException& except );
			static xercesc::DOMDocument* SerializeToXml( const AppException& exception );

			static AppException DeserializeFromXml( const xercesc::DOMDocument *doc );
				
			void setDefault( const bool def ){ m_Default = def; }

			// used to filter what exceptions this publisher.. publishes
			void setEventFilter( const int eventFilter ) { m_EventFilter = eventFilter; }
			int eventFilter( void ){ return m_EventFilter; }
	};
}

#endif // ABSTRACTLOGPUBLISHER_H
