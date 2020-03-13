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

#include "DumpContext.h"
#include "Trace.h"

using namespace FinTP;

DumpContext* DumpContext::m_Instance = NULL;

DumpContext::DumpContext()
{
}

DumpContext::~DumpContext()
{
}

DumpContext* DumpContext::getInstance()
{
	if ( m_Instance == NULL )
		m_Instance = new DumpContext();
	return m_Instance;
}

void DumpContext::Clear()
{
	getInstance()->m_Context.clear();
}

void DumpContext::Dump()
{
	TRACE_GLOBAL( "----------- Dump of context info begin ---------- " );
	map< string, string >::const_iterator iter = getInstance()->m_Context.begin();
	while( iter != getInstance()->m_Context.end() )
	{
		TRACE_GLOBAL( "\t[" << iter->first << "] = [" << iter->second << "]" );
	}
	TRACE_GLOBAL( "------------ Dump of context info end ---------- " );
}

void DumpContext::Add( const string& name, const string& value )
{
	( void )getInstance()->m_Context.insert( pair<string, string>( name, value ) );
}
