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

#include "WorkItemPool.h"

using namespace FinTP;

//ManagedBuffer implementation
ManagedBuffer::ManagedBuffer( const ManagedBuffer& source )
{
	m_BufferAddr = new unsigned char*;
	*m_BufferAddr = NULL;

	m_BufferType = ManagedBuffer::Copy;
	m_BufferSize = source.size();
	m_MaxBufferSize = source.max_size();

	allocate( m_BufferSize );
	if ( ( m_BufferAddr != NULL ) && ( *m_BufferAddr != NULL ) )
		memcpy( *m_BufferAddr, source.buffer(), m_BufferSize );
	else
	{
		if ( m_BufferAddr != NULL )
			delete m_BufferAddr;
		throw runtime_error( "Managed buffer alloc failed." );
	}
}

ManagedBuffer::ManagedBuffer( unsigned char* buff, ManagedBuffer::BufferType bufferType, unsigned long allocsize, unsigned long maxBufferSize )
{
	m_BufferAddr = new unsigned char*;
	*m_BufferAddr = NULL;

	// adopt will take the buffer pointer and delete it upon destruction
	switch( bufferType )
	{
	case ManagedBuffer::Ref :
		*m_BufferAddr = buff;
		break;

	case ManagedBuffer::Adopt :
		*m_BufferAddr = buff;
		if ( buff == NULL )
		{
			if ( allocsize != 0 )
				allocate( allocsize );
		}
		break;

	case ManagedBuffer::Copy :
		// not adopting, if the buffer passed is valid, copy contents and destroy copy upon destructor call
		*m_BufferAddr = NULL;
		if ( allocsize != 0 )
		{
			allocate( allocsize );
			if ( ( m_BufferAddr != NULL ) && ( *m_BufferAddr != NULL ) )
			{ 
				if ( buff != NULL )
					memcpy( *m_BufferAddr, buff, allocsize );
			}
			else 
			{
				if ( m_BufferAddr != NULL )
					delete m_BufferAddr;
				throw runtime_error( "Managed buffer alloc failed." );
			}
		}
		break;
	default :
		throw runtime_error( "Invalid buffer type" );
	}

	m_BufferType = bufferType;
	m_BufferSize = allocsize;
	m_MaxBufferSize = maxBufferSize;
}

ManagedBuffer& ManagedBuffer::operator=( const ManagedBuffer& source )
{
	if ( this == &source )
		return *this;

	if ( m_BufferAddr != NULL )
	{
		if ( ( m_BufferType != ManagedBuffer::Ref ) && ( *m_BufferAddr != NULL ) )
			delete[] *m_BufferAddr;
	}
	else
		m_BufferAddr = new unsigned char*;

	*m_BufferAddr = NULL;

	m_BufferType = ManagedBuffer::Copy;
	m_BufferSize = source.size();
	m_MaxBufferSize = source.max_size();

	allocate( m_BufferSize );
	if ( ( m_BufferAddr != NULL ) && ( *m_BufferAddr != NULL ) )
		memcpy( *m_BufferAddr, source.buffer(), m_BufferSize );
	else
	{
		if ( m_BufferAddr != NULL )
			delete m_BufferAddr;
		throw runtime_error( "Managed buffer alloc failed." );
	}

	return *this;
}

ManagedBuffer ManagedBuffer::getRef()
{
	return ManagedBuffer( *m_BufferAddr, ManagedBuffer::Ref, m_BufferSize, m_MaxBufferSize );
}

ManagedBuffer::~ManagedBuffer()
{
	// nothing to do here
	if ( m_BufferAddr == NULL )
		return;

	if ( m_BufferType == ManagedBuffer::Ref )
	{
		// delete only the address, leave the buffer
		delete m_BufferAddr;
		m_BufferAddr = NULL;
		
		return;
	}

	if ( *m_BufferAddr != NULL )
	{
		delete[] *m_BufferAddr;
		*m_BufferAddr = NULL;
	}

	delete m_BufferAddr;
}

