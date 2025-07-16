//
#include <cctype>
#include <locale>
#include <random>
#include <algorithm>
#include <stdexcept>
//
#include <arrow/array/array_decimal.h>
//
#include "b64.hpp"
#include "json.hpp"
#include "reader.hpp"

#define DEFAULT_CASE            "lower"
#define DEFAULT_SCALE                9
#define DEFAULT_PRECISION            4

//
namespace mti { namespace parq {

//
reader::reader() : table_( nullptr )
{
    init();
}

//
reader::reader( const char* file )
{
    init();
    open( file );
}

//
reader::reader( std::string file )
{
    init();
    open( file );
}

//
void reader::open( const char* file )
{
    //
    if ( ! is_open() )
    {
        //
        if ( file != nullptr )
        {
            std::shared_ptr<arrow::io::ReadableFile> in;

            try
            {
                PARQUET_ASSIGN_OR_THROW( in,
                    arrow::io::ReadableFile::Open( file,
                                                   arrow::default_memory_pool() ) );

                //
                PARQUET_ASSIGN_OR_THROW( read_, parquet::arrow::OpenFile( in, 
                                      arrow::default_memory_pool() ) );

                //
                PARQUET_THROW_NOT_OK( read_->ReadTable( &table_ ) );
            }
            catch ( parquet::ParquetException& ex )
            {
                throw reader::exception( CORRUPTED_FILE, ex.what() );
            }
            catch ( ... )
            {
                // ...
                throw reader::exception( UNKNOWN_ERROR, "Unknown exception!" );
            }

        }
        else
            throw reader::exception( MISSING_FILE, "Invalid file name!" );
    }
    else
        throw reader::exception( ALREADY_OPEN, "Already open" );
}

//
void reader::open( std::string file )
{
    open( file.c_str() );
}

//
void reader::close()
{
    //
    table_ = nullptr;
}

//
reader::options_ptr reader::properties()
{
    init();
    return option_;
}

//
std::string reader::property( std::string name )
{
    std::string val;

    //
    init();

    //
    if ( option_ != nullptr )
    {
        auto itm = option_->find( name );

        if ( itm != option_->end() )
            val = itm->second;
    }
    else
        throw reader::exception( CRITICAL_ERROR, "Invalid Properties" );

    return val;
}

//
void reader::property( std::string name, std::string value )
{
    option_->insert( { name, value } );
}

//
void reader::set_case( std::string val )
{
    property( PROP_CASE, val );
}

//
void reader::set_scale( unsigned int val )
{
    property( PROP_SCALE, std::to_string( val ) );
}

//
void reader::set_precision( unsigned int val )
{
    property( PROP_PRECISION, std::to_string( val ) );
}

//
reader::Table reader::table()
{
    return table_;
}

reader::Column reader::column( Index i )
{
    //
    if ( ! is_open() )
        throw reader::exception( NOT_OPEN, "Not open" );

    //
    return table_->column( i );
}

//
reader::Columns reader::columns()
{
    //
    if ( ! is_open() )
        throw reader::exception( NOT_OPEN, "Not open" );

    //
    return table_->columns();
}

//
reader::Field reader::field( reader::Index i )
{
    //
    if ( ! is_open() )
        throw reader::exception( NOT_OPEN, "Not open" );

    //
    return table_->field( i );
}

//
reader::Fields reader::fields()
{
    //
    if ( ! is_open() )
        throw reader::exception( NOT_OPEN, "Not open" );

    //
    return table_->fields();
}

//
size_t reader::num_rows()
{
    //
    if ( table_ != nullptr )
        return table_->num_rows();
    else
        return 0;
}

//
size_t reader::num_cols()
{
    //
    if ( table_ != nullptr )
        return table_->num_columns();
    else
        return 0;
}

//
bool reader::is_open()
{
    //
    return ( table_ != nullptr );
}

//
std::string reader::name( reader::Index col )
{
    reader::Field fld = field( col );

    //
    if ( fld == nullptr )
        throw reader::exception( OUT_OF_RANGE, "Column [" + std::to_string( col ) + "] invalid" );

    //
    return use_case( fld->name() );
}

//
std::string reader::value( reader::Index col, reader::Index row )
{
    std::string val = "";

    //
    if ( ( col < num_cols() ) && ( row < num_rows() ) )
    {
        Column dat = column( col );

        if ( dat != nullptr )
        {
            //
            switch ( dat->type()->id() )
            {
                //
                case arrow::Type::type::BOOL:
                    {
                        auto ary = std::static_pointer_cast<arrow::BooleanArray>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = ( ary->Value( row ) ? "true" : "false" );
                        }
                    }
                    break;

                case arrow::Type::type::UINT8:
                    {
                        auto ary = std::static_pointer_cast<arrow::UInt8Array>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = std::to_string( ary->Value( row ) );
                        }
                    }
                    break;

                case arrow::Type::type::INT8:
                    {
                        auto ary = std::static_pointer_cast<arrow::Int8Array>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = std::to_string( ary->Value( row ) );
                        }
                    }
                    break;

                case arrow::Type::type::UINT16:
                    {
                        auto ary = std::static_pointer_cast<arrow::UInt16Array>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = std::to_string( ary->Value( row ) );
                        }
                    }
                    break;

