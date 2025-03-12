#ifndef PROWAVEDAQ_H
#define PROWAVEDAQ_H

#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <thread>
#include <atomic>
#include <modbus/modbus.h>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <mutex>
#include <regex>
#include <filesystem>

// Include INIReader for configuration parsing
#include "./iniReader/INIReader.h"
extern "C" {
#include "./iniReader/ini.h"
}

using namespace std;
namespace fs = std::filesystem;

class ProWaveDAQ {
public:
    // Constructor & Destructor
    ProWaveDAQ();
    ~ProWaveDAQ();

    // Initializes the device using the specified .ini configuration file.
    void initDevices(const char* filename);

    // Starts reading vibration data.
    void startReading();

    // Stops reading vibration data.
    void stopReading();

    // Retrieves the most recent vibration data.
    vector<double> getData();

    // Returns the current data read count.
    int getCounter() const;

    // Returns the sample rate.
    int getSampleRate() const;

    // Scans for connected devices.
    void scanDevices();

private:
    // Modbus-related variables
    modbus_t* ctx;          // Modbus context
    string serialPort;      // Serial port used for communication
    int baudRate;           // Baud rate for serial communication
    int sampleRate;         // Sampling rate for data acquisition
    int slaveID;            // Modbus slave ID
    atomic<int> counter;    // Counter for data reads
    atomic<bool> reading;   // Flag to indicate if reading is active
    thread readingThread;   // Thread handling data reading

    vector<double> latestData; // Stores the latest acquired data
    mutex dataMutex;           // Mutex to ensure thread safety

    // Internal function for reading data in a loop.
    void readLoop();
};

#endif // PROWAVEDAQ_H
