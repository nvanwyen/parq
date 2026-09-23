//
// format.cpp
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

//
#include "format.hpp"
#include "reader_parquet.hpp"

#ifdef WITH_AVRO
#include "reader_avro.hpp"
#endif

//
namespace mti { namespace parq {

//
std::string to_string( input_format fmt )
{
    switch ( fmt )
    {
        case input_format::AVRO:    return "avro";
        case input_format::PARQUET: return "parquet";
    }

    return "parquet";
}

//
bool supported( input_format fmt )
{
    switch ( fmt )
    {
        case input_format::AVRO:
#ifdef WITH_AVRO
            return true;
#else
            return false;
#endif

        case input_format::PARQUET:
            return true;
    }

    return false;
}

//
reader_ptr make_reader( input_format fmt )
{
    switch ( fmt )
    {
        case input_format::AVRO:
#ifdef WITH_AVRO
            return reader_ptr( new avro_reader() );
#else
            throw reader::exception( NOT_SUPPORTED,
                "This build has no avro support. Rebuild with -DWITH_AVRO=ON "
                "( requires the avro-cpp library )." );
#endif

        case input_format::PARQUET:
            return reader_ptr( new parquet_reader() );
    }

    return reader_ptr( new parquet_reader() );
}

}} // namespace mti::parq