                case arrow::Type::type::INT16:
                    {
                        auto ary = std::static_pointer_cast<arrow::Int16Array>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = std::to_string( ary->Value( row ) );
                        }
                    }
                    break;

                case arrow::Type::type::UINT32:
                    {
                        auto ary = std::static_pointer_cast<arrow::UInt32Array>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = std::to_string( ary->Value( row ) );
                        }
                    }
                    break;

                case arrow::Type::type::INT32:
                    {
                        auto ary = std::static_pointer_cast<arrow::Int32Array>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = std::to_string( ary->Value( row ) );
                        }
                    }
                    break;

                case arrow::Type::type::UINT64:
                    {
                        auto ary = std::static_pointer_cast<arrow::UInt64Array>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = std::to_string( ary->Value( row ) );
                        }
                    }
                    break;

                case arrow::Type::type::INT64:
                    {
                        auto ary = std::static_pointer_cast<arrow::Int64Array>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = std::to_string( ary->Value( row ) );
                        }
                    }
                    break;

                case arrow::Type::type::FLOAT:
                    {
                        auto ary = std::static_pointer_cast<arrow::FloatArray>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = std::to_string( ary->Value( row ) );
                        }
                    }
                    break;

                case arrow::Type::type::DECIMAL:
                    {
                        auto ary = std::static_pointer_cast<arrow::DecimalArray>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = arrow::Decimal128( ary->Value( row ) ).ToString( 0 );
                        }
                    }
                    break;

                case arrow::Type::type::DOUBLE:
                    {
                        auto ary = std::static_pointer_cast<arrow::DoubleArray>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = std::to_string( ary->Value( row ) );
                        }
                    }
                    break;

                case arrow::Type::type::STRING:
                    {
                        auto ary = std::static_pointer_cast<arrow::StringArray>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = ary->Value( row );
                        }
                    }
                    break;

                case arrow::Type::type::BINARY:
                    {
                        auto ary = std::static_pointer_cast<arrow::BinaryArray>( dat->chunk( 0 ) );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                                val = mti::crypto::b64().encode( std::string( ary->Value( row ) ) );
                        }
                    }
                    break;

                case arrow::Type::type::DATE32:
                    {
                        auto ary = std::static_pointer_cast<arrow::Date32Array>( dat->chunk( 0 ) );

                        // int
                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                            {
                                char buf[ 80 ] = { '\0' };
                                struct tm* tm = nullptr;
                                time_t tim = ( ary->Value( row ) / 1000 );

                                tm = std::localtime( &tim );
                                std::strftime( buf, 80,"%Y-%m-%d", tm );

                                val = std::string( buf );
                            }
                        }
                    }
                    break;

                case arrow::Type::type::DATE64:
                    {
                        auto ary = std::static_pointer_cast<arrow::Date64Array>( dat->chunk( 0 ) );

                        // long long
                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                            {
                                char buf[ 80 ] = { '\0' };
                                struct tm* tm = nullptr;
                                time_t tim = ( ( ary->Value( row ) / 1000 ) / 1000 );

                                tm = std::localtime( &tim );
                                std::strftime( buf, 80,"%Y-%m-%d", tm );

                                val = std::string( buf );
                            }
                        }
                    }
                    break;

                case arrow::Type::type::TIMESTAMP:
                    {
                        auto ary = std::static_pointer_cast<arrow::TimestampArray>( dat->chunk( 0 ) );

                        // long long
                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                            {
                                char buf[ 80 ] = { '\0' };
                                struct tm* tm = nullptr;
                                time_t tim = ( ( ary->Value( row ) / 1000 ) / 1000 );

                                tm = std::localtime( &tim );
                                std::strftime( buf, 80,"%Y-%m-%d %H:%M:%S", tm );

                                val = std::string( buf );
                            }
                        }
                    }
                    break;

                case arrow::Type::type::TIME32:
                    {
                        auto ary = std::static_pointer_cast<arrow::Time32Array>( dat->chunk( 0 ) );

                        // int
                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                            {
                                char buf[ 80 ] = { '\0' };
                                struct tm* tm = nullptr;
                                time_t tim = ( ary->Value( row ) / 1000 );

                                tm = std::localtime( &tim );
                                std::strftime( buf, 80,"%Y-%m-%d %H:%M:%S", tm );

                                val = std::string( buf );
                            }
                        }
                    }
                    break;

                case arrow::Type::type::TIME64:
                    {
                        auto ary = std::static_pointer_cast<arrow::Time64Array>( dat->chunk( 0 ) );

                        // long long
                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( row ) )
                            {
                                char buf[ 80 ] = { '\0' };
                                struct tm* tm = nullptr;
                                time_t tim = ( ( ary->Value( row ) / 1000 ) / 1000 );

                                tm = std::localtime( &tim );
                                std::strftime( buf, 80,"%Y-%m-%d %H:%M:%S", tm );

                                val = std::string( buf );
                            }
                        }
                    }
                    break;

                case arrow::Type::type::NA:
                    val = "";
                    break;

                case arrow::Type::type::INTERVAL_MONTHS:
                case arrow::Type::type::INTERVAL_DAY_TIME:
                case arrow::Type::type::HALF_FLOAT:
                default:
                    val = "--- UNSUPPORTED ---";
                    break;
            }
        }
        else
            throw reader::exception( OUT_OF_RANGE, "Column [" + std::to_string( col ) + "] invalid" );

    }
    else
        throw reader::exception( OUT_OF_RANGE, "Column [" + std::to_string( col )
                                             + "], Row [" + std::to_string( row )
                                             + "] out of range!" );

    return val;
}

