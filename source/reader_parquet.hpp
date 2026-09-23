//
// reader_parquet.hpp
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

#ifndef __MTI_PARQ_READER_PARQUET_HPP__
#define __MTI_PARQ_READER_PARQUET_HPP__

//
#include <parquet/exception.h>
#include <parquet/arrow/reader.h>
#include <parquet/file_reader.h>
#include <parquet/metadata.h>
//
#include "reader.hpp"

//
namespace mti { namespace parq {

//
// Reads Apache Parquet files
//
class parquet_reader : public reader
{
    public:
        //
        using FileReader = std::unique_ptr<parquet::arrow::FileReader>;

        //
        parquet_reader();
        parquet_reader( const char* file );
        parquet_reader( std::string file );

        //
        ~parquet_reader() override;

        //
        void open( const char* file ) override;
        using reader::open;

        //
        void close() override;

        //
        std::string format() const override;
        size_t num_row_groups() const override;
        std::string created_by() const override;
        std::string compression_type( Index col ) const override;

    protected:
    private:
        //
        FileReader read_;
};

}} // namespace mti::parq

#endif // __MTI_PARQ_READER_PARQUET_HPP__
