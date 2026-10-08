#pragma once

#include <chrono>
#include <map>
#include <string>
#include <utility>
#include <vector>

// What kind of hardware a reading belongs to. Used for ordering and icons.
enum class Category { Cpu, Gpu, Memory, Board, Drive, Network, Other };

// One reading of one sensor at one moment, e.g. {"CPU [coretemp]", "temp1_input", "Package", "°C", 54.0}.
struct Reading {
	std::string group;  // device it belongs to (shown as a collapsible parent row)
	std::string key;    // stable unique id within the group (used to track min/max)
	std::string label;  // human-readable name
	std::string unit;   // °C, RPM, V, W, A, MHz, %, GB, MB/s
	double value = 0.0;
	Category category = Category::Other;
};

// Reads live sensor values from /sys and /proc (and nvidia-smi if present).
// Plain C++ with no Qt, so it can be tested on its own.
class SensorReader {
public:
	std::vector<Reading> read();

private:
	void readHwmon(std::vector<Reading> &out);
	void readCpuClocks(std::vector<Reading> &out);
	void readCpuUsage(std::vector<Reading> &out);
	void readMemory(std::vector<Reading> &out);
	void readDisks(std::vector<Reading> &out, double seconds);
	void readNetwork(std::vector<Reading> &out, double seconds);
	void readNvidia(std::vector<Reading> &out);

	// Previous totals, needed to turn counters into rates.
	std::vector<std::pair<unsigned long long, unsigned long long>> prevCpu; // total, idle
	std::map<std::string, std::pair<unsigned long long, unsigned long long>> prevDisk; // sectors read, written
	std::map<std::string, std::pair<unsigned long long, unsigned long long>> prevNet;  // bytes rx, tx
	std::chrono::steady_clock::time_point prevTime;
	bool havePrevTime = false;

	int nvidiaState = 0; // 0 = not checked yet, 1 = available, -1 = not available
};
