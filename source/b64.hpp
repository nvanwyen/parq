//
// b64.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2004-2025 Metasystems Technologies Inc. (MTI)
// All rights reserved
//
// Distributed under the MTI Software License, Version 0.1.
//
// as defined by accompanying file MTI-LICENSE-0.1.info or
// at http://www.mtihq.com/license/MTI-LICENSE-0.1.info
//

#ifndef __MTI_CRYPTO_B64_HPP__
#define __MTI_CRYPTO_B64_HPP__

//
#include <memory>
#include <string>
#include <istream>

//
namespace mti { namespace crypto {

//
class b64
{
    public:
        //
        b64() {}
        virtual ~b64() {}

        // raw
        static size_t encode( const char* in, size_t len, char** out );
        static size_t decode( const char* in, size_t len, char** out );

        //
        static size_t encode( std::istream& si, std::ostream& so );
        static size_t decode( std::istream& si, std::ostream& so );

        //
        static size_t encode( std::streambuf& bi, std::streambuf& bo );
        static size_t decode( std::streambuf& bi, std::streambuf& bo );

        //
        static std::string encode( const std::string in );
        static std::string decode( const std::string in );

    protected:
    private:
        //
        static size_t calc_decode_len( const char* in );
};

//
using b64_ptr = std::shared_ptr<b64>;

}} // namespace mti::crypto

#endif // __MTI_CRYPTO_B64_HPP__
