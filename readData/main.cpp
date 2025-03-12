#include <iostream>
#include <modbus/modbus.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <vector>
#include <chrono>
#include <iomanip>
#include <fstream>

#define SERIAL_PORT "/dev/ttyUSB0"
#define BAUDRATE 3000000
#define SAMPLE_RATE 7812
#define SLAVE_ID 1

using namespace std;

// Global variable to store the original terminal settings
struct termios original_tty;

void setNonBlockingMode()
{
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty); // Get the current terminal attributes
    original_tty = tty;            // Store the original terminal attributes for restoration later

    tty.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode (line buffering) and echoing of input
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK); // Set non-blocking mode
}

/**
 * @brief Restore terminal attributes to their original state.
 *
 * This function ensures that when the program terminates, the terminal settings
 * return to normal to avoid unexpected behavior in the shell.
 */
void resetTerminalMode()
{
    // Restore original terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tty);

    // Remove the O_NONBLOCK flag to restore blocking mode
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}

int main(void)
{
    modbus_t *ctx = modbus_new_rtu(SERIAL_PORT, BAUDRATE, 'N', 8, 1);
    if (!ctx)
    {
        cerr << "Failed to create Modbus context!" << endl;
        return -1;
    }

    modbus_set_slave(ctx, SLAVE_ID);
    if (modbus_connect(ctx) == -1)
    {
        cerr << "Modbus connection failed!" << endl;
        modbus_free(ctx);
        return -1;
    }

    ofstream csv_file("output.csv", ios::out | ios::app);
    if (!csv_file.is_open())
    {
        cerr << "Failed to open output.csv" << endl;
        return -1;
    }
    csv_file << "Time (ms),Data Length,X,Y,Z" << endl;

    // 讀取 Chip ID
    uint16_t chip_id[3];
    if (modbus_read_input_registers(ctx, 0x80, 3, chip_id) == -1)
    {
        cerr << "Failed to read Chip ID!" << endl;
    }
    else
    {
        cout << "ChipID: " << hex << chip_id[0] << ", " << chip_id[1] << ", " << chip_id[2] << endl;
    }

    // 設定取樣率
    if (modbus_write_register(ctx, 0x01, SAMPLE_RATE) == -1)
    {
        cerr << "Failed to set Sample Rate!" << endl;
    }

    // 讀取振動數據 FIFO 長度
    uint16_t data_len;
    if (modbus_read_input_registers(ctx, 0x02, 1, &data_len) == -1)
    {
        cerr << "Failed to read Data Length!" << endl;
    }
    else
    {
        cout << "Data Length: " << data_len << endl;
    }

    // 連續讀取數據
    int maxSize = 41 * 3;
    uint16_t vib_data[maxSize + 1];
    int counter = 0;

    char ch;
    cout << "Start reading data, press 'Q' or 'q' to terminate the program." << endl;

    while (true)
    {
        auto start_time = chrono::high_resolution_clock::now();

        /*if (read(STDIN_FILENO, &ch, 1) > 0) {
            if (ch == 'Q' || ch == 'q') {
                cout << "Saving final data before exit..." << endl;
                resetTerminalMode(); // Restore terminal settings before exiting
                break;
            }
            cout << "You pressed: " << ch << endl;
        }*/

        if (data_len >= maxSize)
        {
            modbus_read_input_registers(ctx, 0x02, maxSize + 1, vib_data);
        }
        else if (data_len <= 6)
        {
            usleep(1000);
            modbus_read_input_registers(ctx, 0x02, 1, &data_len);
            continue;
        }
        else
        {
            modbus_read_input_registers(ctx, 0x03, data_len + 1, vib_data);
        }

        auto end_time = chrono::high_resolution_clock::now();
        chrono::duration<double, milli> elapsed = end_time - start_time;

        counter++;
            
        cout << fixed << setprecision(6) << setw(10) << noshowpos << elapsed.count() << "ms ";
        cout << "Data Length: "
                << setw(7) << setfill('0') << dec << vib_data[0] << " "
                << "[X]"
                << setw(8) << setfill(' ') << right << showpos << (int16_t)vib_data[1] / 8192.0 << " "
                << "[Y]"
                << setw(8) << setfill(' ') << right << showpos << (int16_t)vib_data[2] / 8192.0 << " "
                << "[Z]"
                << setw(8) << setfill(' ') << right << showpos << (int16_t)vib_data[3] / 8192.0 << endl;

        // 寫入 CSV 檔案
        csv_file << fixed << setprecision(6) << elapsed.count() << ","
                    << vib_data[0] << ","
                    << (int16_t)vib_data[1] / 8192.0 << ","
                    << (int16_t)vib_data[2] / 8192.0 << ","
                    << (int16_t)vib_data[3] / 8192.0 << endl;
    }

    csv_file.close();
    modbus_close(ctx);
    modbus_free(ctx);
    return 0;
}