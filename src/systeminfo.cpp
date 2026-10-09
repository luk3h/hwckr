#include "systeminfo.h"
#include "pciids.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ifaddrs.h>
#include <map>
#include <set>
#include <sstream>
#include <sys/utsname.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

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

std::string linkTarget(const fs::path &p)
{
	std::error_code ec;
	fs::path t = fs::read_symlink(p, ec);
	return ec ? "" : t.filename().string();
}

std::string formatBytes(double bytes)
{
	const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
	int u = 0;
	while (bytes >= 1024.0 && u < 5) {
		bytes /= 1024.0;
		u++;
	}
	std::ostringstream ss;
	ss.setf(std::ios::fixed);
	ss.precision(u >= 3 ? 1 : 0);
	ss << bytes << " " << units[u];
	return ss.str();
}

std::string formatMHz(double mhz)
{
	std::ostringstream ss;
	ss << static_cast<long>(mhz + 0.5) << " MHz";
	return ss.str();
}

void add(Device &d, const std::string &name, const std::string &value)
{
	if (!value.empty())
		d.properties.push_back({name, value});
}

void section(Device &d, const std::string &title)
{
	d.properties.push_back({title, ""});
}

//--------------------------
// CPU
//--------------------------
Device collectCpu()
{
	Device d;
	d.category = Category::Cpu;

	std::ifstream f("/proc/cpuinfo");
	std::string line;
	std::map<std::string, std::string> first; // values from the first processor block
	std::set<std::pair<std::string, std::string>> cores; // unique (physical id, core id)
	std::set<std::string> sockets;
	std::string physId;
	int threads = 0;

	while (std::getline(f, line)) {
		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;
		std::string key = line.substr(0, colon);
		std::string value = colon + 2 <= line.size() ? line.substr(colon + 2) : "";
		while (!key.empty() && isspace(static_cast<unsigned char>(key.back())))
			key.pop_back();

		if (key == "processor")
			threads++;
		if (key == "physical id") {
			physId = value;
			sockets.insert(value);
		}
		if (key == "core id")
			cores.insert({physId, value});
		if (!first.count(key))
			first[key] = value;
	}

	std::string model = first.count("model name") ? first["model name"] : first["Model"];
	if (model.empty())
		model = "Unknown CPU";
	d.title = model;

	struct utsname u{};
	uname(&u);

	section(d, "Processor");
	add(d, "Model", model);
	add(d, "Vendor", first["vendor_id"]);
	add(d, "Architecture", u.machine);
	std::string sig;
	if (first.count("cpu family"))
		sig = "Family " + first["cpu family"] + ", Model " + first["model"] + ", Stepping " + first["stepping"];
	add(d, "Signature", sig);
	add(d, "Microcode", first["microcode"]);

	section(d, "Topology");
	add(d, "Sockets", sockets.empty() ? "1" : std::to_string(sockets.size()));
	int physical = cores.empty() ? threads : static_cast<int>(cores.size());
	add(d, "Cores", std::to_string(physical));
	add(d, "Threads", std::to_string(threads));
	add(d, "Hyper-Threading / SMT", threads > physical ? "Enabled" : "Disabled / not supported");

	section(d, "Clocks");
	const fs::path freq = "/sys/devices/system/cpu/cpu0/cpufreq";
	std::string minF = readLine(freq / "cpuinfo_min_freq");
	std::string maxF = readLine(freq / "cpuinfo_max_freq");
	if (!minF.empty()) add(d, "Minimum Clock", formatMHz(std::stod(minF) / 1000.0));
	if (!maxF.empty()) add(d, "Maximum Clock", formatMHz(std::stod(maxF) / 1000.0));
	add(d, "Frequency Driver", readLine(freq / "scaling_driver"));
	add(d, "Governor", readLine(freq / "scaling_governor"));

	section(d, "Cache");
	const fs::path cache = "/sys/devices/system/cpu/cpu0/cache";
	std::error_code ec;
	std::vector<std::pair<std::string, std::string>> caches;
	for (auto &idx : fs::directory_iterator(cache, ec)) {
		if (idx.path().filename().string().rfind("index", 0) != 0)
			continue;
		std::string level = readLine(idx.path() / "level");
		std::string type = readLine(idx.path() / "type");
		std::string size = readLine(idx.path() / "size");
		std::string name = "L" + level + (type == "Data" ? " Data" : type == "Instruction" ? " Instruction" : "");
		caches.push_back({name, size});
	}
	std::sort(caches.begin(), caches.end());
	for (auto &[name, size] : caches)
		add(d, name, size + " per " + (name.rfind("L3", 0) == 0 ? "package" : "core"));

	// Instruction set feature grid, shown as lit/unlit pills like HWiNFO.
	std::set<std::string> flags;
	std::istringstream fl(first["flags"]);
	for (std::string flag; fl >> flag;)
		flags.insert(flag);

	if (!flags.empty()) {
		const std::vector<std::pair<std::string, std::string>> known = {
			{"MMX", "mmx"}, {"SSE", "sse"}, {"SSE2", "sse2"}, {"SSE3", "pni"},
			{"SSSE3", "ssse3"}, {"SSE4.1", "sse4_1"}, {"SSE4.2", "sse4_2"}, {"SSE4A", "sse4a"},
			{"AVX", "avx"}, {"AVX2", "avx2"}, {"AVX-512", "avx512f"}, {"FMA3", "fma"},
			{"AES-NI", "aes"}, {"SHA", "sha_ni"}, {"BMI1", "bmi1"}, {"BMI2", "bmi2"},
			{"F16C", "f16c"}, {"POPCNT", "popcnt"}, {"ADX", "adx"}, {"RDRAND", "rdrand"},
			{"x86-64", "lm"}, {"NX", "nx"}, {"HTT", "ht"}, {"VT-x", "vmx"},
			{"AMD-V", "svm"}, {"Hypervisor", "hypervisor"}};
		for (auto &[label, flag] : known)
			d.features.push_back({label, flags.count(flag) > 0});
	}
	return d;
}