void ManagedBuffer::allocate( unsigned long allocsize )
{
	if ( allocsize > m_MaxBufferSize )
		throw logic_error( "Insufficient buffer size." );

	m_BufferSize = allocsize;
	if ( *m_BufferAddr != NULL )
	{
		delete[] *m_BufferAddr;
		*m_BufferAddr = NULL;
	}
	*m_BufferAddr = new unsigned char[ m_BufferSize ];
	if ( *m_BufferAddr == NULL )
		throw runtime_error( "Buffer allocation failed" );
}

string ManagedBuffer::str() const
{
	if ( *m_BufferAddr == NULL )
		return "";

	string result = string( ( char* )( *m_BufferAddr ), m_BufferSize );
	return result;
}

string ManagedBuffer::str( const string::size_type size ) const
{
	if ( *m_BufferAddr == NULL )
		return "";

	string result = ( m_BufferSize > size ) ? string( ( char* )( *m_BufferAddr ), size ) : string( ( char* )( *m_BufferAddr ), m_BufferSize );
	return result;
}

void ManagedBuffer::copyFrom( const string& source, const unsigned long maxBufferSize )
{
	unsigned long sourcesize = source.length();
	copyFrom( ( unsigned char* )source.c_str(), sourcesize, maxBufferSize );
}

void ManagedBuffer::copyFrom( const unsigned char* source, unsigned long sourceSize, unsigned long maxBufferSize )
{
	if ( sourceSize == 0 )
	{
		if ( *m_BufferAddr != NULL )
			delete[] *m_BufferAddr;
		*m_BufferAddr = NULL;
		m_MaxBufferSize = 0;
		m_BufferSize = 0;
	}
	else
	{
		m_BufferSize = sourceSize;
		m_MaxBufferSize = maxBufferSize;
		allocate( m_BufferSize );
		memcpy( *m_BufferAddr, source, m_BufferSize );
	}
}

void ManagedBuffer::copyFrom( const ManagedBuffer& source )
{
	m_MaxBufferSize = source.max_size();

	if ( ( source.buffer() == NULL ) || ( source.size() == 0 ) )
	{
		if ( *m_BufferAddr != NULL )
			delete[] *m_BufferAddr;
		*m_BufferAddr = NULL;
		m_BufferSize = 0;
	}
	else
	{
		m_BufferSize = source.size();

		// this will erase the buffer
		allocate( m_BufferSize );
		memcpy( *m_BufferAddr, source.buffer(), m_BufferSize );
	}
}

void ManagedBuffer::copyFrom( const ManagedBuffer* source )
{
	m_MaxBufferSize = source->max_size();

	if ( ( source->buffer() == NULL ) || ( source->size() == 0 ) )
	{
		if ( *m_BufferAddr != NULL )
			delete[] *m_BufferAddr;
		*m_BufferAddr = NULL;
		m_BufferSize = 0;
	}
	else
	{
		m_BufferSize = source->size();

		// this will erase the buffer
		allocate( m_BufferSize );
		memcpy( *m_BufferAddr, source->buffer(), m_BufferSize );
	}
}

ManagedBuffer ManagedBuffer::operator+( const unsigned long offset ) const 
{
	if ( offset > m_BufferSize )
		throw out_of_range( "buffer index" );
	return ManagedBuffer( *m_BufferAddr + offset, ManagedBuffer::Ref, m_BufferSize - offset, m_MaxBufferSize - offset );
}

ManagedBuffer& ManagedBuffer::operator+=( const unsigned long offset )
{
	if ( m_BufferType != ManagedBuffer::Ref )
		return *this;

	if ( offset > m_BufferSize )
		throw out_of_range( "buffer index" );
	*m_BufferAddr += offset;
	m_BufferSize -= offset;
	m_MaxBufferSize -= offset;

	return *this;
}

long ManagedBuffer::operator-( const ManagedBuffer& source ) const
{
	return *m_BufferAddr - source.buffer();
}
