//
// reader_avro.hpp
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

#ifndef __MTI_PARQ_READER_AVRO_HPP__
#define __MTI_PARQ_READER_AVRO_HPP__

//
#include "reader.hpp"

//
namespace mti { namespace parq {

//
// Reads Apache Avro object container files.
//
// The file is decoded into an arrow::Table on open, so every accessor and
// value rendering in the base class is shared with the parquet reader -- the
// output of the two formats differs only where the metadata genuinely does.
//
class avro_reader : public reader
{
    public:
        //
        avro_reader();
        avro_reader( const char* file );
        avro_reader( std::string file );

        //
        ~avro_reader() override;

        //
        void open( const char* file ) override;
        using reader::open;

        //
        void close() override;

        //
        std::string format() const override;

        // avro has no row groups; these are its blocks
        size_t num_row_groups() const override;

        //
        std::string created_by() const override;

        // avro compresses per block, not per column, so every column reports
        // the one file level codec
        std::string compression_type( Index col ) const override;

    protected:
    private:
        //
        std::string codec_;
        std::string created_;
        size_t blocks_;
};

}} // namespace mti::parq

#endif // __MTI_PARQ_READER_AVRO_HPP__
