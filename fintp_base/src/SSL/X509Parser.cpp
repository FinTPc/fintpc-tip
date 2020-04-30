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

#include "X509Parser.h"

using namespace std;
using namespace FinTP;

X509Parser::X509Parser( const string& certificate ): m_X509C( NULL ), m_SerialDEC( "" ), m_Issuer( "" ), m_Subject( "" ), m_PublicKeyType( "" )
{
	if( certificate.empty() )
	{
		TRACE( "Empty certificate provided" );
		return;
	}
	
	BIO *bio = BIO_new( BIO_s_mem() );
	BIO_puts( bio, certificate.c_str() );
	m_X509C = PEM_read_bio_X509( bio, NULL, NULL, NULL );
	if( m_X509C == NULL )
	{
		BIO_free( bio );
		throw runtime_error( "An error has occured while trying to parse X509 certificate" );
	}

	if ( m_X509C != NULL )
	{
		ReadSerialNumber( X509_get_serialNumber( m_X509C ) );
		m_Issuer = ReadName( X509_get_issuer_name( m_X509C ) );
		m_Subject = ReadName( X509_get_subject_name( m_X509C ) );
		m_PublicKeyType = ReadKeyType();
		/*
		m_PEM = pem( m_X509C );
		m_Version = X509_get_version( m_X509C ) + 1;
		ReadThumbPrint( m_X509C );
		m_SignatureAlgorithm = string( OBJ_nid2ln( OBJ_obj2nid( ( m_X509C )->sig_alg->algorithm ) ) );
		ReadKeyType( m_X509C );
		ReadKeySize( m_X509C );
		m_BeforeASN1ISODateTime = ReadASN1ISODateTime( X509_get_notAfter( m_X509C ) );
		m_AfterASN1ISODateTime = ReadASN1ISODateTime( X509_get_notBefore( m_X509C ) );
		*/
	}

	BIO_free( bio );
}

void X509Parser::ReadSerialNumber( ASN1_INTEGER *bs )
{
	BIGNUM* bn = ASN1_INTEGER_to_BN( bs, NULL );
	char* result = BN_bn2dec( bn );
	if( result != NULL )
	{
		m_SerialDEC = string( result );
		OPENSSL_free( result );
	}

	BN_free( bn );
}

string X509Parser::ReadName( X509_NAME *subj_or_issuer )
{
	BIO *bio_out = BIO_new( BIO_s_mem( ) );

	X509_NAME_print( bio_out, subj_or_issuer, 0 );
	BUF_MEM *memBuff;
	BIO_get_mem_ptr( bio_out, &memBuff );
	
	string issuer = string( memBuff->data, memBuff->length );
	BIO_free( bio_out );

	return issuer;
}

string X509Parser::ReadKeyType()
{
	string publicKeyType = "";
	EVP_PKEY *pKey = X509_get_pubkey( m_X509C );
	int key_type = EVP_PKEY_type( pKey->type );
	EVP_PKEY_free( pKey );

	switch ( key_type )
	{
		case EVP_PKEY_RSA :
			publicKeyType = "RSA";
			break;
		case EVP_PKEY_DSA:
			publicKeyType = "DSA";
			break;
		case EVP_PKEY_DH:
			publicKeyType = "DH";
			break;
		case EVP_PKEY_EC:
			publicKeyType = "ECC";
			break;
		default:
			publicKeyType = "";
			break;
	}

	return publicKeyType;
}

