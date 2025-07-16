//
// b64.cpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2004-2020 Metasystems Technologies Inc. (MTI)
// All rights reserved
//
// Distributed under the MTI Software License, Version 0.1.
//
// as defined by accompanying file MTI-LICENSE-0.1.info or
// at http://www.mtihq.com/license/MTI-LICENSE-0.1.info
//

//
#include <string.h>
#include <openssl/bio.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

//
#include <vector>
#include <sstream>

//
#include "b64.hpp"

#define BUFSIZE     8192

//
namespace mti { namespace crypto {

// static
size_t b64::encode( const char* in, size_t len, char** out )
{
    BIO* mem     = NULL;
    BIO* b64     = NULL;
    BUF_MEM* ptr = NULL;
    size_t   rsz = 0;

    //
    b64 = BIO_new ( BIO_f_base64() );
    BIO_set_flags( b64, BIO_FLAGS_BASE64_NO_NL );
    mem = BIO_new( BIO_s_mem() );

    //
    b64 = BIO_push( b64, mem );

    //
    if ( ( rsz = BIO_write( b64, in, len ) ) > 0 )
    {
        //
        BIO_flush( b64 );

        //
        BIO_get_mem_ptr( b64, &ptr );

        //
        *out = (char*)::malloc( ( rsz = ptr->length ) + 1 );
        ::memset( *out, 0, rsz + 1 );

        //
        ::memcpy( *out, ptr->data, ptr->length );

        (*out)[ ptr->length + 1 ] = 0;
    }

    //
    BIO_free_all( b64 );

    //
    return rsz;
}

// static
size_t b64::decode( const char* in, size_t len, char** out )
{
    BIO* b64   = NULL;
    BIO* mem   = NULL;
    size_t rsz = 0;

    //
    *out = (char*)::malloc( len + 1 );
    ::memset( *out, 0, len + 1 );

    //
    b64 = BIO_new( BIO_f_base64() );
    BIO_set_flags( b64, BIO_FLAGS_BASE64_NO_NL );
    mem = BIO_new_mem_buf( (char*)in, len );

    //
    mem = BIO_push( b64, mem );

    //
    rsz = BIO_read( mem, *out, len );

    //
    BIO_free_all( mem );

    //
    return rsz;
}

// static
size_t b64::encode( std::istream& si, std::ostream& so )
{
    size_t len = 0;
    std::vector<char> vec;
    char in[ BUFSIZE ];
    char* out = NULL;

    //
    while ( ! si.eof() )
    {
        int rsz = 0;

        //
        memset( in, 0, BUFSIZE );

        //
        si.read( (char*)in, BUFSIZE );

        //
        if ( ( rsz = (int)si.gcount() ) > 0 )
        {
            for ( int i = 0; i < rsz; ++i )
                vec.push_back( in[ i ] );
        }
    }

    //
    if ( vec.size() > 0 )
    {
        if ( ( len = encode( (const char*)&vec[ 0 ], vec.size(), &out ) ) > 0 )
        {
            for ( size_t i = 0; i < len; ++i )
            {
                char ch[ 2 ] = { 0 };

                ch[ 0 ] = out [ i ];
                so.write( ch, 1 );
            }

            free( out );
        }
    }

    return len;
}

// static
size_t b64::decode( std::istream& si, std::ostream& so )
{
    size_t len = 0;
    std::vector<char> vec;
    char in[ BUFSIZE ];
    char* out = NULL;

    //
    while ( ! si.eof() )
    {
        int rsz = 0;

        //
        memset( in, 0, BUFSIZE );

        //
        si.read( (char*)in, BUFSIZE );

        //
        if ( ( rsz = (int)si.gcount() ) > 0 )
        {
            for ( int i = 0; i < rsz; ++i )
                vec.push_back( in[ i ] );

            free( out );
        }
    }

    //
    if ( vec.size() > 0 )
    {
        if ( ( len = decode( (const char*)&vec[ 0 ], vec.size(), &out ) ) > 0 )
        {
            for ( size_t i = 0; i < len; ++i )
            {
                char ch[ 2 ] = { 0 };

                ch[ 0 ] = out [ i ];
                so.write( ch, 1 );
            }
        }
    }

    return len;
}

// static
size_t b64::encode( std::streambuf& bi, std::streambuf& bo )
{
    std::istream si( &bi );
    std::ostream so( &bo );

    return encode( si, so );
}

// static
size_t b64::decode( std::streambuf& bi, std::streambuf& bo )
{
    std::istream si( &bi );
    std::ostream so( &bo );

    return decode( si, so );
}

// static
std::string b64::encode( const std::string in )
{
    std::stringstream si( in );
    std::stringstream so;

    encode( si, so );

    return so.str();
}

// static
std::string b64::decode( const std::string in )
{
    std::stringstream si( in );
    std::stringstream so;

    decode( si, so );

    return so.str();
}

// static
size_t b64::calc_decode_len( const char* in )
{
    size_t len = strlen( in );
    size_t pad = 0;

    if ( in[ len - 1 ] == '=' && in[ len - 2 ] == '=' ) // last two chars are =
        pad = 2;
    else if ( in[ len - 1 ] == '=' )                    // last char is =
        pad = 1;

    return ( len * 3 ) / 4 - pad;
}

}} // namespace mti::crypto
