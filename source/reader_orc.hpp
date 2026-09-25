//
// reader_orc.hpp
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

#ifndef __MTI_PARQ_READER_ORC_HPP__
#define __MTI_PARQ_READER_ORC_HPP__

//
#include "reader.hpp"

//
namespace mti { namespace parq {

//
// Reads Apache ORC files.
//
// Arrow's ORC adapter hands back an arrow::Table directly, the same as the
// parquet reader does, so every accessor and value rendering in the base class
// is shared and only the loading differs.
//
// NOTE: unlike the parquet reader this does not keep its file reader alive.
// The adapter's metadata accessors are read once at open and cached below,
// and the table it returns owns its own buffers -- so there is nothing left
// to keep the file open for.
//
class orc_reader : public reader
{
    public:
        //
        orc_reader();
        orc_reader( const char* file );
        orc_reader( std::string file );

        //
        ~orc_reader() override;

        //
        void open( const char* file ) override;
        using reader::open;

        //
        void close() override;

        //
        std::string format() const override;

        // orc has no row groups; these are its stripes
        size_t num_row_groups() const override;

        //
        std::string created_by() const override;

        // orc compresses per stream with one file level codec, so every column
        // reports the same value -- the same situation as avro blocks
        std::string compression_type( Index col ) const override;

    protected:
    private:
        //
        std::string codec_;
        std::string created_;
        size_t stripes_;
};

}} // namespace mti::parq

#endif // __MTI_PARQ_READER_ORC_HPP__
