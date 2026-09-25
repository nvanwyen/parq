//
// reader.cpp
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
#include <cctype>
#include <ctime>
#include <locale>
#include <random>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <iomanip>
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
namespace {

// Floor division / modulus. Plain / and % truncate toward zero, which would
// push pre-epoch (negative) values into the wrong day or second.
int64_t floor_div( int64_t a, int64_t b )
{
    int64_t q = a / b;

    if ( ( a % b != 0 ) && ( ( a < 0 ) != ( b < 0 ) ) )
        --q;

    return q;
}

//
int64_t floor_mod( int64_t a, int64_t b )
{
    int64_t r = a % b;

    if ( ( r != 0 ) && ( ( r < 0 ) != ( b < 0 ) ) )
        r += b;

    return r;
}

// Sub-second units per second for an Arrow time unit
int64_t units_per_second( arrow::TimeUnit::type unit )
{
    switch ( unit )
    {
        case arrow::TimeUnit::SECOND: return 1LL;
        case arrow::TimeUnit::MILLI:  return 1000LL;
        case arrow::TimeUnit::MICRO:  return 1000000LL;
        case arrow::TimeUnit::NANO:   return 1000000000LL;
    }

    return 1LL;
}

// Render the fractional part, trimmed of trailing zeros. Empty when whole.
std::string fraction( int64_t sub, int64_t per_second )
{
    if ( ( sub == 0 ) || ( per_second <= 1 ) )
        return std::string();

    int digits = 0;

    for ( int64_t v = per_second; v > 1; v /= 10 )
        ++digits;

    std::ostringstream os;
    os << std::setfill( '0' ) << std::setw( digits ) << sub;

    std::string str = os.str();

    while ( ( ! str.empty() ) && ( str.back() == '0' ) )
        str.pop_back();

    return str.empty() ? std::string() : ( "." + str );
}

// Render an absolute instant, given as seconds since the Unix epoch, in UTC.
// Date and timestamp columns carry no zone, so rendering them in local time
// would shift the value the file actually holds.
std::string format_instant( int64_t secs, const char* fmt )
{
    std::time_t tim = static_cast<std::time_t>( secs );
    struct tm tmv = {};

    if ( gmtime_r( &tim, &tmv ) == nullptr )
        return std::string();

    char buf[ 80 ] = { '\0' };

    if ( std::strftime( buf, sizeof( buf ), fmt, &tmv ) == 0 )
        return std::string();

    return std::string( buf );
}

// Render a time of day held as a count of sub-second units since midnight
std::string format_time_of_day( int64_t value, arrow::TimeUnit::type unit )
{
    int64_t per_second = units_per_second( unit );
    int64_t secs = floor_div( value, per_second );
    int64_t sub = floor_mod( value, per_second );

    int64_t hh = floor_div( secs, 3600 ) % 24;
    int64_t mm = floor_div( secs, 60 ) % 60;
    int64_t ss = floor_mod( secs, 60 );

    std::ostringstream os;

    os << std::setfill( '0' )
       << std::setw( 2 ) << hh << ":"
       << std::setw( 2 ) << mm << ":"
       << std::setw( 2 ) << ss
       << fraction( sub, per_second );

    return os.str();
}

// Resolve a table global row number to the chunk that actually holds it.
//
// A column is a ChunkedArray and the chunking is not visible to the caller:
// parquet normally comes back coalesced into a single chunk, but orc returns
// one chunk per stripe, and arrow is free to split any column ( a binary
// column crossing the 2 GB offset limit, for one ). Indexing chunk( 0 ) with a
// table global row -- as this did -- is an out of bounds read the moment a
// column has more than one chunk.
bool locate( const std::shared_ptr<arrow::ChunkedArray>& col,
             int64_t row,
             std::shared_ptr<arrow::Array>& out,
             int64_t& idx )
{
    if ( ( col == nullptr ) || ( row < 0 ) )
        return false;

    for ( int i = 0; i < col->num_chunks(); ++i )
    {
        std::shared_ptr<arrow::Array> chk = col->chunk( i );

        if ( chk == nullptr )
            return false;

        if ( row < chk->length() )
        {
            out = chk;
            idx = row;

            return true;
        }

        row -= chk->length();
    }

    return false;
}

} // anonymous namespace

