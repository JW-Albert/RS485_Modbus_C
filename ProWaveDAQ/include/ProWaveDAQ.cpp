#include "ProWaveDAQ.h"

// **Scan for available Modbus devices**
void ProWaveDAQ::scanDevices() {
    vector<string> devices;
    regex usbPattern("/dev/ttyUSB[0-9]+");

    // **Scan the `/dev/` directory for ttyUSB devices**
    for (const auto& entry : fs::directory_iterator("/dev/")) {
        string path = entry.path().string();
        if (regex_match(path, usbPattern)) {
            devices.push_back(path);
        }
    }

    // **If no devices are found**
    if (devices.empty()) {
        cout << "No Modbus devices found!" << endl;
        return;
    }

    // **Display available devices**
    cout << "Available Modbus devices:" << endl;
    for (size_t i = 0; i < devices.size(); i++) {
        cout << "(" << i + 1 << ") " << devices[i] << endl;
    }
}

// **Callback function for parsing the INI configuration file**
static int handler(void* user, const char* section, const char* name, const char* value) {
    auto* ini_data = reinterpret_cast<std::map<std::string, std::map<std::string, std::string>>*>(user);
    (*ini_data)[section][name] = value;
    return 1;
}

// **Constructor**
ProWaveDAQ::ProWaveDAQ()
    : ctx(nullptr), serialPort("/dev/ttyUSB0"), baudRate(3000000), sampleRate(7812),
    slaveID(1), counter(0), reading(false) {}

// **Destructor**
ProWaveDAQ::~ProWaveDAQ() {
    stopReading();
    if (ctx) {
        modbus_close(ctx);
        modbus_free(ctx);
    }
}

// **Initialize the device from an INI file**
void ProWaveDAQ::initDevices(const char* filename) {
    cout << "Loading settings from INI file..." << endl;

    std::map<std::string, std::map<std::string, std::string>> ini_data;
    if (ini_parse(filename, handler, &ini_data) < 0) {
        cerr << "Error: Unable to load INI file: " << filename << endl;
        return;
    }

    try {
        serialPort = ini_data["ProWaveDAQ"]["serialPort"];
        baudRate = std::stoi(ini_data["ProWaveDAQ"]["baudRate"]);
        sampleRate = std::stoi(ini_data["ProWaveDAQ"]["sampleRate"]);
        slaveID = std::stoi(ini_data["ProWaveDAQ"]["slaveID"]);

        cout << "Loaded settings from INI file:\n"
             << "Serial Port: " << serialPort << "\n"
             << "Baud Rate: " << baudRate << "\n"
             << "Sample Rate: " << sampleRate << "\n"
             << "Slave ID: " << slaveID << endl;
    } catch (const std::exception& e) {
        cerr << "Error parsing INI file: " << e.what() << endl;
        return;
    }

    // **Step 1: Create Modbus connection**
    cout << "Creating Modbus context..." << endl;
    ctx = modbus_new_rtu(serialPort.c_str(), baudRate, 'N', 8, 1);
    if (!ctx) {
        cerr << "Error: Failed to create Modbus context!" << endl;
        return;
    }
    cout << "Modbus context created successfully." << endl;

    // **Step 2: Set Slave ID**
    cout << "Setting Modbus Slave ID..." << endl;
    if (modbus_set_slave(ctx, slaveID) == -1) {
        cerr << "Error: Failed to set Modbus Slave ID: " << slaveID << endl;
        modbus_close(ctx);
        modbus_free(ctx);
        ctx = nullptr;
        return;
    }
    cout << "Modbus Slave ID set successfully." << endl;

    // **Step 3: Connect to Modbus device**
    cout << "Connecting to Modbus device..." << endl;
    if (modbus_connect(ctx) == -1) {
        cerr << "Error: Failed to connect to Modbus device!" << endl;
        modbus_free(ctx);
        ctx = nullptr;
        return;
    }
    cout << "Connected to Modbus device successfully." << endl;

    // Read Chip ID
    uint16_t chip_id[3];
    if (modbus_read_input_registers(ctx, 0x80, 3, chip_id) == -1) {
        cerr << "Failed to read Chip ID!" << endl;
    } else {
        cout << "ChipID: " << hex << chip_id[0] << ", " << chip_id[1] << ", " << chip_id[2] << endl;
    }

    // **Step 4: Set Sample Rate**
    cout << "Setting Sample Rate..." << endl;
    if (modbus_write_register(ctx, 0x01, sampleRate) == -1) {
        cerr << "Error: Failed to set Sample Rate!" << endl;
    } else {
        cout << "Sample Rate set successfully." << endl;
    }
}

// **Start reading vibration data (runs in a background thread)**
void ProWaveDAQ::startReading() {
    if (reading) {
        cerr << "Reading is already running!" << endl;
        return;
    }

    reading = true;
    readingThread = thread(&ProWaveDAQ::readLoop, this);
}

// **Stop reading vibration data**
void ProWaveDAQ::stopReading() {
    if (reading) {
        reading = false;
        if (readingThread.joinable()) {
            readingThread.join();
        }
    }
    modbus_close(ctx);
    modbus_free(ctx);
}

// **Read vibration data (main reading loop)**
void ProWaveDAQ::readLoop() {
    int prev_data_len = 0;
    int maxSize = 41 * 3;
    int saveLen = 0;
    int dropCounter = 0;
    uint16_t vib_data[maxSize + 1];
    modbus_read_input_registers(ctx, 0x02, 1, vib_data);
    saveLen = vib_data[0];
    cout << "Data Length: " << dec << vib_data[0] << endl;

    cout << "Reading loop started..." << endl;
    while (reading) {
        auto start_time = chrono::high_resolution_clock::now();

        if (vib_data[0] >= maxSize) {
            modbus_read_input_registers(ctx, 0x02, maxSize + 1, vib_data);
        } else if (vib_data[0] <= 6) {
            usleep(1000);
            modbus_read_input_registers(ctx, 0x02, 1, vib_data);
            continue;
        } else {
            modbus_read_input_registers(ctx, 0x02, vib_data[0] + 1, vib_data);
        }

        lock_guard<mutex> lock(dataMutex);
        latestData.clear();

        for (int i = 1; i <= saveLen; i++) {
            latestData.push_back(static_cast<double>(static_cast<int16_t>(vib_data[i])) / 8192.0);
        }

        counter++;
        saveLen = vib_data[0];
    }
}

// **Retrieve the latest vibration data**
vector<double> ProWaveDAQ::getData() {
    lock_guard<mutex> lock(dataMutex);
    return latestData;
}

// **Get the number of data reads**
int ProWaveDAQ::getCounter() const {
    return counter;
}

// **Get the sample rate**
int ProWaveDAQ::getSampleRate() const {
    return sampleRate;
}
