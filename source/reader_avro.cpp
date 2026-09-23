//
// reader_avro.cpp
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
#include <fstream>
#include <sstream>
#include <vector>
//
#include <arrow/builder.h>
// NOTE: avro-cpp 1.12 calls fmt::format from its public headers but only
// includes <fmt/core.h>. Since fmt 11 that header no longer pulls in format()
// unless asked, so pull it in here before anything avro -- otherwise every
// avro header fails to compile against a current fmt.
#if defined( __has_include )
#  if __has_include( <fmt/format.h> )
#    include <fmt/format.h>
#  endif
#endif
//
#include <avro/DataFile.hh>
#include <avro/Generic.hh>
#include <avro/GenericDatum.hh>
#include <avro/LogicalType.hh>
#include <avro/Node.hh>
#include <avro/Schema.hh>
#include <avro/ValidSchema.hh>
#include <avro/Exception.hh>
//
#include "reader_avro.hpp"

//
namespace mti { namespace parq {

//
namespace {

//
// ---------------------------------------------------------------------------
// schema mapping
// ---------------------------------------------------------------------------
//

// The non-null branch of a nullable union ( the ["null", T] idiom ), or null
// when the union is anything else -- avro allows arbitrary unions but there is
// no single column type that can represent one.
avro::NodePtr nullable_branch( const avro::NodePtr& node )
{
    avro::NodePtr found;

    for ( size_t i = 0; i < node->leaves(); ++i )
    {
        avro::NodePtr leaf = node->leafAt( i );

        if ( leaf->type() == avro::AVRO_NULL )
            continue;

        // more than one non-null branch -- not a plain nullable
        if ( found != nullptr )
            return avro::NodePtr();

        found = leaf;
    }

    return found;
}

// Unwrap a nullable union down to the branch that carries the data
avro::NodePtr effective( const avro::NodePtr& node )
{
    if ( ( node != nullptr ) && ( node->type() == avro::AVRO_UNION ) )
    {
        avro::NodePtr branch = nullable_branch( node );

        if ( branch != nullptr )
            return branch;
    }

    return node;
}

// Map an avro schema node onto the arrow type its values will be held in.
// Anything with no faithful column type -- records, arrays, maps, general
// unions -- becomes utf8 and is rendered as text; see to_text() below.
std::shared_ptr<arrow::DataType> arrow_type( const avro::NodePtr& node )
{
    avro::NodePtr n = effective( node );

    if ( n == nullptr )
        return arrow::utf8();

    avro::LogicalType lt = n->logicalType();

    switch ( n->type() )
    {
        case avro::AVRO_NULL:
            return arrow::null();

        case avro::AVRO_BOOL:
            return arrow::boolean();

        case avro::AVRO_INT:
            if ( lt.type() == avro::LogicalType::DATE )
                return arrow::date32();

            if ( lt.type() == avro::LogicalType::TIME_MILLIS )
                return arrow::time32( arrow::TimeUnit::MILLI );

            return arrow::int32();

        case avro::AVRO_LONG:
            switch ( lt.type() )
            {
                case avro::LogicalType::TIME_MICROS:
                    return arrow::time64( arrow::TimeUnit::MICRO );

                case avro::LogicalType::TIMESTAMP_MILLIS:
#ifdef AVRO_HAS_EXTENDED_TIMESTAMPS
                case avro::LogicalType::LOCAL_TIMESTAMP_MILLIS:
#endif
                    return arrow::timestamp( arrow::TimeUnit::MILLI );

                case avro::LogicalType::TIMESTAMP_MICROS:
#ifdef AVRO_HAS_EXTENDED_TIMESTAMPS
                case avro::LogicalType::LOCAL_TIMESTAMP_MICROS:
#endif
                    return arrow::timestamp( arrow::TimeUnit::MICRO );

#ifdef AVRO_HAS_EXTENDED_TIMESTAMPS
                // these logical types only exist from avro-cpp 1.12.2; on older
                // releases such columns fall through to INT64
                case avro::LogicalType::TIMESTAMP_NANOS:
                case avro::LogicalType::LOCAL_TIMESTAMP_NANOS:
                    return arrow::timestamp( arrow::TimeUnit::NANO );
#endif

                default:
                    return arrow::int64();
            }

        case avro::AVRO_FLOAT:
            return arrow::float32();

        case avro::AVRO_DOUBLE:
            return arrow::float64();

        case avro::AVRO_STRING:
            return arrow::utf8();

        case avro::AVRO_ENUM:
            return arrow::utf8();

        case avro::AVRO_BYTES:
            if ( lt.type() == avro::LogicalType::DECIMAL )
                return arrow::decimal128( lt.precision(), lt.scale() );

            return arrow::binary();

        case avro::AVRO_FIXED:
            if ( lt.type() == avro::LogicalType::DECIMAL )
                return arrow::decimal128( lt.precision(), lt.scale() );

            return arrow::fixed_size_binary( static_cast<int32_t>( n->fixedSize() ) );

        default:
            // record / array / map / general union -- rendered as text
            return arrow::utf8();
    }
}

//
// ---------------------------------------------------------------------------
// value rendering
// ---------------------------------------------------------------------------
//

std::string to_text( const avro::GenericDatum& d );

// Quote and escape a string for the compact JSON used to render nested values
std::string quote( const std::string& s )
{
    std::ostringstream os;

    os << '"';

    for ( unsigned char c : s )
    {
        switch ( c )
        {
            case '"':  os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\b': os << "\\b";  break;
            case '\f': os << "\\f";  break;
            case '\n': os << "\\n";  break;
            case '\r': os << "\\r";  break;
            case '\t': os << "\\t";  break;
            default:
                if ( c < 0x20 )
                {
                    static const char* hex = "0123456789abcdef";

                    os << "\\u00" << hex[ ( c >> 4 ) & 0xF ] << hex[ c & 0xF ];
                }
                else
                    os << static_cast<char>( c );
                break;
        }
    }

    os << '"';

    return os.str();
}

//
std::string bytes_to_text( const std::vector<uint8_t>& v )
{
    static const char* hex = "0123456789abcdef";
    std::string out;

    out.reserve( v.size() * 2 );

    for ( uint8_t b : v )
    {
        out += hex[ ( b >> 4 ) & 0xF ];
        out += hex[ b & 0xF ];
    }

    return out;
}

// Render any datum as compact JSON. Used for the nested avro types that have
// no flat column representation, so they show real data rather than a
// placeholder.
std::string to_text( const avro::GenericDatum& d )
{
    switch ( d.type() )
    {
        case avro::AVRO_NULL:
            return "null";

        case avro::AVRO_BOOL:
            return d.value<bool>() ? "true" : "false";

        case avro::AVRO_INT:
            return std::to_string( d.value<int32_t>() );

        case avro::AVRO_LONG:
            return std::to_string( d.value<int64_t>() );

        case avro::AVRO_FLOAT:
            return std::to_string( d.value<float>() );

        case avro::AVRO_DOUBLE:
            return std::to_string( d.value<double>() );

        case avro::AVRO_STRING:
            return quote( d.value<std::string>() );

        case avro::AVRO_BYTES:
            return quote( bytes_to_text( d.value<std::vector<uint8_t>>() ) );

        case avro::AVRO_FIXED:
            return quote( bytes_to_text( d.value<avro::GenericFixed>().value() ) );

        case avro::AVRO_ENUM:
            {
                const avro::GenericEnum& e = d.value<avro::GenericEnum>();

                return quote( e.schema()->nameAt( e.value() ) );
            }

        case avro::AVRO_RECORD:
            {
                const avro::GenericRecord& r = d.value<avro::GenericRecord>();
                std::ostringstream os;

                os << "{";

                for ( size_t i = 0; i < r.fieldCount(); ++i )
                {
                    if ( i > 0 )
                        os << ",";

                    os << quote( r.schema()->nameAt( i ) ) << ":" << to_text( r.fieldAt( i ) );
                }

                os << "}";

                return os.str();
            }

        case avro::AVRO_ARRAY:
            {
                const avro::GenericArray::Value& v = d.value<avro::GenericArray>().value();
                std::ostringstream os;

                os << "[";

                for ( size_t i = 0; i < v.size(); ++i )
                {
                    if ( i > 0 )
                        os << ",";

                    os << to_text( v[ i ] );
                }

                os << "]";

                return os.str();
            }

        case avro::AVRO_MAP:
            {
                const avro::GenericMap::Value& v = d.value<avro::GenericMap>().value();
                std::ostringstream os;

                os << "{";

                for ( size_t i = 0; i < v.size(); ++i )
                {
                    if ( i > 0 )
                        os << ",";

                    os << quote( v[ i ].first ) << ":" << to_text( v[ i ].second );
                }

                os << "}";

                return os.str();
            }

        default:
            return "null";
    }
}

//
// ---------------------------------------------------------------------------
// appending
// ---------------------------------------------------------------------------
//

// Append one avro datum to the arrow builder for its column. Type mismatches
// are appended as null rather than throwing, so a single odd row cannot abort
// a whole file.
arrow::Status append( arrow::ArrayBuilder* bld,
                      const std::shared_ptr<arrow::DataType>& typ,
                      const avro::GenericDatum& d )
{
    // an unselected null union branch, or an actual null
    if ( d.type() == avro::AVRO_NULL )
        return bld->AppendNull();

    switch ( typ->id() )
    {
        case arrow::Type::BOOL:
            return static_cast<arrow::BooleanBuilder*>( bld )->Append( d.value<bool>() );

        case arrow::Type::INT32:
            return static_cast<arrow::Int32Builder*>( bld )->Append( d.value<int32_t>() );

        case arrow::Type::INT64:
            return static_cast<arrow::Int64Builder*>( bld )->Append( d.value<int64_t>() );

        case arrow::Type::FLOAT:
            return static_cast<arrow::FloatBuilder*>( bld )->Append( d.value<float>() );

        case arrow::Type::DOUBLE:
            return static_cast<arrow::DoubleBuilder*>( bld )->Append( d.value<double>() );

        case arrow::Type::DATE32:
            return static_cast<arrow::Date32Builder*>( bld )->Append( d.value<int32_t>() );

        case arrow::Type::TIME32:
            return static_cast<arrow::Time32Builder*>( bld )->Append( d.value<int32_t>() );

        case arrow::Type::TIME64:
            return static_cast<arrow::Time64Builder*>( bld )->Append( d.value<int64_t>() );

        case arrow::Type::TIMESTAMP:
            return static_cast<arrow::TimestampBuilder*>( bld )->Append( d.value<int64_t>() );

        case arrow::Type::BINARY:
            {
                const std::vector<uint8_t>& v = d.value<std::vector<uint8_t>>();

                return static_cast<arrow::BinaryBuilder*>( bld )->Append(
                    v.empty() ? reinterpret_cast<const uint8_t*>( "" ) : v.data(),
                    static_cast<int32_t>( v.size() ) );
            }

        case arrow::Type::FIXED_SIZE_BINARY:
            {
                const std::vector<uint8_t>& v = d.value<avro::GenericFixed>().value();

                return static_cast<arrow::FixedSizeBinaryBuilder*>( bld )->Append( v.data() );
            }

        case arrow::Type::DECIMAL128:
            {
                // avro holds a decimal as big endian two's complement bytes,
                // in either a bytes or a fixed field
                std::vector<uint8_t> v = ( d.type() == avro::AVRO_FIXED )
                                       ? d.value<avro::GenericFixed>().value()
                                       : d.value<std::vector<uint8_t>>();

                if ( v.empty() )
                    return bld->AppendNull();

                auto res = arrow::Decimal128::FromBigEndian( v.data(),
                                                             static_cast<int32_t>( v.size() ) );

                if ( ! res.ok() )
                    return bld->AppendNull();

                return static_cast<arrow::Decimal128Builder*>( bld )->Append( *res );
            }

        case arrow::Type::NA:
            return bld->AppendNull();

        case arrow::Type::STRING:
        default:
            {
                // plain strings and enums render as themselves; everything
                // else lands here as compact JSON
                if ( d.type() == avro::AVRO_STRING )
                    return static_cast<arrow::StringBuilder*>( bld )->Append( d.value<std::string>() );

                if ( d.type() == avro::AVRO_ENUM )
                {
                    const avro::GenericEnum& e = d.value<avro::GenericEnum>();

                    return static_cast<arrow::StringBuilder*>( bld )->Append( e.schema()->nameAt( e.value() ) );
                }

                return static_cast<arrow::StringBuilder*>( bld )->Append( to_text( d ) );
            }
    }
}

//
// ---------------------------------------------------------------------------
// container file header
// ---------------------------------------------------------------------------
//

// avro-cpp keeps the codec and the file metadata private, so the header is
// walked here to report them. Layout is: magic, a map of metadata, a 16 byte
// sync marker, then blocks of ( object count, byte size, data, sync ).
struct header_info
{
    std::string codec = "null";
    std::string created = "unknown";
    size_t blocks = 0;
    bool valid = false;
};

//
bool read_varint( std::istream& in, int64_t& out )
{
    uint64_t val = 0;
    int shift = 0;

    for ( ;; )
    {
        int c = in.get();

        if ( c == EOF )
            return false;

        val |= ( static_cast<uint64_t>( c & 0x7F ) << shift );

        if ( ( c & 0x80 ) == 0 )
            break;

        shift += 7;

        if ( shift > 63 )
            return false;
    }

    // zig zag
    out = static_cast<int64_t>( ( val >> 1 ) ^ ( ~( val & 1 ) + 1 ) );

    return true;
}

//
bool read_bytes( std::istream& in, std::string& out )
{
    int64_t len = 0;

    if ( ! read_varint( in, len ) )
        return false;

    if ( ( len < 0 ) || ( len > ( 1 << 26 ) ) )
        return false;

    out.assign( static_cast<size_t>( len ), '\0' );

    if ( len > 0 )
        in.read( &out[ 0 ], len );

    return in.good();
}

//
header_info read_header( const std::string& path )
{
    header_info info;
    std::ifstream in( path, std::ios::binary );

    if ( ! in.is_open() )
        return info;

    char magic[ 4 ] = { 0 };

    in.read( magic, 4 );

    if ( ( ! in.good() ) || ( magic[ 0 ] != 'O' ) || ( magic[ 1 ] != 'b' )
                         || ( magic[ 2 ] != 'j' ) || ( magic[ 3 ] != 1 ) )
        return info;

    // metadata map
    for ( ;; )
    {
        int64_t count = 0;

        if ( ! read_varint( in, count ) )
            return info;

        if ( count == 0 )
            break;

        if ( count < 0 )
        {
            // negative count is followed by the block byte size
            int64_t ignored = 0;

            if ( ! read_varint( in, ignored ) )
                return info;

            count = -count;
        }

        for ( int64_t i = 0; i < count; ++i )
        {
            std::string key;
            std::string val;

            if ( ( ! read_bytes( in, key ) ) || ( ! read_bytes( in, val ) ) )
                return info;

            if ( key == "avro.codec" )
                info.codec = val;
            else if ( ( key == "created_by" ) || ( key == "avro.created.by" ) || ( key == "writer" ) )
                info.created = val;
        }
    }

    // sync marker
    in.seekg( 16, std::ios::cur );

    if ( ! in.good() )
        return info;

    // walk the blocks
    for ( ;; )
    {
        int64_t objects = 0;
        int64_t bytes = 0;

        if ( ! read_varint( in, objects ) )
            break;

        if ( ! read_varint( in, bytes ) )
            break;

        if ( bytes < 0 )
            break;

        in.seekg( bytes + 16, std::ios::cur );

        if ( ! in.good() )
            break;

        ++info.blocks;

        if ( in.peek() == EOF )
            break;
    }

    info.valid = true;

    return info;
}

} // anonymous namespace

//
avro_reader::avro_reader() : codec_( "unknown" ),
                             created_( "unknown" ),
                             blocks_( 0 )
{
}

//
avro_reader::avro_reader( const char* file ) : codec_( "unknown" ),
                                               created_( "unknown" ),
                                               blocks_( 0 )
{
    open( file );
}

//
avro_reader::avro_reader( std::string file ) : codec_( "unknown" ),
                                               created_( "unknown" ),
                                               blocks_( 0 )
{
    open( file.c_str() );
}

//
avro_reader::~avro_reader()
{
}

//
std::string avro_reader::format() const
{
    return "avro";
}

//
void avro_reader::open( const char* file )
{
    //
    if ( is_open() )
        throw reader::exception( ALREADY_OPEN, "Already open" );

    //
    if ( file == nullptr )
        throw reader::exception( MISSING_FILE, "Invalid file name!" );

    try
    {
        avro::DataFileReader<avro::GenericDatum> rdr( file );

        const avro::ValidSchema& schema = rdr.dataSchema();
        avro::NodePtr root = schema.root();

        // a container file is normally a record per row; anything else is
        // taken as a single unnamed column
        bool is_record = ( root->type() == avro::AVRO_RECORD );
        size_t cols = is_record ? root->leaves() : 1;

        std::vector<std::shared_ptr<arrow::Field>> fields;
        std::vector<std::unique_ptr<arrow::ArrayBuilder>> builders;
        std::vector<std::shared_ptr<arrow::DataType>> types;

        for ( size_t i = 0; i < cols; ++i )
        {
            avro::NodePtr leaf = is_record ? root->leafAt( i ) : root;
            std::string name = is_record ? root->nameAt( i ) : std::string( "value" );
            std::shared_ptr<arrow::DataType> typ = arrow_type( leaf );

            std::unique_ptr<arrow::ArrayBuilder> bld;

            if ( ! arrow::MakeBuilder( arrow::default_memory_pool(), typ, &bld ).ok() )
                throw reader::exception( INVALID_TYPE,
                                         "Unsupported avro type for column '" + name + "'" );

            fields.push_back( arrow::field( name, typ ) );
            types.push_back( typ );
            builders.push_back( std::move( bld ) );
        }

        // rows
        avro::GenericDatum datum( schema );

        while ( rdr.read( datum ) )
        {
            if ( is_record )
            {
                const avro::GenericRecord& rec = datum.value<avro::GenericRecord>();

                for ( size_t i = 0; i < cols; ++i )
                {
                    if ( ! append( builders[ i ].get(), types[ i ], rec.fieldAt( i ) ).ok() )
                        ( void ) builders[ i ]->AppendNull();
                }
            }
            else
            {
                if ( ! append( builders[ 0 ].get(), types[ 0 ], datum ).ok() )
                    ( void ) builders[ 0 ]->AppendNull();
            }
        }

        // finish
        std::vector<std::shared_ptr<arrow::Array>> arrays;

        for ( size_t i = 0; i < cols; ++i )
        {
            std::shared_ptr<arrow::Array> ary;

            if ( ! builders[ i ]->Finish( &ary ).ok() )
                throw reader::exception( CRITICAL_ERROR, "Could not build column data" );

            arrays.push_back( ary );
        }

        table_ = arrow::Table::Make( arrow::schema( fields ), arrays );

        if ( table_ == nullptr )
            throw reader::exception( CRITICAL_ERROR, "Could not build table" );

        filename_ = file;

        // codec, writer and block count come from the container header
        header_info info = read_header( filename_ );

        if ( info.valid )
        {
            codec_ = info.codec;
            created_ = info.created;
            blocks_ = info.blocks;
        }
    }
    catch ( reader::exception& )
    {
        throw;
    }
    catch ( avro::Exception& ex )
    {
        throw reader::exception( CORRUPTED_FILE, ex.what() );
    }
    catch ( std::exception& ex )
    {
        throw reader::exception( UNKNOWN_ERROR, ex.what() );
    }
    catch ( ... )
    {
        throw reader::exception( UNKNOWN_ERROR, "Unknown exception!" );
    }
}

//
void avro_reader::close()
{
    //
    codec_ = "unknown";
    created_ = "unknown";
    blocks_ = 0;

    //
    reader::close();
}

//
size_t avro_reader::num_row_groups() const
{
    return blocks_;
}

//
std::string avro_reader::created_by() const
{
    return created_;
}

//
std::string avro_reader::compression_type( reader::Index col ) const
{
    ( void ) col;

    // avro compresses whole blocks, so the codec is a property of the file and
    // every column reports the same value
    return codec_.empty() ? std::string( "none" )
                          : ( ( codec_ == "null" ) ? std::string( "none" ) : codec_ );
}

}} // namespace mti::parq
