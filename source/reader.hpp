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
// #include <arrow/csv/writer.h>
#include <parquet/exception.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
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
#define INVAILD_POINTER    10

//
namespace mti { namespace parq {

//
using arrow::internal::StringFormatter;

//
class reader
{
    public:
        //
        using FileReader = std::unique_ptr<parquet::arrow::FileReader>;
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
            exception( parquet::ParquetStatusException& ex )
                : code_( 0 ),
                  what_( "" ) { code_ = CORRUPTED_FILE;
                                what_ = ex.what(); }

            //
            exception( parquet::ParquetException& ex )
                : code_( 0 ),
                  what_( "" ) { code_ = CRITICAL_ERROR;
                                what_ = ex.what(); }

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
        reader( const char* file );
        reader( std::string file );

        //
        void open( const char* file );
        void open( std::string file );

        //
        void close();

        //
        options_ptr properties();
        std::string property( std::string name );
        void property( std::string name, std::string value );

        void set_case( std::string val );
        void set_scale( unsigned int val );
        void set_precision( unsigned int val );

        //
        Table table();
        Column column( Index i );
        Columns columns();

        //
        Field field( Index i );
        Fields fields();

        //
        size_t num_rows();
        size_t num_cols();

        //
        bool is_open();

        //
        std::string name( Index col );
        std::string value( Index col, Index row ); 

        //
        std::string key( std::string id, Index row );

        //
        std::string json( Index row ); 

        //
        static std::string to_type( Column col );
        static std::string to_type( Type type );
        static std::string to_type( Kind type );

    protected:
    private:
        FileReader read_;
        Table table_;

        options_ptr option_;

        //
        void init();

        //
        std::string use_case( std::string s );
        std::string to_lower( std::string s );
        std::string to_upper( std::string s );

        //
        std::string trim( std::string s );
        std::string ltrim( std::string s );
        std::string rtrim( std::string s );

        //
        std::string uuid( int sz = 0 );
};

//
using reader_ptr = std::shared_ptr<reader>;

}} // namespace mti::parq

#endif // __MTI_PARQ_READER_HPP__
