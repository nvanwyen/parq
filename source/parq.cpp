//
// parq.cpp
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

#include <iostream>
#include <iomanip>
#include <string>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <sstream>
#include "reader.hpp"
#include "format.hpp"

enum class OutputFormat
{
    TABULAR,
    JSON,
    CSV,
    XML
};

void print_usage(const char* program_name)
{
    std::cout << "Usage: " << program_name << " [options] <file> [file2] [file3] ...\n";
    std::cout << "Options:\n";
    std::cout << "  -p, --parquet            Input files are Parquet (default)\n";
    std::cout << "  -a, --avro               Input files are Avro\n";
    std::cout << "  -t, --tabular            Output in tabular format (default)\n";
    std::cout << "  -j, --json               Output in JSON format\n";
    std::cout << "  -c, --csv                Output in CSV format\n";
    std::cout << "  -x, --xml                Output in XML format\n";
    std::cout << "  -l, --limit <n>          Limit output to first n rows\n";
    std::cout << "  -m, --metadata           Show only column metadata information\n";
    std::cout << "  -C, --columns <list>     Show only specified columns (comma-delimited)\n";
    std::cout << "      --case <upper|lower> Set output case (default: lower)\n";
    std::cout << "  -h, --help               Show this help message\n";
    std::cout << "\nNote: Output format options are mutually exclusive.\n";
    std::cout << "      Input format options are mutually exclusive.\n";
    std::cout << "Multiple files can be processed in sequence.\n";
}

std::string escape_xml(const std::string& data)
{
    std::string buffer;
    buffer.reserve(data.size());
    
    for (size_t pos = 0; pos != data.size(); ++pos)
    {
        switch (data[pos])
        {
            case '&':  buffer.append("&amp;");  break;
            case '\"': buffer.append("&quot;"); break;
            case '\'': buffer.append("&apos;"); break;
            case '<':  buffer.append("&lt;");   break;
            case '>':  buffer.append("&gt;");   break;
            default:   buffer.append(&data[pos], 1); break;
        }
    }
    return buffer;
}

std::vector<std::string> parse_column_list(const std::string& column_list)
{
    std::vector<std::string> columns;
    std::string current_column;
    
    for (char c : column_list)
    {
        if (c == ',')
        {
            if (!current_column.empty())
            {
                // Trim whitespace
                size_t start = current_column.find_first_not_of(" \t");
                size_t end = current_column.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos)
                    columns.push_back(current_column.substr(start, end - start + 1));
                current_column.clear();
            }
        }
        else
            current_column += c;
    }
    
    // Add the last column
    if (!current_column.empty())
    {
        size_t start = current_column.find_first_not_of(" \t");
        size_t end = current_column.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos)
            columns.push_back(current_column.substr(start, end - start + 1));
    }
    
    return columns;
}

std::vector<size_t> get_column_indices(const mti::parq::reader& reader, const std::vector<std::string>& column_names)
{
    std::vector<size_t> indices;
    
    for (const std::string& name : column_names)
    {
        bool found = false;
        for (size_t i = 0; i < reader.num_cols(); i++)
        {
            if (reader.name(i) == name)
            {
                indices.push_back(i);
                found = true;
                break;
            }
        }
        if (!found)
            std::cerr << "Warning: Column '" << name << "' not found in file\n";
    }
    
    return indices;
}