//
std::string reader::key( std::string id, Index row )
{
    std::string val;

    //
    for ( size_t c = 0; c < num_cols() ; ++c )
    {
        //
        if ( use_case( id ) == name( c ) )
        {
            val = value( c, row );
            break;
        }
    }

    //
    if ( val.size() == 0 )
        val = uuid( 0 );

    return val;
}

//
std::string reader::json( Index row )
{
    mti::json doc;

    //
    if ( is_open() )
    {
        for ( Index c = 0; c < num_cols(); ++c )
            doc[ name( c ) ] = value( c, row );
    }
    else
        throw reader::exception( NOT_OPEN, "Not open" );

    //
    return mti::document::format::output( doc, false );
}

// static
std::string reader::to_type( reader::Column col )
{
    //
    if ( col == nullptr )
        throw reader::exception( INVALID_COLUMN, "Invalid column" );

    //
    return to_type( col->type() );
}

// static
std::string reader::to_type( reader::Type type )
{
    //
    if ( type == nullptr )
        throw reader::exception( INVALID_TYPE, "Invalid type" );

    //
    return to_type( type->id() );
}

// static
std::string reader::to_type( reader::Kind type )
{
    std::string str;

    //
    switch ( type )
    {
        //
        case arrow::Type::type::BOOL:
            str = "BOOL";
            break;

        case arrow::Type::type::UINT8:
            str = "UINT8";
            break;

        case arrow::Type::type::INT8:
            str = "INT8";
            break;

        case arrow::Type::type::UINT16:
            str = "UINT16";
            break;

        case arrow::Type::type::INT16:
            str = "INT16";
            break;

        case arrow::Type::type::UINT32:
            str = "UINT32";
            break;

        case arrow::Type::type::INT32:
            str = "INT32";
            break;

        case arrow::Type::type::UINT64:
            str = "UINT64";
            break;

        case arrow::Type::type::INT64:
            str = "INT64";
            break;

        case arrow::Type::type::HALF_FLOAT:
            str = "HALF_FLOAT";
            break;

        case arrow::Type::type::FLOAT:
            str = "FLOAT";
            break;

        case arrow::Type::type::DOUBLE:
            str = "DOUBLE";
            break;

        case arrow::Type::type::STRING:
            str = "STRING";
            break;

        case arrow::Type::type::BINARY:
            str = "BINARY";
            break;

        case arrow::Type::type::DATE32:
            str = "DATE32";
            break;

        case arrow::Type::type::DATE64:
            str = "DATE64";
            break;

        case arrow::Type::type::TIMESTAMP:
            str = "TIMESTAMP";
            break;

        case arrow::Type::type::TIME32:
            str = "TIME32";
            break;

        case arrow::Type::type::TIME64:
            str = "TIME64";
            break;

        case arrow::Type::type::NA:
            str = "NA";
            break;

        case arrow::Type::type::DECIMAL:
            str = "DECIMAL";
            break;

        case arrow::Type::type::INTERVAL_MONTHS:
            str = "INTERVAL_MONTHS";
            break;

        case arrow::Type::type::INTERVAL_DAY_TIME:
            str = "INTERVAL_DAY_TIME";
            break;

        default:
            // Unsupported types. test ensures that
            // when one of these are added build breaks.
            str = "UNKNOWN";
    }

    //
    return str;
}

