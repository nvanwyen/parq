//
// reader_parquet.cpp
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
#include "reader_parquet.hpp"

//
namespace mti { namespace parq {

//
parquet_reader::parquet_reader()
{
}

//
parquet_reader::parquet_reader( const char* file )
{
    open( file );
}

//
parquet_reader::parquet_reader( std::string file )
{
    open( file.c_str() );
}

//
parquet_reader::~parquet_reader()
{
}

//
std::string parquet_reader::format() const
{
    return "parquet";
}

//
void parquet_reader::open( const char* file )
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
#if ARROW_VERSION_MAJOR >= 24
                PARQUET_ASSIGN_OR_THROW( table_, read_->ReadTable() );
#else
                PARQUET_THROW_NOT_OK( read_->ReadTable( &table_ ) );
#endif

                // Store filename for later use
                filename_ = file;
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
void parquet_reader::close()
{
    //
    read_.reset();

    //
    reader::close();
}

//
std::string parquet_reader::compression_type( reader::Index col ) const
{
    std::string compression = "none";

    try {
        if (read_ != nullptr) {
            // Get the parquet file reader
            auto parquet_reader = read_->parquet_reader();
            if (parquet_reader != nullptr) {
                // Get file metadata
                auto file_metadata = parquet_reader->metadata();
                // NOTE: the bound here is the column count. Comparing col against
                // num_row_groups() -- as this once did -- reported "none" for every
                // column whose index happened to exceed the row group count.
                if (file_metadata != nullptr && file_metadata->num_row_groups() > 0
                                             && col < static_cast<Index>(file_metadata->num_columns())) {
                    // Get the first row group to check compression
                    auto row_group = file_metadata->RowGroup(0);
                    if (row_group != nullptr && col < static_cast<Index>(row_group->num_columns())) {
                        auto column_chunk = row_group->ColumnChunk(col);
                        if (column_chunk != nullptr) {
                            // Get compression codec
                            switch (column_chunk->compression()) {
                                case parquet::Compression::UNCOMPRESSED:
                                    compression = "none";
                                    break;
                                case parquet::Compression::SNAPPY:
                                    compression = "snappy";
                                    break;
                                case parquet::Compression::GZIP:
                                    compression = "gzip";
                                    break;
                                case parquet::Compression::LZO:
                                    compression = "lzo";
                                    break;
                                case parquet::Compression::BROTLI:
                                    compression = "brotli";
                                    break;
                                case parquet::Compression::LZ4:
                                    compression = "lz4";
                                    break;
                                case parquet::Compression::ZSTD:
                                    compression = "zstd";
                                    break;
                                default:
                                    compression = "unknown";
                                    break;
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
        // If we can't determine compression, return "unknown"
        compression = "unknown";
    }

    return compression;
}

//
size_t parquet_reader::num_row_groups() const
{
    size_t count = 0;

    try {
        if (read_ != nullptr) {
            auto parquet_reader = read_->parquet_reader();
            if (parquet_reader != nullptr) {
                auto file_metadata = parquet_reader->metadata();
                if (file_metadata != nullptr) {
                    count = file_metadata->num_row_groups();
                }
            }
        }
    } catch (...) {
        // Return 0 if we can't determine
    }

    return count;
}

//
std::string parquet_reader::created_by() const
{
    std::string created_by = "unknown";

    try {
        if (read_ != nullptr) {
            auto parquet_reader = read_->parquet_reader();
            if (parquet_reader != nullptr) {
                auto file_metadata = parquet_reader->metadata();
                if (file_metadata != nullptr) {
                    created_by = file_metadata->created_by();
                }
            }
        }
    } catch (...) {
        // Return "unknown" if we can't determine
    }

    return created_by;
}

}} // namespace mti::parq
