//
// reader.hpp
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

#ifndef __MTI_PARQ_READER_HPP__
#define __MTI_PARQ_READER_HPP__

//
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
//
#include <arrow/api.h>
#include <arrow/io/api.h>
//
#include <arrow/util/formatting.h>

//
#define PROP_CASE           "case"
#define PROP_SCALE          "scale"
#define PROP_PRECISION      "precision"

#define NOT_OPEN            1
#define ALREADY_OPEN        2
#define MISSING_FILE        3
#define OUT_OF_RANGE        4
#define INVALID_TYPE        5
#define UNKNOWN_ERROR       6
#define CORRUPTED_FILE      7
#define INVALID_COLUMN      8
#define CRITICAL_ERROR      9
#define INVALID_POINTER    10
#define NOT_SUPPORTED      11

//
namespace mti { namespace parq {

//
using arrow::internal::StringFormatter;

//
// Format agnostic reader.
//
// Everything below the load step works on an arrow::Table, so a derived reader
// only has to turn its own file format into that table -- every accessor, type
// mapping and value rendering is then shared. See reader_parquet.hpp and
// reader_avro.hpp for the two implementations.
//
class reader
{
    public:
        //
        using Table = std::shared_ptr<arrow::Table>;
        using Column = std::shared_ptr<arrow::ChunkedArray>;
        using Columns = std::vector<Column>;
        using Field = std::shared_ptr<arrow::Field>;
        using Fields = std::vector<Field>;
        using Index = unsigned int;
        using Type = std::shared_ptr<arrow::DataType>;
        using Kind = arrow::Type::type;

        //
        using options = std::map<std::string, std::string>;
        using options_ptr = std::shared_ptr<options>;

        //
        class exception : public std::exception
        {
          public:
            //
            exception( std::string w )
                : code_( 0 ),
                  what_( w ) {}

            //
            exception( int c, std::string w )
                : code_( c ),
                  what_( w ) {}

            //
            const char* what() const noexcept override
            {
                return what_.c_str();
            }

            //
            int code() const
            {
                return code_;
            }

          protected:
          private:
            //
            int code_;
            std::string what_;
        };

        //
        reader();
        virtual ~reader();

        //
        // format specific -- implemented by the derived readers
        //
        virtual void open( const char* file ) = 0;
        void open( std::string file );

        // the name of the format this reader handles ( "parquet", "avro" )
        virtual std::string format() const = 0;

        // parquet row groups / avro blocks
        virtual size_t num_row_groups() const = 0;

        // the writer that produced the file, where the format records it
        virtual std::string created_by() const = 0;

        // per column for parquet, file level for avro
        virtual std::string compression_type( Index col ) const = 0;

        // The schema declared default for a column, as it is written in the
        // schema ( so a declared null default reads "null", which is not the
        // same as having no default at all -- that is an empty string ).
        //
        // Only avro has the concept: parquet and orc record no per column
        // default anywhere in their metadata, so they never return one and
        // the base implementation below is what they use.
        virtual std::string default_value( Index col ) const;

        // Whether the column's schema permits null. The base implementation
        // reads arrow's own field flag, which parquet ( REQUIRED vs OPTIONAL )
        // and orc both populate correctly. Avro overrides it because avro
        // encodes nullability as a ["null", T] union rather than a flag.
        virtual bool is_nullable( Index col ) const;

        //
        virtual void close();

        //
        options_ptr properties();
        std::string property( std::string name ) const;
        void property( std::string name, std::string value );

        void set_case( std::string val );
        void set_scale( unsigned int val );
        void set_precision( unsigned int val );

        //
        Table table() const;
        Column column( Index i ) const;
        Columns columns() const;

        //
        Field field( Index i ) const;
        Fields fields() const;

        //
        size_t num_rows() const;
        size_t num_cols() const;

        //
        bool is_open() const;

        //
        std::string name( Index col ) const;
        std::string value( Index col, Index row ) const;

        //
        int64_t file_size() const;
        std::string file_checksum() const;

        //
        std::string key( std::string id, Index row );

        //
        std::string json( Index row );

        //
        static std::string to_type( Column col );
        static std::string to_type( Type type );
        static std::string to_type( Kind type );

    protected:
        //
        Table table_;
        std::string filename_;

        mutable options_ptr option_;

        //
        void init() const;

    private:
        //
        std::string use_case( std::string s ) const;
        std::string to_lower( std::string s ) const;
        std::string to_upper( std::string s ) const;

        //
        std::string trim( std::string s ) const;
        std::string ltrim( std::string s ) const;
        std::string rtrim( std::string s ) const;

        //
        std::string uuid( int sz = 0 );
};

//
using reader_ptr = std::shared_ptr<reader>;

}} // namespace mti::parq

#endif // __MTI_PARQ_READER_HPP__
