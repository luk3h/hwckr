#include "sensorreader.h"
#include "pciids.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

// Read the first line of a small sysfs file, trimmed. Returns "" if it can't be read.
std::string readLine(const fs::path &p)
{
	std::ifstream f(p);
	std::string s;
	std::getline(f, s);
	while (!s.empty() && isspace(static_cast<unsigned char>(s.back())))
		s.pop_back();
	size_t start = s.find_first_not_of(" \t");
	return start == std::string::npos ? "" : s.substr(start);
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

bool readCounter(const fs::path &p, unsigned long long &out)
{
	std::string s = readLine(p);
	if (s.empty())
		return false;
	try {
		out = std::stoull(s);
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

// Turn a hwmon chip name into a friendly device name and category.
std::pair<std::string, Category> describeChip(const std::string &chip, const fs::path &dev)
{
	if (chip == "coretemp") return {"CPU [Intel]", Category::Cpu};
	if (chip == "k10temp" || chip == "zenpower") return {"CPU [AMD]", Category::Cpu};
	if (chip == "cpu_thermal") return {"CPU", Category::Cpu};
	if (chip == "amdgpu" || chip == "radeon" || chip == "nouveau" || chip == "i915" || chip == "xe") {
		// Name the actual card, and tell built-in CPU graphics apart from a graphics card.
		fs::path pci = dev / "device";
		std::string prefix = PciIds::isIntegratedGpu(pci) ? "Integrated GPU: " : "GPU: ";
		return {prefix + PciIds::gpuName(pci), Category::Gpu};
	}
	if (chip == "nvme") {
		std::string model = readLine(dev / "device" / "model");
		return {model.empty() ? "NVMe Drive" : "NVMe: " + model, Category::Drive};
	}
	if (chip == "drivetemp") {
		std::string model = readLine(dev / "device" / "model");
		return {model.empty() ? "Drive" : "Drive: " + model, Category::Drive};
	}
	if (chip == "acpitz") return {"ACPI Thermal Zone", Category::Board};
	if (chip == "pch_cannonlake" || chip.rfind("pch_", 0) == 0) return {"Chipset [" + chip + "]", Category::Board};
	if (chip.rfind("nct", 0) == 0 || chip.rfind("it87", 0) == 0 || chip.rfind("it86", 0) == 0 ||
	    chip.rfind("f71", 0) == 0 || chip.rfind("w83", 0) == 0 || chip == "asus_wmi_sensors" ||
	    chip == "asus-ec-sensors" || chip == "gigabyte_wmi" || chip == "dell_smm")
		return {"Motherboard [" + chip + "]", Category::Board};
	if (chip.rfind("r8169", 0) == 0) return {"Ethernet [Realtek]", Category::Network};
	if (chip.rfind("igc", 0) == 0 || chip.rfind("e1000", 0) == 0) return {"Ethernet [Intel]", Category::Network};
	if (chip.rfind("iwlwifi", 0) == 0) return {"Wi-Fi [Intel]", Category::Network};
	if (chip.rfind("mt7", 0) == 0) return {"Wi-Fi [MediaTek]", Category::Network};
	if (chip.rfind("ath1", 0) == 0) return {"Wi-Fi [Qualcomm]", Category::Network};
	if (chip == "BAT0" || chip == "BAT1" || chip == "ADP1" || chip == "AC")
		return {"Battery [" + chip + "]", Category::Board};
	return {chip, Category::Other};
}

int categoryRank(Category c)
{
	switch (c) {
	case Category::Cpu: return 0;
	case Category::Gpu: return 1;
	case Category::Memory: return 2;
	case Category::Board: return 3;
	case Category::Drive: return 4;
	case Category::Network: return 5;
	default: return 6;
	}
}

// Whole disks only: skip partitions and virtual devices.
bool isRealDisk(const std::string &name)
{
	for (const char *skip : {"loop", "ram", "zram", "dm-", "sr", "fd"})
		if (name.rfind(skip, 0) == 0)
			return false;
	return fs::exists("/sys/block/" + name);
}

} // namespace

std::vector<Reading> SensorReader::read()
{
	auto now = std::chrono::steady_clock::now();
	double seconds = havePrevTime ? std::chrono::duration<double>(now - prevTime).count() : 0.0;
	prevTime = now;
	havePrevTime = true;

	std::vector<Reading> out;
	readCpuClocks(out);
	readCpuUsage(out);
	readMemory(out);
	readHwmon(out);
	readNvidia(out);
	readDisks(out, seconds);
	readNetwork(out, seconds);

	// CPU first, then GPU, memory, board, drives, network. Keeps the order within each.
	// Within graphics, the dedicated card comes before built-in CPU graphics.
	std::stable_sort(out.begin(), out.end(), [](const Reading &a, const Reading &b) {
		if (categoryRank(a.category) != categoryRank(b.category))
			return categoryRank(a.category) < categoryRank(b.category);
		bool ai = a.group.rfind("Integrated", 0) == 0, bi = b.group.rfind("Integrated", 0) == 0;
		return !ai && bi;
	});
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

	// Count device names so duplicates (e.g. two identical drives) get told apart.
	std::map<std::string, int> nameCount;
	for (auto &d : devices)
		nameCount[describeChip(readLine(d / "name"), d).first]++;

	// temp1_input, fan2_input, in0_input, power1_average, curr1_input ...
	static const std::regex sensorFile(R"(^(temp|fan|in|power|curr)(\d+)_(input|average)$)");

	for (auto &dev : devices) {
		std::string chip = readLine(dev / "name");
		if (chip.empty())
			chip = dev.filename();
		auto [group, category] = describeChip(chip, dev);
		if (nameCount[group] > 1)
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
			r.category = category;

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

			// Ignore obviously bogus values some chips report for unconnected inputs.
			if (r.unit == "°C" && (r.value <= -100 || r.value >= 200))
				continue;

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
		if (name.size() > 3 && name.rfind("cpu", 0) == 0 && isdigit(static_cast<unsigned char>(name[3])))
			cpus.push_back(trailingNumber(name));
	}
	std::sort(cpus.begin(), cpus.end());

	std::vector<Reading> threads;
	double sum = 0, maxClock = 0;
	for (int c : cpus) {
		double khz;
		fs::path p = root / ("cpu" + std::to_string(c)) / "cpufreq" / "scaling_cur_freq";
		if (!readNumber(p, khz))
			continue;
		double mhz = khz / 1000.0;
		threads.push_back({"CPU Clocks", "cpu" + std::to_string(c),
		                   "Thread " + std::to_string(c) + " Clock", "MHz", mhz, Category::Cpu});
		sum += mhz;
		maxClock = std::max(maxClock, mhz);
	}
	if (threads.empty())
		return;

	out.push_back({"CPU Clocks", "avg", "Average Clock", "MHz", sum / threads.size(), Category::Cpu});
	out.push_back({"CPU Clocks", "max", "Highest Clock", "MHz", maxClock, Category::Cpu});
	out.insert(out.end(), threads.begin(), threads.end());
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
			std::string label = names[i] == "cpu" ? "Total CPU Usage" : "Thread " + names[i].substr(3) + " Usage";
			out.push_back({"CPU Usage", names[i], label, "%", pct, Category::Cpu});
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

	out.push_back({"Memory", "load", "Memory Load", "%", total ? 100.0 * used / total : 0.0, Category::Memory});
	out.push_back({"Memory", "used", "Memory Used", "GB", used / GB, Category::Memory});
	out.push_back({"Memory", "avail", "Memory Available", "GB", avail / GB, Category::Memory});
	out.push_back({"Memory", "cached", "Cached", "GB", kb["Cached"] / GB, Category::Memory});

	if (kb["SwapTotal"] > 0) {
		double swapUsed = kb["SwapTotal"] - kb["SwapFree"];
		out.push_back({"Memory", "swap", "Swap Used", "GB", swapUsed / GB, Category::Memory});
	}
}

//--------------------------
// Drive read/write speed
// from /proc/diskstats (512-byte sectors)
//--------------------------
void SensorReader::readDisks(std::vector<Reading> &out, double seconds)
{
	std::ifstream f("/proc/diskstats");
	std::string line;
	while (std::getline(f, line)) {
		std::istringstream ss(line);
		unsigned long long major, minor, reads, readsMerged, sectorsRead, msRead, writes, writesMerged, sectorsWritten;
		std::string name;
		if (!(ss >> major >> minor >> name >> reads >> readsMerged >> sectorsRead >> msRead >> writes >> writesMerged >> sectorsWritten))
			continue;
		if (!isRealDisk(name))
			continue;

		auto prev = prevDisk.find(name);
		if (prev != prevDisk.end() && seconds > 0) {
			const double MB = 1024.0 * 1024.0;
			double readRate = (sectorsRead - prev->second.first) * 512.0 / MB / seconds;
			double writeRate = (sectorsWritten - prev->second.second) * 512.0 / MB / seconds;

			std::string model = readLine("/sys/block/" + name + "/device/model");
			std::string group = "Drive: " + name + (model.empty() ? "" : " [" + model + "]");
			out.push_back({group, "read", "Read Rate", "MB/s", readRate, Category::Drive});
			out.push_back({group, "write", "Write Rate", "MB/s", writeRate, Category::Drive});
		}
		prevDisk[name] = {sectorsRead, sectorsWritten};
	}
}

//--------------------------
// Network download/upload speed
// from /sys/class/net/*/statistics
//--------------------------
void SensorReader::readNetwork(std::vector<Reading> &out, double seconds)
{
	const fs::path root = "/sys/class/net";
	std::error_code ec;
	std::vector<std::string> names;
	for (auto &d : fs::directory_iterator(root, ec)) {
		std::string name = d.path().filename();
		// Skip loopback and virtual interfaces (docker, bridges, VPN tunnels).
		if (name == "lo" || !fs::exists(d.path() / "device"))
			continue;
		names.push_back(name);
	}
	std::sort(names.begin(), names.end());

	for (auto &name : names) {
		unsigned long long rx, tx;
		if (!readCounter(root / name / "statistics" / "rx_bytes", rx) ||
		    !readCounter(root / name / "statistics" / "tx_bytes", tx))
			continue;

		auto prev = prevNet.find(name);
		if (prev != prevNet.end() && seconds > 0) {
			const double KB = 1024.0;
			bool wifi = fs::exists(root / name / "wireless");
			std::string group = std::string(wifi ? "Wi-Fi: " : "Network: ") + name;
			out.push_back({group, "down", "Download Rate", "KB/s", (rx - prev->second.first) / KB / seconds, Category::Network});
			out.push_back({group, "up", "Upload Rate", "KB/s", (tx - prev->second.second) / KB / seconds, Category::Network});
			out.push_back({group, "total_down", "Total Downloaded", "MB", rx / (KB * KB), Category::Network});
			out.push_back({group, "total_up", "Total Uploaded", "MB", tx / (KB * KB), Category::Network});
		}
		prevNet[name] = {rx, tx};
	}
}

//--------------------------
// NVIDIA GPUs (proprietary driver doesn't use hwmon)
// via nvidia-smi
//--------------------------
void SensorReader::readNvidia(std::vector<Reading> &out)
{
	if (nvidiaState == 0)
		nvidiaState = (access("/usr/bin/nvidia-smi", X_OK) == 0) ? 1 : -1;
	if (nvidiaState != 1)
		return;

	FILE *p = popen("/usr/bin/nvidia-smi --query-gpu=index,name,temperature.gpu,utilization.gpu,"
	                "clocks.gr,clocks.mem,power.draw,fan.speed,memory.used,memory.total "
	                "--format=csv,noheader,nounits 2>/dev/null", "r");
	if (!p)
		return;

	char buf[512];
	while (fgets(buf, sizeof buf, p)) {
		std::vector<std::string> f;
		std::stringstream ss(buf);
		std::string field;
		while (std::getline(ss, field, ',')) {
			size_t a = field.find_first_not_of(" \n");
			size_t b = field.find_last_not_of(" \n");
			f.push_back(a == std::string::npos ? "" : field.substr(a, b - a + 1));
		}
		if (f.size() < 10)
			continue;

		std::string group = "GPU [NVIDIA " + f[1] + "]";
		auto add = [&](const std::string &key, const std::string &label, const std::string &unit,
		               const std::string &text, double scale = 1.0) {
			try {
				out.push_back({group, key, label, unit, std::stod(text) * scale, Category::Gpu});
			} catch (...) {
				// "[N/A]" or similar: sensor not supported on this card
			}
		};
		add("temp", "GPU Temperature", "°C", f[2]);
		add("load", "GPU Core Load", "%", f[3]);
		add("clock", "GPU Clock", "MHz", f[4]);
		add("memclock", "GPU Memory Clock", "MHz", f[5]);
		add("power", "GPU Power", "W", f[6]);
		add("fan", "GPU Fan", "%", f[7]);
		add("memused", "GPU Memory Used", "GB", f[8], 1.0 / 1024.0);
	}
	pclose(p);
}
