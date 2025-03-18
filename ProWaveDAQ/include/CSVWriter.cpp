#include "CSVWriter.h"

// Constructor: Initializes the CSVWriter and generates the initial CSV filename.
CSVWriter::CSVWriter(int numChannels, const string& outputDir, const string& label)
    : numChannels(numChannels), outputDir(outputDir), label(label) {
    currentFilename = generateFilename(); // Generate the first filename

    // Create the "output" directory if it does not exist
    if (!fs::exists("output")) {
        cout << "Creating output directory: " << "output" << endl;
        fs::create_directories("output");
    }

    // Create the specified output directory if it does not exist
    if (!fs::exists(outputDir)) {
        cout << "Creating output directory: " << outputDir << endl;
        fs::create_directories(outputDir);
    }
}

// Writes a block of data to the CSV file.
void CSVWriter::addDataBlock(vector<double>&& dataBlock) {
    lock_guard<mutex> lock(fileMutex); // Ensure thread safety
    ofstream file(currentFilename, ios::app); // Open file in append mode

    // Write data in rows, separating values with commas
    for (size_t i = 0; i < dataBlock.size(); i += numChannels) {
        for (int j = 0; j < numChannels; ++j) {
            file << dataBlock[i + j];
            if (j < numChannels - 1) {
                file << ",";
            }
        }
        file << "\n";
    }
    file.close();
}

// Updates the filename when a new save unit is triggered.
void CSVWriter::updateFilename() {
    lock_guard<mutex> lock(fileMutex); // Ensure thread safety
    currentFilename = generateFilename();
}

// Generates a new CSV filename based on the current timestamp.
#include <chrono>
#include <iomanip>
#include <sstream>

string CSVWriter::generateFilename() {
    auto now = chrono::system_clock::now();
    time_t now_time = chrono::system_clock::to_time_t(now);
    tm local_time;

#ifdef _WIN32
    localtime_s(&local_time, &now_time); // Windows-specific function
#else
    localtime_r(&now_time, &local_time); // POSIX function
#endif
    // Construct the filename with timestamp and label
    ostringstream oss;
    oss << outputDir << "/"
        << put_time(&local_time, "%Y%m%d%H%M%S")  // Year-Month-Day Hour-Minute-Second
        << "_" << label << ".csv";

    return oss.str();
}
