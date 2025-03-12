#include "ProWaveDAQ.h"
#include "CSVWriter.h"
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <string>
#include <termios.h>                 // Include for terminal input settings
#include <unistd.h>                  // Include for POSIX API (UNIX system calls)
#include <fcntl.h>                   // Include for file control options (e.g., non-blocking mode)
#include <filesystem>
#include <atomic>
#include "INIReader.h"

using namespace std;
namespace fs = filesystem;

// Global variable to store the original terminal settings
struct termios original_tty; 

// Set terminal to non-blocking mode
void setNonBlockingMode() {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);  // Get the current terminal attributes
    original_tty = tty;             // Store original settings for later restoration

    tty.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode (line buffering) and input echo
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK); // Enable non-blocking mode
}

// Restore original terminal settings
void resetTerminalMode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tty); // Restore original settings

    // Remove the O_NONBLOCK flag to return to blocking mode
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}

// Get current time as a formatted string (YYYYMMDDHHMMSS)
string getCurrentTime() {
    auto now = chrono::system_clock::now();
    time_t now_time = chrono::system_clock::to_time_t(now);
    struct tm localTime;
    localtime_r(&now_time, &localTime); // Convert to local time

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", &localTime);
    return string(buffer);
}

int main() {
    ProWaveDAQ daq;

    while (true) {
        system("clear");

        // Load configuration file for setting parameters
        const string iniFilePath = "API/Master.ini";
        INIReader reader(iniFilePath);

        if (reader.ParseError() < 0) {
            cerr << "Cannot load INI file: " << iniFilePath << endl;
            return 1;
        }

        // Read the "SaveUnit" setting (time interval in seconds)
        const string targetSection = "SaveUnit";
        const string targetKey = "second";
        int SaveUnit = reader.GetInteger(targetSection, targetKey, 60);
        cout << "[" << targetSection << "] " << targetKey << " = " << SaveUnit << endl;

        daq.initDevices("API/ProWaveDAQ.ini");

        int ProWaveDAQSampleRate = daq.getSampleRate();
        cout << "ProWaveDAQ Sample Rate: " << ProWaveDAQSampleRate << " Hz" << endl;

        // * 3 channels
        int targetSize = SaveUnit * ProWaveDAQSampleRate * 3;
        int prevCounter = 0;

        daq.startReading();
        usleep(200000);

        system("clear"); // Clear terminal screen for better readability
        cout << "============================== Label Creation ============================" << endl;
        string label;
        cout << "Please enter the label of the data (type 'exit' to exit): ";
        cin >> label;
        string folder = getCurrentTime() + "_" + label;
        if (label == "exit") {
            break;
        }

        fs::create_directory("output/ProWaveDAQ/" + folder);

        // **Initialize CSVWriter**
        CSVWriter csvWriter(3, "output/ProWaveDAQ/" + folder, label);

        char ch;
        setNonBlockingMode();
        bool isRunning = true;
        int dataSize = 0;

        while (isRunning) {
            int currentCounter = daq.getCounter();

            if (read(STDIN_FILENO, &ch, 1) > 0) {
                if (ch == 'Q' || ch == 'q') {
                    isRunning = false;
                    cout << "Saving final data before exit..." << endl;
                    resetTerminalMode(); // Restore terminal settings before exiting
                    break;
                }
                cout << "You pressed: " << ch << endl;
            }

            // **Only process new data when the counter changes**
            if (currentCounter > prevCounter) {
                vector<double> data = daq.getData();
                dataSize += data.size();

                if (dataSize < targetSize) {
                    csvWriter.addDataBlock(move(data));
                } else {
                    int dataActualSize = data.size();  // **Prevent misuse of dataSize**
                    int emptySpace = targetSize - (dataSize - dataActualSize);

                    // **If dataSize > targetSize, split data into batches**
                    while (dataSize >= targetSize) {
                        vector<double> batch(data.begin(), data.begin() + emptySpace);
                        csvWriter.addDataBlock(move(batch));

                        // **Update filename after each full batch**
                        csvWriter.updateFilename();
                        cout << "CSV Saved & Filename Updated" << endl;

                        dataSize -= targetSize;
                    }

                    int pending = dataActualSize - emptySpace;

                    // **Handle remaining data that is less than targetSize**
                    if (pending) {
                        vector<double> remainingData(data.begin() + emptySpace, data.end());
                        csvWriter.addDataBlock(move(remainingData));
                        dataSize = pending;
                    } else {
                        dataSize = 0;
                    }
                }
                prevCounter += 1;
            }
        }

        resetTerminalMode();
        daq.stopReading();
    }

    daq.stopReading();
    return 0;
}
