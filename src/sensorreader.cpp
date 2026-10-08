#include "sensorreader.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace {

// Read the first line of a small sysfs file. Returns "" if it can't be read.
std::string readLine(const fs::path &p)
{
	std::ifstream f(p);
	std::string s;
	std::getline(f, s);
	return s;
}

bool readNumber(const fs::path &p, double &out)
{
	std::string s = readLine(p);
	if (s.empty())
		return false;
	try {
		out = std::stod(s);
		return true;
	} catch (...) {
		return false;
	}
}

// Trailing number in a name like "hwmon3" or "cpu12", or -1.
int trailingNumber(const std::string &s)
{
	size_t i = s.size();
	while (i > 0 && isdigit(static_cast<unsigned char>(s[i - 1])))
		--i;
	return i == s.size() ? -1 : std::stoi(s.substr(i));
}

} // namespace

std::vector<Reading> SensorReader::read()
{
	std::vector<Reading> out;
	readCpuClocks(out);
	readCpuUsage(out);
	readMemory(out);
	readHwmon(out);
	return out;
}

//--------------------------
// Temperatures, fans, voltages, power, current
// from /sys/class/hwmon/hwmon*/
//--------------------------
void SensorReader::readHwmon(std::vector<Reading> &out)
{
	const fs::path root = "/sys/class/hwmon";
	std::error_code ec;
	if (!fs::exists(root, ec))
		return;

	// Collect and sort hwmon devices numerically (hwmon2 before hwmon10).
	std::vector<fs::path> devices;
	for (auto &d : fs::directory_iterator(root, ec))
		devices.push_back(d.path());
	std::sort(devices.begin(), devices.end(), [](const fs::path &a, const fs::path &b) {
		return trailingNumber(a.filename()) < trailingNumber(b.filename());
	});

	// Count chip names so duplicates (e.g. two "nvme" drives) get told apart.
	std::map<std::string, int> nameCount;
	for (auto &d : devices)
		nameCount[readLine(d / "name")]++;

	// temp1_input, fan2_input, in0_input, power1_average, curr1_input ...
	static const std::regex sensorFile(R"(^(temp|fan|in|power|curr)(\d+)_(input|average)$)");

	for (auto &dev : devices) {
		std::string chip = readLine(dev / "name");
		if (chip.empty())
			chip = dev.filename();
		std::string group = chip;
		if (nameCount[chip] > 1)
			group += " (" + dev.filename().string() + ")";

		struct Item { std::string type; int index; std::string file; };
		std::vector<Item> items;
		for (auto &f : fs::directory_iterator(dev, ec)) {
			std::string name = f.path().filename();
			std::smatch m;
			if (std::regex_match(name, m, sensorFile))
				items.push_back({m[1], std::stoi(m[2]), name});
		}

		// Group by type (temps, then fans, ...) and number within type.
		static const std::map<std::string, int> order = {
			{"temp", 0}, {"fan", 1}, {"in", 2}, {"power", 3}, {"curr", 4}};
		std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
			if (a.type != b.type)
				return order.at(a.type) < order.at(b.type);
			if (a.index != b.index)
				return a.index < b.index;
			return a.file < b.file;
		});

		for (auto &it : items) {
			double raw;
			if (!readNumber(dev / it.file, raw))
				continue;

			Reading r;
			r.group = group;
			r.key = it.file;

			std::string base = it.type + std::to_string(it.index);
			std::string label = readLine(dev / (base + "_label"));
			if (label.empty()) {
				if (it.type == "temp") label = "Temperature " + std::to_string(it.index);
				else if (it.type == "fan") label = "Fan " + std::to_string(it.index);
				else if (it.type == "in") label = "Voltage " + std::to_string(it.index);
				else if (it.type == "power") label = "Power " + std::to_string(it.index);
				else label = "Current " + std::to_string(it.index);
			}
			r.label = label;

			// sysfs units: millidegrees C, RPM, millivolts, microwatts, milliamps
			if (it.type == "temp")       { r.unit = "°C";  r.value = raw / 1000.0; }
			else if (it.type == "fan")   { r.unit = "RPM"; r.value = raw; }
			else if (it.type == "in")    { r.unit = "V";   r.value = raw / 1000.0; }
			else if (it.type == "power") { r.unit = "W";   r.value = raw / 1000000.0; }
			else                         { r.unit = "A";   r.value = raw / 1000.0; }

			out.push_back(r);
		}
	}
}