bool is_numeric_column(const mti::parq::reader& reader, size_t column_index)
{
    auto field = reader.field(column_index);
    if (!field)
        return false;
        
    switch (field->type()->id())
    {
        case arrow::Type::type::UINT8:
        case arrow::Type::type::INT8:
        case arrow::Type::type::UINT16:
        case arrow::Type::type::INT16:
        case arrow::Type::type::UINT32:
        case arrow::Type::type::INT32:
        case arrow::Type::type::UINT64:
        case arrow::Type::type::INT64:
        case arrow::Type::type::FLOAT:
        case arrow::Type::type::DOUBLE:
        case arrow::Type::type::DECIMAL:
        case arrow::Type::type::DECIMAL256:
            return true;
        default:
            return false;
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    std::vector<std::string> filenames;
    OutputFormat output_format = OutputFormat::TABULAR;
    int format_count = 0;
    mti::parq::input_format in_format = mti::parq::input_format::PARQUET;
    int in_format_count = 0;
    bool metadata_only = false;
    std::vector<std::string> selected_columns;
    size_t limit = 0;
    bool has_limit = false;
    std::string case_option = "lower";

    // Parse arguments
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        
        // Handle long and short options
        if (arg == "--tabular" || arg == "-t")
        {
            output_format = OutputFormat::TABULAR;
            format_count++;
        }
        else if (arg == "--json" || arg == "-j")
        {
            output_format = OutputFormat::JSON;
            format_count++;
        }
        else if (arg == "--csv" || arg == "-c")
        {
            output_format = OutputFormat::CSV;
            format_count++;
        }
        else if (arg == "--xml" || arg == "-x")
        {
            output_format = OutputFormat::XML;
            format_count++;
        }
        else if (arg == "--parquet" || arg == "-p")
        {
            in_format = mti::parq::input_format::PARQUET;
            in_format_count++;
        }
        else if (arg == "--avro" || arg == "-a")
        {
            in_format = mti::parq::input_format::AVRO;
            in_format_count++;
        }
        else if (arg == "--metadata" || arg == "-m")
            metadata_only = true;
        else if ((arg == "--columns" || arg == "-C") && i + 1 < argc)
            selected_columns = parse_column_list(argv[++i]);
        else if ((arg == "--limit" || arg == "-l") && i + 1 < argc)
        {
            limit = std::atoi(argv[++i]);
            has_limit = true;
        }
        else if (arg == "--case" && i + 1 < argc)
            case_option = argv[++i];
        else if (arg == "--help" || arg == "-h")
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg[0] != '-')
            filenames.push_back(arg);  // This is a filename
        else
        {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Check if we have any files to process
    if (filenames.empty()) {
        std::cerr << "Error: No input files specified.\n";
        print_usage(argv[0]);
        return 1;
    }

    if (format_count > 1) {
        std::cerr << "Error: Output format options are mutually exclusive.\n";
        std::cerr << "Please specify only one output format.\n";
        return 1;
    }

    if (in_format_count > 1) {
        std::cerr << "Error: Input format options are mutually exclusive.\n";
        std::cerr << "Please specify only one of --parquet or --avro.\n";
        return 1;
    }

    if (!mti::parq::supported(in_format)) {
        std::cerr << "Error: This build has no " << mti::parq::to_string(in_format)
                  << " support.\n";
        std::cerr << "Rebuild with -DWITH_AVRO=ON (requires the avro-cpp library).\n";
        return 1;
    }

    // Process each file
    for (size_t file_idx = 0; file_idx < filenames.size(); file_idx++) {
        const std::string& filename = filenames[file_idx];
        
        // Add separator between files if processing multiple files
        if (file_idx > 0) {
            std::cout << "\n" << std::string(80, '=') << "\n\n";
        }
        
        try {
            mti::parq::reader_ptr reader_ref = mti::parq::make_reader(in_format);
            mti::parq::reader& parquet_reader = *reader_ref;
            
            try {
                parquet_reader.open(filename);
            } catch (...) {
                std::cerr << "Error: Failed to open " << mti::parq::to_string(in_format)
                          << " file: " << filename << std::endl;
                if (filenames.size() == 1) {
                    return 1;  // Exit with error for single file
                } else {
                    continue;  // Continue with next file for multiple files
                }
            }
            
            parquet_reader.set_case(case_option);
            
            std::cout << "File: " << filename << "\n";
            
            try {
                std::cout << "Rows: " << parquet_reader.num_rows() << ", Columns: " << parquet_reader.num_cols() << "\n";
            } catch (...) {
                std::cerr << "Error: Failed to read file metadata from: " << filename << std::endl;
                if (filenames.size() == 1) {
                    return 1;  // Exit with error for single file
                } else {
                    continue;  // Continue with next file for multiple files
                }
            }
        
        if (metadata_only) {
            std::cout << "\nFile Information:\n\n";
            
            // Calculate column widths for file info table
            size_t subject_width = 15;  // for subject column
            size_t data_width = 20;     // start with minimum for data column
            
            // Pre-calculate all values to determine maximum width
            std::string row_groups_str = std::to_string(parquet_reader.num_row_groups());
            std::string created_by_str = parquet_reader.created_by();
            std::string file_size_str;
            std::string checksum_str = parquet_reader.file_checksum();
            
            int64_t file_size = parquet_reader.file_size();
            if (file_size > 0) {
                std::stringstream size_str;
                if (file_size < 1024) {
                    size_str << file_size << " bytes";
                } else if (file_size < 1024 * 1024) {
                    size_str << std::fixed << std::setprecision(2) 
                             << (file_size / 1024.0) << " KB";
                } else if (file_size < 1024 * 1024 * 1024) {
                    size_str << std::fixed << std::setprecision(2) 
                             << (file_size / (1024.0 * 1024.0)) << " MB";
                } else {
                    size_str << std::fixed << std::setprecision(2) 
                             << (file_size / (1024.0 * 1024.0 * 1024.0)) << " GB";
                }
                file_size_str = size_str.str();
            } else {
                file_size_str = "unavailable";
            }
            
            // Find maximum width needed
            data_width = std::max(data_width, row_groups_str.length());
            data_width = std::max(data_width, created_by_str.length());
            data_width = std::max(data_width, file_size_str.length());
            data_width = std::max(data_width, checksum_str.length());
            data_width = std::max(data_width, std::string("Value").length());  // header
            
            // Add some padding
            data_width += 2;
            
            // Print header
            std::cout << "  " << std::left 
                      << std::setw(subject_width) << "Property" << " | "
                      << std::setw(data_width) << "Value" << " |\n";
                      
            // Print separator line
            std::cout << "  " << std::string(subject_width, '-') << "-+-"
                      << std::string(data_width, '-') << "-+\n";
            
            // Row Groups
            std::cout << "  " << std::left
                      << std::setw(subject_width) << "Row Groups" << " | "
                      << std::setw(data_width) << row_groups_str << " |\n";
            
            // Created By
            std::cout << "  " << std::left
                      << std::setw(subject_width) << "Created By" << " | "
                      << std::setw(data_width) << created_by_str << " |\n";
            
            // File Size
            std::cout << "  " << std::left
                      << std::setw(subject_width) << "File Size" << " | "
                      << std::setw(data_width) << file_size_str << " |\n";
            
            // File Checksum
            std::cout << "  " << std::left
                      << std::setw(subject_width) << "File Checksum" << " | "
                      << std::setw(data_width) << checksum_str << " |\n";
        }
        
        std::cout << "\n";

        if (metadata_only) {
            std::cout << "Column Information:\n\n";
            
            // Calculate column widths for nice formatting
            size_t idx_width = 5;  // for index
            size_t name_width = 20; // start with minimum
            size_t type_width = 15; // start with minimum
            size_t comp_width = 12; // for compression
            
            // Find maximum widths
            for (size_t i = 0; i < parquet_reader.num_cols(); i++) {
                name_width = std::max(name_width, parquet_reader.name(i).length());
                auto field = parquet_reader.field(i);
                type_width = std::max(type_width, mti::parq::reader::to_type(field->type()).length());
            }
            
            // Add some padding
            name_width += 2;
            type_width += 2;
            
            // Print header
            std::cout << "  " << std::right << std::setw(idx_width) << "Index" << " | "
                      << std::left << std::setw(name_width) << "Column Name" << " | "
                      << std::setw(type_width) << "Data Type" << " | "
                      << std::setw(comp_width) << "Compression" << " |\n";
                      
            // Print separator line
            std::cout << "  " << std::string(idx_width, '-') << "-+-"
                      << std::string(name_width, '-') << "-+-"
                      << std::string(type_width, '-') << "-+-"
                      << std::string(comp_width, '-') << "-+\n";
            
            // Print column information
            for (size_t i = 0; i < parquet_reader.num_cols(); i++) {
                auto field = parquet_reader.field(i);
                std::cout << "  " << std::right << std::setw(idx_width) << i << " | "
                          << std::left << std::setw(name_width) << parquet_reader.name(i) << " | "
                          << std::setw(type_width) << mti::parq::reader::to_type(field->type()) << " | "
                          << std::setw(comp_width) << parquet_reader.compression_type(i) << " |\n";
            }
            
            continue;  // Move to next file instead of exiting
        }

        // Get column indices if specific columns are requested
        std::vector<size_t> column_indices;
        if (!selected_columns.empty()) {
            column_indices = get_column_indices(parquet_reader, selected_columns);
            if (column_indices.empty()) {
                std::cerr << "Error: No valid columns found.\n";
                if (filenames.size() == 1) {
                    return 1;  // Exit with error for single file
                } else {
                    continue;  // Continue with next file for multiple files
                }
            }
        } else {
            // Use all columns
            for (size_t i = 0; i < parquet_reader.num_cols(); i++) {
                column_indices.push_back(i);
            }
        }

        size_t rows_to_process = parquet_reader.num_rows();
        if (has_limit && limit < rows_to_process) {
            rows_to_process = limit;
        }

        switch (output_format) {
            case OutputFormat::TABULAR:
            {
                std::vector<size_t> col_widths(column_indices.size());
                std::vector<bool> is_numeric(column_indices.size());
                
                // Determine column types and initial widths
                for (size_t i = 0; i < column_indices.size(); i++)
                {
                    col_widths[i] = parquet_reader.name(column_indices[i]).length();
                    is_numeric[i] = is_numeric_column(parquet_reader, column_indices[i]);
                }
                
                // Calculate maximum widths needed
                for (size_t row = 0; row < rows_to_process; row++)
                {
                    for (size_t i = 0; i < column_indices.size(); i++)
                    {
                        size_t len = parquet_reader.value(column_indices[i], row).length();
                        if (len > col_widths[i])
                            col_widths[i] = len;
                    }
                }
                
                auto print_separator = [&]()
                {
                    std::cout << "+";
                    for (size_t i = 0; i < column_indices.size(); i++)
                        std::cout << std::string(col_widths[i] + 2, '-') << "+";
                    std::cout << "\n";
                };
                
                // Print header row
                print_separator();
                std::cout << "|";
                for (size_t i = 0; i < column_indices.size(); i++)
                {
                    if (is_numeric[i])
                        std::cout << " " << std::right << std::setw(col_widths[i]) 
                                  << parquet_reader.name(column_indices[i]) << " |";
                    else
                        std::cout << " " << std::left << std::setw(col_widths[i]) 
                                  << parquet_reader.name(column_indices[i]) << " |";
                }
                std::cout << "\n";
                print_separator();
                
                // Print data rows
                for (size_t row = 0; row < rows_to_process; row++)
                {
                    std::cout << "|";
                    for (size_t i = 0; i < column_indices.size(); i++)
                    {
                        if (is_numeric[i])
                            std::cout << " " << std::right << std::setw(col_widths[i]) 
                                      << parquet_reader.value(column_indices[i], row) << " |";
                        else
                            std::cout << " " << std::left << std::setw(col_widths[i]) 
                                      << parquet_reader.value(column_indices[i], row) << " |";
                    }
                    std::cout << "\n";
                }
                print_separator();
                break;
            }
            
            case OutputFormat::JSON: {
                std::cout << "[\n";
                for (size_t row = 0; row < rows_to_process; row++) {
                    std::cout << "  {\n";
                    for (size_t i = 0; i < column_indices.size(); i++) {
                        if (i > 0) std::cout << ",\n";
                        std::cout << "    \"" << parquet_reader.name(column_indices[i]) << "\": \"" 
                                  << parquet_reader.value(column_indices[i], row) << "\"";
                    }
                    std::cout << "\n  }";
                    if (row < rows_to_process - 1) {
                        std::cout << ",";
                    }
                    std::cout << "\n";
                }
                std::cout << "]\n";
                break;
            }
            
            case OutputFormat::CSV: {
                for (size_t i = 0; i < column_indices.size(); i++) {
                    if (i > 0) std::cout << ",";
                    std::cout << "\"" << parquet_reader.name(column_indices[i]) << "\"";
                }
                std::cout << "\n";

                for (size_t row = 0; row < rows_to_process; row++) {
                    for (size_t i = 0; i < column_indices.size(); i++) {
                        if (i > 0) std::cout << ",";
                        std::string val = parquet_reader.value(column_indices[i], row);
                        if (val.find_first_of(",\"\n\r") != std::string::npos) {
                            std::cout << "\"";
                            for (char c : val) {
                                if (c == '"') std::cout << "\"\"";
                                else std::cout << c;
                            }
                            std::cout << "\"";
                        } else {
                            std::cout << val;
                        }
                    }
                    std::cout << "\n";
                }
                break;
            }
            
            case OutputFormat::XML: {
                std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
                std::cout << "<parquet-data>\n";
                std::cout << "  <metadata>\n";
                std::cout << "    <file>" << escape_xml(filename) << "</file>\n";
                std::cout << "    <rows>" << parquet_reader.num_rows() << "</rows>\n";
                std::cout << "    <columns>" << parquet_reader.num_cols() << "</columns>\n";
                std::cout << "  </metadata>\n";
                std::cout << "  <records>\n";
                
                for (size_t row = 0; row < rows_to_process; row++) {
                    std::cout << "    <record row=\"" << row << "\">\n";
                    for (size_t i = 0; i < column_indices.size(); i++) {
                        std::string name = parquet_reader.name(column_indices[i]);
                        std::string value = parquet_reader.value(column_indices[i], row);
                        std::cout << "      <" << name << ">" 
                                  << escape_xml(value) 
                                  << "</" << name << ">\n";
                    }
                    std::cout << "    </record>\n";
                }
                
                std::cout << "  </records>\n";
                std::cout << "</parquet-data>\n";
                break;
            }
        }

        } catch (mti::parq::reader::exception& e) {
            std::cerr << "Error: " << e.what() << " (code: " << e.code() << ")\n";
            if (filenames.size() == 1) {
                return 1;  // Exit with error for single file
            } else {
                continue;  // Continue with next file for multiple files
            }
        } catch (std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            if (filenames.size() == 1) {
                return 1;  // Exit with error for single file
            } else {
                continue;  // Continue with next file for multiple files
            }
        }
    }

    return 0;
}
