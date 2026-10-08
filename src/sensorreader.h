#pragma once

#include <string>
#include <utility>
#include <vector>

// One reading of one sensor at one moment, e.g. {"coretemp", "Package id 0", "°C", 54.0}.
struct Reading {
	std::string group;  // device it belongs to (shown as a collapsible parent row)
	std::string key;    // stable unique id within the group (used to track min/max)
	std::string label;  // human-readable name
	std::string unit;   // °C, RPM, V, W, A, MHz, %, GB
	double value = 0.0;
};

// Reads live sensor values from /sys and /proc. Plain C++ with no Qt,
// so it can be tested on its own.
class SensorReader {
public:
	std::vector<Reading> read();

private:
	void readHwmon(std::vector<Reading> &out);
	void readCpuClocks(std::vector<Reading> &out);
	void readCpuUsage(std::vector<Reading> &out);
	void readMemory(std::vector<Reading> &out);

	// Previous /proc/stat totals per CPU line, needed to work out usage %.
	std::vector<std::pair<unsigned long long, unsigned long long>> prevCpu;
};
