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

#include "FileFilter.h"
#include "Trace.h"
#include "StringUtil.h"
#include "PlatformDeps.h"

using namespace FinTP;

//Constructor
FileFilter::FileFilter() : AbstractFilter( FilterType::FILE )
{
}

//Destructor 
FileFilter::~FileFilter()
{
}

AbstractFilter::FilterResult FileFilter::ProcessMessage( AbstractFilter::buffer_type inputData, AbstractFilter::buffer_type outputData, NameValueCollection& transportHeaders, bool asClient )
{
	ValidateProperties( transportHeaders );

	if ( inputData.get() == NULL )
		throw runtime_error( "Empty data" );

	const string destName = Path::Combine( m_DestinationPath, Path::GetFilename( m_FileName ) );
	StringUtil::SerializeToFile( destName, inputData.get()->str() );

	return AbstractFilter::Completed;
}

bool FileFilter::canLogPayload()
{
	return true;
//	throw logic_error( "You must override this function to let callers know if this filter can log the payload without disturbing operations" );
}


bool FileFilter::isMethodSupported( AbstractFilter::FilterMethod method, bool asClient )
{
	switch( method )
	{
		case AbstractFilter::BufferToBuffer :
				return true;

		default:
			return false;
	}
}

/// private methods implementation
void FileFilter::ValidateProperties( NameValueCollection& transportHeaders )
{
	if ( m_Properties.ContainsKey( "FILEPATH" ) )
		m_DestinationPath = m_Properties[ "FILEPATH"  ];
	else
		throw runtime_error( "FILEPATH not specified for File filter" );

	if ( transportHeaders.ContainsKey( "FILENAME" ) )
		m_FileName = transportHeaders[ "FILENAME" ];
	else
		throw runtime_error( "FILENAME not specified for File filter" );
}