//--------------------------
// Motherboard and BIOS
//--------------------------
Device collectBoard()
{
	Device d;
	d.category = Category::Board;
	const fs::path dmi = "/sys/class/dmi/id";

	std::string vendor = readLine(dmi / "board_vendor");
	std::string name = readLine(dmi / "board_name");
	if (vendor.empty() && name.empty())
		d.title = "Motherboard";
	else
		d.title = vendor.empty() ? name : name.empty() ? vendor : vendor + " " + name;

	section(d, "System");
	add(d, "Manufacturer", readLine(dmi / "sys_vendor"));
	add(d, "Product", readLine(dmi / "product_name"));
	add(d, "Version", readLine(dmi / "product_version"));
	std::string chassis = readLine(dmi / "chassis_type");
	static const std::map<std::string, std::string> chassisTypes = {
		{"3", "Desktop"}, {"4", "Low Profile Desktop"}, {"6", "Mini Tower"}, {"7", "Tower"},
		{"8", "Portable"}, {"9", "Laptop"}, {"10", "Notebook"}, {"13", "All in One"},
		{"14", "Sub Notebook"}, {"17", "Main Server Chassis"}, {"23", "Rack Mount Chassis"},
		{"30", "Tablet"}, {"31", "Convertible"}, {"32", "Detachable"}, {"35", "Mini PC"}, {"36", "Stick PC"}};
	if (chassisTypes.count(chassis))
		add(d, "Form Factor", chassisTypes.at(chassis));

	section(d, "Motherboard");
	add(d, "Manufacturer", vendor);
	add(d, "Model", name);
	add(d, "Revision", readLine(dmi / "board_version"));

	section(d, "BIOS / UEFI");
	add(d, "Vendor", readLine(dmi / "bios_vendor"));
	add(d, "Version", readLine(dmi / "bios_version"));
	add(d, "Release Date", readLine(dmi / "bios_date"));
	add(d, "Boot Mode", fs::exists("/sys/firmware/efi") ? "UEFI" : "Legacy BIOS");
	return d;
}

//--------------------------
// Memory
//--------------------------
Device collectMemory()
{
	Device d;
	d.category = Category::Memory;

	std::ifstream f("/proc/meminfo");
	std::map<std::string, double> kb;
	std::string key, unit;
	double value;
	while (f >> key >> value) {
		std::getline(f, unit);
		if (!key.empty() && key.back() == ':')
			key.pop_back();
		kb[key] = value;
	}

	std::string total = formatBytes(kb["MemTotal"] * 1024.0);
	d.title = "Memory (" + total + ")";

	section(d, "System Memory");
	add(d, "Total", total);
	add(d, "Swap", kb["SwapTotal"] > 0 ? formatBytes(kb["SwapTotal"] * 1024.0) : "None");
	add(d, "Page Size", std::to_string(sysconf(_SC_PAGESIZE)) + " bytes");
	if (kb["HugePages_Total"] > 0)
		add(d, "Huge Pages", std::to_string(static_cast<long>(kb["HugePages_Total"])));

	section(d, "Modules");
	add(d, "Details", "Run with sudo and install dmidecode for per-module info");
	return d;
}

