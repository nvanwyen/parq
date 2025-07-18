#include <iostream>
#include <iomanip>
#include <string>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include "reader.hpp"

enum class OutputFormat {
    TABULAR,
    JSON,
    CSV,
    XML
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <parquet_file> [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --tabular            Output in tabular format (default)\n";
    std::cout << "  --json               Output in JSON format\n";
    std::cout << "  --csv                Output in CSV format\n";
    std::cout << "  --xml                Output in XML format\n";
    std::cout << "  --limit <n>          Limit output to first n rows\n";
    std::cout << "  --metadata           Show only column metadata information\n";
    std::cout << "  --columns <list>     Show only specified columns (comma-delimited)\n";
    std::cout << "  --case <upper|lower> Set output case (default: lower)\n";
    std::cout << "  --help               Show this help message\n";
    std::cout << "\nNote: Output format options (--tabular, --json, --csv, --xml) are mutually exclusive.\n";
}

std::string escape_xml(const std::string& data) {
    std::string buffer;
    buffer.reserve(data.size());
    for(size_t pos = 0; pos != data.size(); ++pos) {
        switch(data[pos]) {
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

std::vector<std::string> parse_column_list(const std::string& column_list) {
    std::vector<std::string> columns;
    std::string current_column;
    
    for (char c : column_list) {
        if (c == ',') {
            if (!current_column.empty()) {
                // Trim whitespace
                size_t start = current_column.find_first_not_of(" \t");
                size_t end = current_column.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos) {
                    columns.push_back(current_column.substr(start, end - start + 1));
                }
                current_column.clear();
            }
        } else {
            current_column += c;
        }
    }
    
    // Add the last column
    if (!current_column.empty()) {
        size_t start = current_column.find_first_not_of(" \t");
        size_t end = current_column.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            columns.push_back(current_column.substr(start, end - start + 1));
        }
    }
    
    return columns;
}

std::vector<size_t> get_column_indices(const mti::parq::reader& reader, const std::vector<std::string>& column_names) {
    std::vector<size_t> indices;
    
    for (const std::string& name : column_names) {
        bool found = false;
        for (size_t i = 0; i < reader.num_cols(); i++) {
            if (reader.name(i) == name) {
                indices.push_back(i);
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "Warning: Column '" << name << "' not found in parquet file\n";
        }
    }
    
    return indices;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string filename = argv[1];
    OutputFormat output_format = OutputFormat::TABULAR;
    int format_count = 0;
    bool metadata_only = false;
    std::vector<std::string> selected_columns;
    size_t limit = 0;
    bool has_limit = false;
    std::string case_option = "lower";

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--tabular") {
            output_format = OutputFormat::TABULAR;
            format_count++;
        } else if (arg == "--json") {
            output_format = OutputFormat::JSON;
            format_count++;
        } else if (arg == "--csv") {
            output_format = OutputFormat::CSV;
            format_count++;
        } else if (arg == "--xml") {
            output_format = OutputFormat::XML;
            format_count++;
        } else if (arg == "--metadata") {
            metadata_only = true;
        } else if (arg == "--columns" && i + 1 < argc) {
            selected_columns = parse_column_list(argv[++i]);
        } else if (arg == "--limit" && i + 1 < argc) {
            limit = std::atoi(argv[++i]);
            has_limit = true;
        } else if (arg == "--case" && i + 1 < argc) {
            case_option = argv[++i];
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (format_count > 1) {
        std::cerr << "Error: Output format options (--tabular, --json, --csv, --xml) are mutually exclusive.\n";
        std::cerr << "Please specify only one output format.\n";
        return 1;
    }

    try {
        mti::parq::reader parquet_reader(filename);
        
        parquet_reader.set_case(case_option);
        
        std::cout << "File: " << filename << "\n";
        std::cout << "Rows: " << parquet_reader.num_rows() << "\n";
        std::cout << "Columns: " << parquet_reader.num_cols() << "\n\n";

        if (metadata_only) {
            std::cout << "Column Information:\n";
            for (size_t i = 0; i < parquet_reader.num_cols(); i++) {
                auto field = parquet_reader.field(i);
                std::cout << "  [" << i << "] " 
                          << parquet_reader.name(i) << " ("
                          << mti::parq::reader::to_type(field->type()) << ")\n";
            }
            return 0;
        }

        // Get column indices if specific columns are requested
        std::vector<size_t> column_indices;
        if (!selected_columns.empty()) {
            column_indices = get_column_indices(parquet_reader, selected_columns);
            if (column_indices.empty()) {
                std::cerr << "Error: No valid columns found.\n";
                return 1;
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
            case OutputFormat::TABULAR: {
                std::vector<size_t> col_widths(column_indices.size());
                
                for (size_t i = 0; i < column_indices.size(); i++) {
                    col_widths[i] = parquet_reader.name(column_indices[i]).length();
                }
                
                for (size_t row = 0; row < rows_to_process; row++) {
                    for (size_t i = 0; i < column_indices.size(); i++) {
                        size_t len = parquet_reader.value(column_indices[i], row).length();
                        if (len > col_widths[i]) {
                            col_widths[i] = len;
                        }
                    }
                }
                
                auto print_separator = [&]() {
                    std::cout << "+";
                    for (size_t i = 0; i < column_indices.size(); i++) {
                        std::cout << std::string(col_widths[i] + 2, '-') << "+";
                    }
                    std::cout << "\n";
                };
                
                print_separator();
                std::cout << "|";
                for (size_t i = 0; i < column_indices.size(); i++) {
                    std::cout << " " << std::left << std::setw(col_widths[i]) 
                              << parquet_reader.name(column_indices[i]) << " |";
                }
                std::cout << "\n";
                print_separator();
                
                for (size_t row = 0; row < rows_to_process; row++) {
                    std::cout << "|";
                    for (size_t i = 0; i < column_indices.size(); i++) {
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
        return 1;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