//
reader::reader() : table_( nullptr )
{
    init();
}

//
reader::~reader()
{
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
std::string reader::property( std::string name ) const
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
    //
    init();

    // NOTE: assign, do not insert. map::insert will not overwrite an existing
    // key, and init() has already put the defaults in -- so every set_case(),
    // set_scale() and set_precision() call was silently doing nothing.
    ( *option_ )[ name ] = value;
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
reader::Table reader::table() const
{
    return table_;
}

reader::Column reader::column( Index i ) const
{
    //
    if ( ! is_open() )
        throw reader::exception( NOT_OPEN, "Not open" );

    //
    return table_->column( i );
}

//
reader::Columns reader::columns() const
{
    //
    if ( ! is_open() )
        throw reader::exception( NOT_OPEN, "Not open" );

    //
    return table_->columns();
}

//
reader::Field reader::field( reader::Index i ) const
{
    //
    if ( ! is_open() )
        throw reader::exception( NOT_OPEN, "Not open" );

    //
    return table_->field( i );
}

//
reader::Fields reader::fields() const
{
    //
    if ( ! is_open() )
        throw reader::exception( NOT_OPEN, "Not open" );

    //
    return table_->fields();
}

//
size_t reader::num_rows() const
{
    //
    if ( table_ != nullptr )
        return table_->num_rows();
    else
        return 0;
}

//
size_t reader::num_cols() const
{
    //
    if ( table_ != nullptr )
        return table_->num_columns();
    else
        return 0;
}

//
bool reader::is_open() const
{
    //
    return ( table_ != nullptr );
}

//
std::string reader::name( reader::Index col ) const
{
    reader::Field fld = field( col );

    //
    if ( fld == nullptr )
        throw reader::exception( OUT_OF_RANGE, "Column [" + std::to_string( col ) + "] invalid" );

    //
    return use_case( fld->name() );
}

//
std::string reader::value( reader::Index col, reader::Index row ) const
{
    std::string val = "";

    //
    if ( ( col < num_cols() ) && ( row < num_rows() ) )
    {
        Column dat = column( col );

        if ( dat != nullptr )
        {
            // find the chunk holding this row before touching any of it
            std::shared_ptr<arrow::Array> chk;
            int64_t idx = 0;

            if ( ! locate( dat, static_cast<int64_t>( row ), chk, idx ) )
                throw reader::exception( OUT_OF_RANGE, "Column [" + std::to_string( col )
                                                     + "], Row [" + std::to_string( row )
                                                     + "] out of range!" );

            //
            switch ( dat->type()->id() )
            {
                //
                case arrow::Type::type::BOOL:
                    {
                        auto ary = std::static_pointer_cast<arrow::BooleanArray>( chk );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = ( ary->Value( idx ) ? "true" : "false" );
                        }
                    }
                    break;

                case arrow::Type::type::UINT8:
                    {
                        auto ary = std::static_pointer_cast<arrow::UInt8Array>( chk );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = std::to_string( ary->Value( idx ) );
                        }
                    }
                    break;

                case arrow::Type::type::INT8:
                    {
                        auto ary = std::static_pointer_cast<arrow::Int8Array>( chk );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = std::to_string( ary->Value( idx ) );
                        }
                    }
                    break;

                case arrow::Type::type::UINT16:
                    {
                        auto ary = std::static_pointer_cast<arrow::UInt16Array>( chk );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = std::to_string( ary->Value( idx ) );
                        }
                    }
                    break;

                case arrow::Type::type::INT16:
                    {
                        auto ary = std::static_pointer_cast<arrow::Int16Array>( chk );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = std::to_string( ary->Value( idx ) );
                        }
                    }
                    break;

                case arrow::Type::type::UINT32:
                    {
                        auto ary = std::static_pointer_cast<arrow::UInt32Array>( chk );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = std::to_string( ary->Value( idx ) );
                        }
                    }
                    break;

                case arrow::Type::type::INT32:
                    {
                        auto ary = std::static_pointer_cast<arrow::Int32Array>( chk );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = std::to_string( ary->Value( idx ) );
                        }
                    }
                    break;

                case arrow::Type::type::UINT64:
                    {
                        auto ary = std::static_pointer_cast<arrow::UInt64Array>( chk );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = std::to_string( ary->Value( idx ) );
                        }
                    }
                    break;

                case arrow::Type::type::INT64:
                    {
                        auto ary = std::static_pointer_cast<arrow::Int64Array>( chk );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = std::to_string( ary->Value( idx ) );
                        }
                    }
                    break;

                case arrow::Type::type::FLOAT:
                    {
                        auto ary = std::static_pointer_cast<arrow::FloatArray>( chk );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = std::to_string( ary->Value( idx ) );
                        }
                    }
                    break;

                case arrow::Type::type::DECIMAL:
                    {
                        auto ary = std::static_pointer_cast<arrow::Decimal128Array>( chk );
                        auto typ = std::static_pointer_cast<arrow::Decimal128Type>( dat->type() );

                        if ( ( ary != nullptr ) && ( typ != nullptr ) )
                        {
                            // the scale belongs to the column type; assuming 0
                            // here would silently shift the decimal point
                            if ( ! ary->IsNull( idx ) )
                                val = arrow::Decimal128( ary->Value( idx ) ).ToString( typ->scale() );
                        }
                    }
                    break;

                case arrow::Type::type::DECIMAL256:
                    {
                        auto ary = std::static_pointer_cast<arrow::Decimal256Array>( chk );
                        auto typ = std::static_pointer_cast<arrow::Decimal256Type>( dat->type() );

                        if ( ( ary != nullptr ) && ( typ != nullptr ) )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = arrow::Decimal256( ary->Value( idx ) ).ToString( typ->scale() );
                        }
                    }
                    break;

                case arrow::Type::type::DOUBLE:
                    {
                        auto ary = std::static_pointer_cast<arrow::DoubleArray>( chk );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = std::to_string( ary->Value( idx ) );
                        }
                    }
                    break;

                case arrow::Type::type::STRING:
                    {
                        auto ary = std::static_pointer_cast<arrow::StringArray>( chk );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = ary->Value( idx );
                        }
                    }
                    break;

                case arrow::Type::type::BINARY:
                    {
                        auto ary = std::static_pointer_cast<arrow::BinaryArray>( chk );

                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = mti::crypto::b64().encode( std::string( ary->Value( idx ) ) );
                        }
                    }
                    break;

                case arrow::Type::type::DATE32:
                    {
                        auto ary = std::static_pointer_cast<arrow::Date32Array>( chk );

                        // days since the epoch
                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = format_instant( static_cast<int64_t>( ary->Value( idx ) ) * 86400LL,
                                                      "%Y-%m-%d" );
                        }
                    }
                    break;

                case arrow::Type::type::DATE64:
                    {
                        auto ary = std::static_pointer_cast<arrow::Date64Array>( chk );

                        // milliseconds since the epoch
                        if ( ary != nullptr )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = format_instant( floor_div( ary->Value( idx ), 1000LL ),
                                                      "%Y-%m-%d" );
                        }
                    }
                    break;

                case arrow::Type::type::TIMESTAMP:
                    {
                        auto ary = std::static_pointer_cast<arrow::TimestampArray>( chk );
                        auto typ = std::static_pointer_cast<arrow::TimestampType>( dat->type() );

                        // the unit is part of the column type, not fixed
                        if ( ( ary != nullptr ) && ( typ != nullptr ) )
                        {
                            if ( ! ary->IsNull( idx ) )
                            {
                                int64_t per = units_per_second( typ->unit() );
                                int64_t raw = ary->Value( idx );

                                val = format_instant( floor_div( raw, per ), "%Y-%m-%d %H:%M:%S" )
                                    + fraction( floor_mod( raw, per ), per );
                            }
                        }
                    }
                    break;

                case arrow::Type::type::TIME32:
                    {
                        auto ary = std::static_pointer_cast<arrow::Time32Array>( chk );
                        auto typ = std::static_pointer_cast<arrow::Time32Type>( dat->type() );

                        // a time of day, not an instant
                        if ( ( ary != nullptr ) && ( typ != nullptr ) )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = format_time_of_day( ary->Value( idx ), typ->unit() );
                        }
                    }
                    break;

                case arrow::Type::type::TIME64:
                    {
                        auto ary = std::static_pointer_cast<arrow::Time64Array>( chk );
                        auto typ = std::static_pointer_cast<arrow::Time64Type>( dat->type() );

                        // a time of day, not an instant
                        if ( ( ary != nullptr ) && ( typ != nullptr ) )
                        {
                            if ( ! ary->IsNull( idx ) )
                                val = format_time_of_day( ary->Value( idx ), typ->unit() );
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
int64_t reader::file_size() const
{
    // NOTE: this used to report parquet's FileMetaData::size(), which is the
    // size of the thrift metadata block, not of the file -- an 1878 byte file
    // reported 989 bytes. Ask the filesystem instead, which is also the only
    // thing that works for a format with no such metadata at all.
    if ( filename_.empty() )
        return -1;

    struct stat st;

    if ( ::stat( filename_.c_str(), &st ) != 0 )
        return -1;

    return static_cast<int64_t>( st.st_size );
}

//
// Formats with no per column default use this; only the avro reader overrides
// it. Returning empty rather than something like "n/a" keeps the metadata
// table blank for parquet and orc instead of filling a column with noise.
std::string reader::default_value( Index ) const
{
    return std::string();
}

//
bool reader::is_nullable( Index col ) const
{
    Field f = field( col );

    return ( f != nullptr ) ? f->nullable() : true;
}

//
std::string reader::file_checksum() const
{
    std::string checksum = "";
    
    if (!filename_.empty()) {
        std::ifstream file(filename_, std::ios::binary);
        if (file.is_open()) {
            // Use the modern EVP API for OpenSSL 3.0
            EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
            if (mdctx != nullptr) {
                if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) == 1) {
                    const size_t bufferSize = 8192;
                    char buffer[bufferSize];
                    
                    while (file.read(buffer, bufferSize)) {
                        EVP_DigestUpdate(mdctx, buffer, file.gcount());
                    }
                    // Process any remaining bytes
                    if (file.gcount() > 0) {
                        EVP_DigestUpdate(mdctx, buffer, file.gcount());
                    }
                    
                    unsigned char hash[EVP_MAX_MD_SIZE];
                    unsigned int hash_len;
                    
                    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) == 1) {
                        // Convert to hex string
                        std::stringstream ss;
                        for (unsigned int i = 0; i < hash_len; i++) {
                            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
                        }
                        checksum = ss.str();
                    }
                }
                EVP_MD_CTX_free(mdctx);
            }
            
            file.close();
        }
    }
    
    return checksum.empty() ? "unavailable" : checksum;
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

        case arrow::Type::type::DECIMAL256:
            str = "DECIMAL256";
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
void reader::init() const
{
    //
    if ( option_ == nullptr )
    {
        //
        option_.reset( new reader::options() );

        //
        if ( option_ != nullptr )
        {
            option_->insert( { PROP_CASE, DEFAULT_CASE } );
            option_->insert( { PROP_SCALE, std::to_string( DEFAULT_SCALE ) } );
            option_->insert( { PROP_PRECISION, std::to_string( DEFAULT_PRECISION ) } );
        }
    }
}

//
std::string reader::use_case( std::string s ) const
{
    if ( to_lower( trim( property( "case" ) ) ) == "lower" )
        return to_lower( trim( s ) );
    else if ( to_lower( trim( property( "case" ) ) ) == "upper" )
        return to_upper( trim( s ) );
    else
        return trim( s );
}

//
std::string reader::to_lower( std::string s ) const
{
    std::transform( s.begin(), s.end(), s.begin(), ::tolower );
    return s;
}

//
std::string reader::to_upper( std::string s ) const
{
    std::transform( s.begin(), s.end(), s.begin(), ::toupper );
    return s;
}

//
std::string reader::trim( std::string s ) const
{
    return ltrim( rtrim( s ) );
}

//
std::string reader::ltrim( std::string s ) const
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
std::string reader::rtrim( std::string s ) const
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
