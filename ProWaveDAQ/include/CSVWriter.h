#ifndef CSV_WRITER_H
#define CSV_WRITER_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <filesystem>

using namespace std;
namespace fs = filesystem;

// CSVWriter class handles writing data to CSV files in a thread-safe manner.
class CSVWriter {
public:
    // Constructor: Initializes CSVWriter with the number of channels, output directory, and label.
    CSVWriter(int numChannels, const string& outputDir, const string& label);
    
    // Writes a block of data to the current CSV file.
    void addDataBlock(vector<double>&& dataBlock);
    
    // Updates the filename when a new save unit is triggered.
    void updateFilename();

private:
    int numChannels;         // Number of data channels
    string outputDir;        // Directory where CSV files will be stored
    string label;            // Label used in the filename
    string currentFilename;  // Current CSV filename
    mutex fileMutex;         // Mutex for thread safety

    // Generates a new CSV filename based on the current timestamp.
    string generateFilename();
};

#endif // CSV_WRITER_H