//
void reader::init()
{
    //
    if ( option_ == nullptr )
    {
        //
        option_.reset( new reader::options() );

        //
        if ( option_ != nullptr )
        {
            property( PROP_CASE, DEFAULT_CASE );

            set_case( DEFAULT_CASE );
            set_scale( DEFAULT_SCALE );
            set_precision( DEFAULT_PRECISION );

            // option_->insert( { PROP_CASE, DEFAULT_CASE } );
            // option_->insert( { PROP_SCALE, std::to_string( DEFAULT_SCALE ) } );
            // option_->insert( { PROP_PRECISION, std::to_string( DEFAULT_PRECISION ) } );
            // // ... add more ...
        }
    }
}

//
std::string reader::use_case( std::string s )
{
    if ( to_lower( trim( property( "case" ) ) ) == "lower" )
        return to_lower( trim( s ) );
    else if ( to_lower( trim( property( "case" ) ) ) == "upper" )
        return to_upper( trim( s ) );
    else
        return trim( s );
}

//
std::string reader::to_lower( std::string s )
{
    std::transform( s.begin(), s.end(), s.begin(), ::tolower );
    return s;
}

//
std::string reader::to_upper( std::string s )
{
    std::transform( s.begin(), s.end(), s.begin(), ::toupper );
    return s;
}

//
std::string reader::trim( std::string s )
{
    return ltrim( rtrim( s ) );
}

//
std::string reader::ltrim( std::string s )
{
    //
    s.erase( s.begin(), std::find_if( s.begin(),
                                      s.end(),
                                      []( unsigned char c )
    {
        return !std::isspace( c );
    } ) );

    return s;
}

//
std::string reader::rtrim( std::string s )
{
    s.erase( std::find_if( s.rbegin(),
                           s.rend(),
                           []( unsigned char c )
    {
        return !std::isspace( c );
    } ).base(),
    s.end() );

    return s;
}

//
std::string reader::uuid( int sz /*= 0*/ )
{
    //
    static std::random_device              rd_;
    static std::mt19937                    gen_( rd_() );
    static std::uniform_int_distribution<> uni_( 0, 15 );
    static std::uniform_int_distribution<> dis_( 8, 11 );

    //
    std::stringstream ss;

    //
    ss << std::hex;

    //
    for ( int i = 0; i < 8; ++i )
        ss << uni_( gen_ );

    //
    ss << "-";
    for ( int i = 0; i < 4; ++i )
        ss << uni_( gen_ );

    //
    ss << "-4";
    for ( int i = 0; i < 3; ++i )
        ss << uni_( gen_ );

    //
    ss << "-";
    ss << dis_( gen_ );
    for ( int i = 0; i < 3; ++i )
        ss << uni_( gen_ );

    //
    ss << "-";
    for ( int i = 0; i < 12; ++i )
        ss << uni_( gen_ );

    //
    return ( ( sz == 0 ) ? ss.str()
                         : ss.str().substr( 1, sz ) );
}

}} // namespace mti::parq