//--------------------------
// Graphics cards
//--------------------------
std::vector<Device> collectGpus()
{
	std::vector<Device> gpus;
	std::error_code ec;
	std::vector<fs::path> cards;
	for (auto &c : fs::directory_iterator("/sys/class/drm", ec)) {
		std::string n = c.path().filename();
		if (n.rfind("card", 0) == 0 && n.find('-') == std::string::npos)
			cards.push_back(c.path());
	}
	std::sort(cards.begin(), cards.end());

	// Dedicated cards first, built-in CPU graphics after.
	std::stable_sort(cards.begin(), cards.end(), [](const fs::path &a, const fs::path &b) {
		return !PciIds::isIntegratedGpu(a / "device") && PciIds::isIntegratedGpu(b / "device");
	});

	for (auto &card : cards) {
		fs::path dev = card / "device";
		std::string vendorId = readLine(dev / "vendor");
		std::string deviceId = readLine(dev / "device");
		if (vendorId.empty())
			continue;

		auto [vendorName, deviceName] = PciIds::lookup(vendorId, deviceId);
		if (vendorName.empty())
			vendorName = PciIds::shortVendor(vendorId);
		bool integrated = PciIds::isIntegratedGpu(dev);

		Device d;
		d.category = Category::Gpu;
		d.title = PciIds::gpuName(dev) + (integrated ? " (integrated)" : "");

		section(d, integrated ? "Integrated Graphics" : "Graphics Card");
		add(d, "Name", PciIds::gpuName(dev));
		add(d, "Chip", deviceName);
		add(d, "Type", integrated ? "Built into the CPU" : "Dedicated graphics card");
		add(d, "Vendor", vendorName);
		add(d, "PCI ID", vendorId.substr(2) + ":" + deviceId.substr(2));
		add(d, "PCI Slot", linkTarget(dev));
		add(d, "Driver", linkTarget(dev / "driver"));

		std::string vram = readLine(dev / "mem_info_vram_total");
		if (!vram.empty())
			add(d, "Video Memory", formatBytes(std::stod(vram)));

		std::string speed = readLine(dev / "current_link_speed");
		std::string width = readLine(dev / "current_link_width");
		std::string maxSpeed = readLine(dev / "max_link_speed");
		std::string maxWidth = readLine(dev / "max_link_width");
		if (!speed.empty()) {
			section(d, "PCI Express Link");
			add(d, "Current", speed + " x" + width);
			add(d, "Maximum", maxSpeed + " x" + maxWidth);
		}
		gpus.push_back(d);
	}
	return gpus;
}

//--------------------------
// Drives
//--------------------------
std::vector<Device> collectDrives()
{
	std::vector<Device> drives;
	std::error_code ec;
	std::vector<std::string> names;
	for (auto &b : fs::directory_iterator("/sys/block", ec)) {
		std::string n = b.path().filename();
		bool skip = false;
		for (const char *s : {"loop", "ram", "zram", "dm-", "sr", "fd"})
			if (n.rfind(s, 0) == 0)
				skip = true;
		if (!skip)
			names.push_back(n);
	}
	std::sort(names.begin(), names.end());

	for (auto &n : names) {
		fs::path b = fs::path("/sys/block") / n;
		std::string model = readLine(b / "device" / "model");
		std::string vendor = readLine(b / "device" / "vendor");
		double sectors = 0;
		try { sectors = std::stod(readLine(b / "size")); } catch (...) {}
		if (sectors == 0)
			continue;

		bool nvme = n.rfind("nvme", 0) == 0;
		bool rotational = readLine(b / "queue" / "rotational") == "1";
		bool removable = readLine(b / "removable") == "1";
		std::string type;
		if (nvme) type = "NVMe SSD";
		else if (n.rfind("vd", 0) == 0) type = "Virtual Disk";
		else if (n.rfind("mmcblk", 0) == 0) type = "SD Card / eMMC";
		else if (rotational) type = "Hard Disk (HDD)";
		else type = "SSD";
		if (removable)
			type += " (removable)";

		Device d;
		d.category = Category::Drive;
		std::string size = formatBytes(sectors * 512.0);
		d.title = (model.empty() ? n : model) + " (" + size + ")";

		section(d, "Drive");
		add(d, "Model", model.empty() ? "Unknown" : model);
		if (vendor != "ATA" && vendor.rfind("0x", 0) != 0)
			add(d, "Vendor", vendor);
		add(d, "Device", "/dev/" + n);
		add(d, "Type", type);
		add(d, "Capacity", size);
		add(d, "Firmware", readLine(b / "device" / "firmware_rev"));
		add(d, "Scheduler", readLine(b / "queue" / "scheduler"));

		int parts = 0;
		for (auto &p : fs::directory_iterator(b, ec))
			if (p.path().filename().string().rfind(n, 0) == 0)
				parts++;
		add(d, "Partitions", std::to_string(parts));

		drives.push_back(d);
	}
	return drives;
}

