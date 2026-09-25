//
// format.hpp
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

#ifndef __MTI_PARQ_FORMAT_HPP__
#define __MTI_PARQ_FORMAT_HPP__

//
#include <string>
//
#include "reader.hpp"

//
namespace mti { namespace parq {

//
// The input file format, selected by -p / -a
//
enum class input_format
{
    PARQUET,
    AVRO,
    ORC
};

//
std::string to_string( input_format fmt );

// true when this build can read the format ( see the WITH_AVRO and WITH_ORC
// cmake options )
bool supported( input_format fmt );

// Build the reader for a format. Throws reader::exception with NOT_SUPPORTED
// when the format was not compiled in.
reader_ptr make_reader( input_format fmt );

}} // namespace mti::parq

#endif // __MTI_PARQ_FORMAT_HPP__
