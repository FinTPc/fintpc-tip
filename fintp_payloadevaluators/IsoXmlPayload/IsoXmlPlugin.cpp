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

#include "IsoXmlPayload.h"

class IsoXmlPlugin: public Plugin
{
public:
	IsoXmlPlugin( const std::string& name, const std::string& version ): Plugin( name, version ) {}
	bool hasNamespace( const string& aNamespace )
	{
		return ( ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:pacs.008.001.01" )
				|| ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:pacs.008.001.02" )
				|| ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:pacs.004.001.01" )
				|| ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:pacs.004.001.02" )
				|| ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:pacs.003.001.01" )
				|| ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:pacs.003.001.02" )
				|| ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:camt.029.001.03" )
				|| ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:camt.056.001.01" )
				|| ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:pacs.007.001.01" )
				|| ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:camt.007.002.03" )
				|| ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:pacs.007.001.02" )
				|| ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:pacs.002.001.02" )
				|| ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:pain.001.001.08Alv" )
				|| ( aNamespace == "urn:iso:std:iso:20022:tech:xsd:camt.054.001.06Alv" ));
		
	}
	RoutingMessageEvaluator* newInstance( const XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* document )
	{
		return new IsoXmlPayload( document );
	}
};

extern "C" {
#ifdef WIN32
__declspec( dllexport )  Plugin* getPluginInfo( void )
#else
Plugin* getPluginInfo( void )
#endif
{
	return new IsoXmlPlugin( "IsoXml", "0.1" );
}
}