//--------------------------
// Network adapters
//--------------------------
std::vector<Device> collectNetwork()
{
	// IPv4 addresses by interface
	std::map<std::string, std::string> ips;
	struct ifaddrs *list = nullptr;
	if (getifaddrs(&list) == 0) {
		for (auto *a = list; a; a = a->ifa_next) {
			if (a->ifa_addr && a->ifa_addr->sa_family == AF_INET) {
				char buf[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in *>(a->ifa_addr)->sin_addr, buf, sizeof buf);
				ips[a->ifa_name] = buf;
			}
		}
		freeifaddrs(list);
	}

	std::vector<Device> adapters;
	std::error_code ec;
	std::vector<std::string> names;
	for (auto &n : fs::directory_iterator("/sys/class/net", ec)) {
		std::string name = n.path().filename();
		if (name != "lo" && fs::exists(n.path() / "device"))
			names.push_back(name);
	}
	std::sort(names.begin(), names.end());

	for (auto &name : names) {
		fs::path n = fs::path("/sys/class/net") / name;
		bool wifi = fs::exists(n / "wireless");

		Device d;
		d.category = Category::Network;
		d.title = std::string(wifi ? "Wi-Fi: " : "Ethernet: ") + name;

		section(d, "Network Adapter");
		add(d, "Interface", name);
		add(d, "Type", wifi ? "Wireless" : "Wired");
		add(d, "Driver", linkTarget(n / "device" / "driver"));
		add(d, "MAC Address", readLine(n / "address"));
		std::string state = readLine(n / "operstate");
		if (!state.empty())
			state[0] = toupper(state[0]);
		add(d, "Status", state);
		std::string speed = readLine(n / "speed");
		if (!speed.empty() && speed[0] != '-')
			add(d, "Link Speed", speed + " Mbps");
		add(d, "MTU", readLine(n / "mtu"));
		if (ips.count(name))
			add(d, "IPv4 Address", ips[name]);

		adapters.push_back(d);
	}
	return adapters;
}

//--------------------------
// Operating system
//--------------------------
Device collectOs()
{
	Device d;
	d.category = Category::Other;
	d.title = SystemInfo::osName();

	struct utsname u{};
	uname(&u);

	section(d, "Operating System");
	add(d, "Name", SystemInfo::osName());
	add(d, "Kernel", u.release);
	add(d, "Hostname", SystemInfo::hostname());
	const char *desktop = getenv("XDG_CURRENT_DESKTOP");
	add(d, "Desktop", desktop ? desktop : "");
	const char *session = getenv("XDG_SESSION_TYPE");
	add(d, "Display Server", session ? session : "");
	add(d, "Uptime", SystemInfo::uptime());
	return d;
}

} // namespace

std::vector<Device> SystemInfo::collect()
{
	std::vector<Device> all;
	all.push_back(collectCpu());
	all.push_back(collectBoard());
	all.push_back(collectMemory());
	for (auto &g : collectGpus()) all.push_back(g);
	for (auto &dr : collectDrives()) all.push_back(dr);
	for (auto &n : collectNetwork()) all.push_back(n);
	all.push_back(collectOs());

	// Drop section headings that ended up with nothing under them
	// (e.g. no BIOS info readable inside a virtual machine).
	for (auto &d : all) {
		std::vector<Property> kept;
		for (size_t i = 0; i < d.properties.size(); i++) {
			bool heading = d.properties[i].value.empty();
			bool nextIsHeading = i + 1 >= d.properties.size() || d.properties[i + 1].value.empty();
			if (heading && nextIsHeading)
				continue;
			kept.push_back(d.properties[i]);
		}
		d.properties = kept;
	}
	return all;
}

std::string SystemInfo::hostname()
{
	char buf[256] = {};
	gethostname(buf, sizeof buf - 1);
	return buf;
}

std::string SystemInfo::kernel()
{
	struct utsname u{};
	uname(&u);
	return u.release;
}

std::string SystemInfo::osName()
{
	std::ifstream f("/etc/os-release");
	std::string line;
	while (std::getline(f, line)) {
		if (line.rfind("PRETTY_NAME=", 0) == 0) {
			std::string v = line.substr(12);
			if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
				v = v.substr(1, v.size() - 2);
			return v;
		}
	}
	return "Linux";
}

std::string SystemInfo::uptime()
{
	double secs = 0;
	std::ifstream f("/proc/uptime");
	f >> secs;
	long s = static_cast<long>(secs);
	char buf[64];
	long days = s / 86400;
	if (days > 0)
		snprintf(buf, sizeof buf, "%ldd %02ld:%02ld:%02ld", days, (s / 3600) % 24, (s / 60) % 60, s % 60);
	else
		snprintf(buf, sizeof buf, "%02ld:%02ld:%02ld", (s / 3600) % 24, (s / 60) % 60, s % 60);
	return buf;
}
