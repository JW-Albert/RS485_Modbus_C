# 編譯器與參數
CC = g++
CFLAGS = -Wall -std=c++17 -pthread -I./include -I./include/iniReader
LDFLAGS = -lmodbus

# 檔案設定
SRCS = main.cpp include/ProWaveDAQ.cpp include/CSVWriter.cpp \
       include/iniReader/INIReader.cpp include/iniReader/ini.c
OBJS = $(SRCS:.cpp=.o)

# 最終目標執行檔
TARGET = main

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)