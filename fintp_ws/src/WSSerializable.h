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

#ifndef WSSERIALIZABLE_H
#define WSSERIALIZABLE_H

#include "DllMainWs.h"

#include "xercesc/dom/DOM.hpp"

#include <string>
#include <stdexcept>

#ifdef WIN32
	#define __MSXML_LIBRARY_DEFINED__
#endif

#include "XmlStringUtil.h"

XERCES_CPP_NAMESPACE_USE
using namespace std;

class ExportedWsObject WSSerializable
{
	public:
	
		WSSerializable( const string& elementName = "name", const string& namespaceId = "ns", const string& namespaceUri = "uri" );
		virtual ~WSSerializable();
		
		string Serialize() const;
		DOMNode* Serialize( DOMNode* root ) const;
		WSSerializable* Deserialize( const DOMNode* data ) const;

		string getElementName() const { return m_ElementName; }
		string getNamespaceId() const { return m_NamespaceId; }
		string getNamespaceUri() const { return m_NamespaceUri; }
		
	protected :
		
		virtual string internalSerialize() const = 0;
		virtual DOMNode* internalSerialize( DOMNode* root ) const { return root;}
		virtual WSSerializable* internalDeserialize( const DOMNode* data ) = 0;
		
		string m_ElementName;
		string m_NamespaceId;
		string m_NamespaceUri;
};

#endif // WSSERIALIZABLE_H
