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

#include "WSSerializable.h"

#include <sstream>

WSSerializable::WSSerializable( const string& elementName, const string& namespaceId, const string& namespaceUri ) :
	m_ElementName( elementName ), m_NamespaceId( namespaceId ), m_NamespaceUri( namespaceUri )
{
}

WSSerializable::~WSSerializable()
{
}

string WSSerializable::Serialize() const 
{
	return "";
}

DOMNode* WSSerializable::Serialize( DOMNode* root ) const
{
	if ( root == NULL )
		throw invalid_argument( "Root node is NULL" );

	DOMDocument* doc = root->getOwnerDocument();
	if ( doc == NULL )
		throw invalid_argument( "Root node does not belong to a DOMDocument" );

	DOMElement* element = NULL;
	DOMNode* elemRef = NULL;
	try
	{
		stringstream qElemName;
		qElemName << m_NamespaceId << ":" << m_ElementName;
		element = doc->createElementNS( unicodeFormWs( m_NamespaceUri ), unicodeFormWs( qElemName.str() ) );
		
		// this serializes the inner node values
		internalSerialize( element );
		elemRef = root->appendChild( element );
	}
	catch( ... )
	{
		if ( element != NULL )
		{
			element->release();
		}
		throw;
	}
	return elemRef;
}

WSSerializable* WSSerializable::Deserialize( const DOMNode* data ) const
{
	return NULL;
}