/*
void X509Parser::UploadCertificate( const string& certificateFileName )
{
	BIO *bio_mem = BIO_new_file( certificateFileName.c_str(), "r" );
	string errorMessage;

	if ( bio_mem != NULL )
	{
		m_X509C = PEM_read_bio_X509( bio_mem, NULL, NULL, NULL );
		if( m_X509C == NULL )
			errorMessage = "An error has occured while trying to parse certificate: ";
	}
	else
		errorMessage = "An exception has occured while trying to read certificate from file: "; 
	
	BIO_free( bio_mem );
	if( !errorMessage.empty() )
	{
		stringstream error;
		error << errorMessage << "[" << certificateFileName << "]";
		TRACE( error.str() );
		throw runtime_error( error.str() );
	}
}

string X509Parser::pem( X509* x509 )
{
	BIO * bio_out = BIO_new( BIO_s_mem( ) );
	PEM_write_bio_X509( bio_out, x509 );
	BUF_MEM *bio_buf;
	BIO_get_mem_ptr( bio_out, &bio_buf );
	string pem = string( bio_buf->data, bio_buf->length );
	BIO_free( bio_out );
	return pem;
}

void X509Parser::ReadThumbPrint( X509* x509 )
{
	static const char hexbytes[] = "0123456789ABCDEF";
	unsigned int md_size;
	unsigned char md[EVP_MAX_MD_SIZE];
	const EVP_MD * digest = EVP_get_digestbyname( "sha1" );
	X509_digest( x509, digest, md, &md_size );
	stringstream ashex;
	for ( int pos = 0; pos < md_size; pos++ )
	{
		ashex << hexbytes[( md[pos] & 0xf0 ) >> 4];
		ashex << hexbytes[( md[pos] & 0x0f ) >> 0];
	}
	X509Parser::m_ThumbPrint = ashex.str( );
}
void X509Parser::ASN1DateTimeParse( const ASN1_TIME *time, int& year, int& month, int& day, int& hour, int& minute, int& second )
{
	const char* str = ( const char* )time->data;
	size_t i = 0;
	if ( time->type == V_ASN1_UTCTIME ) {
		year = ( str[i++] - '0' ) * 10;
		year += ( str[i++] - '0' );
		year += ( year < 70 ? 2000 : 1900 );
	}
	else if ( time->type == V_ASN1_GENERALIZEDTIME ) {
		year = ( str[i++] - '0' ) * 1000;
		year += ( str[i++] - '0' ) * 100;
		year += ( str[i++] - '0' ) * 10;
		year += ( str[i++] - '0' );
	}
	month = ( str[i++] - '0' ) * 10;
	month += ( str[i++] - '0' ) - 1; 
	day = ( str[i++] - '0' ) * 10;
	day += ( str[i++] - '0' );
	hour = ( str[i++] - '0' ) * 10;
	hour += ( str[i++] - '0' );
	minute = ( str[i++] - '0' ) * 10;
	minute += ( str[i++] - '0' );
	second = ( str[i++] - '0' ) * 10;
	second += ( str[i++] - '0' );
}

void X509Parser::ReadKeySize( X509* x509 )
{
	EVP_PKEY *pkey = X509_get_pubkey( x509 );
	int key_type = EVP_PKEY_type( pkey->type );
	int keysize = -1; //or in bytes, RSA_size( ) DSA_size( ), DH_size( ), ECDSA_size( );
	keysize = key_type == EVP_PKEY_RSA && pkey->pkey.rsa->n ? BN_num_bits( pkey->pkey.rsa->n ) : keysize;
	keysize = key_type == EVP_PKEY_DSA && pkey->pkey.dsa->p ? BN_num_bits( pkey->pkey.dsa->p ) : keysize;
	keysize = key_type == EVP_PKEY_DH  && pkey->pkey.dh->p ? BN_num_bits( pkey->pkey.dh->p ) : keysize;
	keysize = key_type == EVP_PKEY_EC ? EC_GROUP_get_degree( EC_KEY_get0_group( pkey->pkey.ec ) ) : keysize;
	EVP_PKEY_free( pkey );
	m_PublicKeySize = ( ( "%s" ), keysize );
}
string X509Parser::ReadASN1ISODateTime( const ASN1_TIME *tm )
{
	int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
	ASN1DateTimeParse( tm, year, month, day, hour, min, sec );

	char buf[25] = "";
	sprintf( buf, "%04d-%02d-%02d %02d:%02d:%02d GMT", year, month, day, hour, min, sec );
	return string( buf );
}

*/
