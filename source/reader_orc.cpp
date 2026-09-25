//
// reader_orc.cpp
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
#include <arrow/io/api.h>
#include <arrow/type_fwd.h>
//
#include <arrow/adapters/orc/adapter.h>
#include <arrow/adapters/orc/options.h>
//
#include "reader_orc.hpp"

//
namespace mti { namespace parq {

//
namespace {

// The writer that produced the file. ORC records this as an id plus a version
// rather than the free text string parquet carries, so it is composed here.
std::string writer_name( arrow::adapters::orc::WriterId id, int32_t val )
{
    switch ( id )
    {
        case arrow::adapters::orc::WriterId::kOrcJava:     return "ORC Java";
        case arrow::adapters::orc::WriterId::kOrcCpp:      return "ORC C++";
        case arrow::adapters::orc::WriterId::kPresto:      return "Presto";
        case arrow::adapters::orc::WriterId::kScritchleyGo: return "Go";
        case arrow::adapters::orc::WriterId::kTrino:       return "Trino";
        case arrow::adapters::orc::WriterId::kUnknown:     break;
    }

    return "unknown writer (" + std::to_string( val ) + ")";
}

// The names below are the ORC release or Hive issue that introduced each
// writer behaviour; they are what the ORC tooling prints.
std::string writer_version( arrow::adapters::orc::WriterVersion ver )
{
    switch ( ver )
    {
        case arrow::adapters::orc::WriterVersion::kOriginal:  return "original";
        case arrow::adapters::orc::WriterVersion::kHive8732:  return "HIVE-8732";
        case arrow::adapters::orc::WriterVersion::kHive4243:  return "HIVE-4243";
        case arrow::adapters::orc::WriterVersion::kHive12055: return "HIVE-12055";
        case arrow::adapters::orc::WriterVersion::kHive13083: return "HIVE-13083";
        case arrow::adapters::orc::WriterVersion::kOrc101:    return "ORC-101";
        case arrow::adapters::orc::WriterVersion::kOrc135:    return "ORC-135";
        case arrow::adapters::orc::WriterVersion::kOrc517:    return "ORC-517";
        case arrow::adapters::orc::WriterVersion::kOrc203:    return "ORC-203";
        case arrow::adapters::orc::WriterVersion::kOrc14:     return "ORC-14";
        case arrow::adapters::orc::WriterVersion::kMax:       break;
    }

    return "unknown";
}

// NOTE: ORC's own codec names differ from arrow's enum -- what ORC calls ZLIB
// arrives here as GZIP. The ORC name is reported, since that is what the
// writer was asked for and what other ORC tooling shows.
std::string codec_name( arrow::Compression::type codec )
{
    switch ( codec )
    {
        case arrow::Compression::UNCOMPRESSED: return "none";
        case arrow::Compression::SNAPPY:       return "snappy";
        case arrow::Compression::GZIP:         return "zlib";
        case arrow::Compression::LZ4:          return "lz4";
        case arrow::Compression::ZSTD:         return "zstd";
        case arrow::Compression::BROTLI:       return "brotli";
        case arrow::Compression::LZO:          return "lzo";
        case arrow::Compression::BZ2:          return "bzip2";
        default:                               break;
    }

    return "unknown";
}

} // anonymous namespace

//
orc_reader::orc_reader() : codec_( "unknown" ),
                           created_( "unknown" ),
                           stripes_( 0 )
{
}

//
orc_reader::orc_reader( const char* file ) : codec_( "unknown" ),
                                             created_( "unknown" ),
                                             stripes_( 0 )
{
    open( file );
}

//
orc_reader::orc_reader( std::string file ) : codec_( "unknown" ),
                                             created_( "unknown" ),
                                             stripes_( 0 )
{
    open( file.c_str() );
}

//
orc_reader::~orc_reader()
{
}

//
std::string orc_reader::format() const
{
    return "orc";
}

//
void orc_reader::open( const char* file )
{
    //
    if ( is_open() )
        throw reader::exception( ALREADY_OPEN, "Already open" );

    //
    if ( file == nullptr )
        throw reader::exception( MISSING_FILE, "Invalid file name!" );

    try
    {
        //
        auto in = arrow::io::ReadableFile::Open( file, arrow::default_memory_pool() );

        if ( ! in.ok() )
            throw reader::exception( MISSING_FILE, in.status().ToString() );

        //
        auto rdr = arrow::adapters::orc::ORCFileReader::Open( *in,
                                                              arrow::default_memory_pool() );

        if ( ! rdr.ok() )
            throw reader::exception( CORRUPTED_FILE, rdr.status().ToString() );

        //
        auto tbl = ( *rdr )->Read();

        if ( ! tbl.ok() )
            throw reader::exception( CORRUPTED_FILE, tbl.status().ToString() );

        table_ = *tbl;

        if ( table_ == nullptr )
            throw reader::exception( CRITICAL_ERROR, "Could not build table" );

        filename_ = file;

        // read the metadata while the reader is still alive -- see the note in
        // the header about why it is not kept
        stripes_ = static_cast<size_t>( ( *rdr )->NumberOfStripes() );

        created_ = writer_name( ( *rdr )->GetWriterId(), ( *rdr )->GetWriterIdValue() )
                 + " (" + writer_version( ( *rdr )->GetWriterVersion() ) + ")";

        auto codec = ( *rdr )->GetCompression();

        if ( codec.ok() )
            codec_ = codec_name( *codec );
    }
    catch ( reader::exception& )
    {
        throw;
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
void orc_reader::close()
{
    //
    codec_ = "unknown";
    created_ = "unknown";
    stripes_ = 0;

    //
    reader::close();
}

//
size_t orc_reader::num_row_groups() const
{
    return stripes_;
}

//
std::string orc_reader::created_by() const
{
    return created_;
}

//
std::string orc_reader::compression_type( reader::Index col ) const
{
    ( void ) col;

    return codec_.empty() ? std::string( "none" ) : codec_;
}

}} // namespace mti::parq