//--------------------------
// Per-thread clock speed
// from /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq (kHz)
//--------------------------
void SensorReader::readCpuClocks(std::vector<Reading> &out)
{
	const fs::path root = "/sys/devices/system/cpu";
	std::error_code ec;
	std::vector<int> cpus;
	for (auto &d : fs::directory_iterator(root, ec)) {
		std::string name = d.path().filename();
		if (name.rfind("cpu", 0) == 0 && trailingNumber(name) >= 0 &&
		    name.size() > 3 && isdigit(static_cast<unsigned char>(name[3])))
			cpus.push_back(trailingNumber(name));
	}
	std::sort(cpus.begin(), cpus.end());

	double sum = 0;
	int n = 0;
	for (int c : cpus) {
		double khz;
		fs::path p = root / ("cpu" + std::to_string(c)) / "cpufreq" / "scaling_cur_freq";
		if (!readNumber(p, khz))
			continue;
		out.push_back({"CPU Clocks", "cpu" + std::to_string(c),
		               "Thread " + std::to_string(c), "MHz", khz / 1000.0});
		sum += khz / 1000.0;
		n++;
	}
	if (n > 0)
		out.insert(out.end() - n, {"CPU Clocks", "avg", "Average", "MHz", sum / n});
}

//--------------------------
// CPU usage % (total and per thread)
// from /proc/stat, comparing against the previous reading
//--------------------------
void SensorReader::readCpuUsage(std::vector<Reading> &out)
{
	std::ifstream f("/proc/stat");
	std::string line;
	std::vector<std::pair<unsigned long long, unsigned long long>> now; // total, idle
	std::vector<std::string> names;

	while (std::getline(f, line)) {
		if (line.rfind("cpu", 0) != 0)
			break; // cpu lines are always first
		std::istringstream ss(line);
		std::string name;
		ss >> name;
		unsigned long long v, total = 0, idle = 0;
		int i = 0;
		while (ss >> v) {
			// fields: user nice system idle iowait irq softirq steal guest guest_nice
			if (i < 8)
				total += v; // guest time is already included in user/nice
			if (i == 3 || i == 4)
				idle += v;
			i++;
		}
		now.push_back({total, idle});
		names.push_back(name);
	}

	if (prevCpu.size() == now.size()) {
		for (size_t i = 0; i < now.size(); i++) {
			auto dTotal = now[i].first - prevCpu[i].first;
			auto dIdle = now[i].second - prevCpu[i].second;
			double pct = dTotal ? 100.0 * (double)(dTotal - dIdle) / dTotal : 0.0;
			std::string label = names[i] == "cpu" ? "Total" : "Thread " + names[i].substr(3);
			out.push_back({"CPU Usage", names[i], label, "%", pct});
		}
	}
	prevCpu = now;
}

//--------------------------
// RAM and swap
// from /proc/meminfo (kB)
//--------------------------
void SensorReader::readMemory(std::vector<Reading> &out)
{
	std::ifstream f("/proc/meminfo");
	std::map<std::string, double> kb;
	std::string key;
	double value;
	std::string unit;
	while (f >> key >> value) {
		std::getline(f, unit); // swallow " kB"
		if (!key.empty() && key.back() == ':')
			key.pop_back();
		kb[key] = value;
	}
	if (!kb.count("MemTotal"))
		return;

	const double GB = 1024.0 * 1024.0;
	double total = kb["MemTotal"];
	double avail = kb.count("MemAvailable") ? kb["MemAvailable"] : kb["MemFree"];
	double used = total - avail;

	out.push_back({"Memory", "used", "Used", "GB", used / GB});
	out.push_back({"Memory", "avail", "Available", "GB", avail / GB});
	out.push_back({"Memory", "load", "Load", "%", total ? 100.0 * used / total : 0.0});

	if (kb["SwapTotal"] > 0) {
		double swapUsed = kb["SwapTotal"] - kb["SwapFree"];
		out.push_back({"Memory", "swap", "Swap Used", "GB", swapUsed / GB});
	}
}
