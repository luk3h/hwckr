#include "pciids.h"

#include <algorithm>
#include <fstream>

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

std::string normaliseId(std::string id)
{
	if (id.rfind("0x", 0) == 0)
		id = id.substr(2);
	std::transform(id.begin(), id.end(), id.begin(), ::tolower);
	return id;
}

// "Navi 48 [Radeon RX 9070/9070 XT/9070 GRE]" -> "Radeon RX 9070/9070 XT/9070 GRE"
std::string marketingName(const std::string &full)
{
	size_t open = full.find('[');
	size_t close = full.rfind(']');
	if (open != std::string::npos && close != std::string::npos && close > open + 1)
		return full.substr(open + 1, close - open - 1);
	return full;
}

} // namespace

std::pair<std::string, std::string> PciIds::lookup(std::string vendorId, std::string deviceId)
{
	vendorId = normaliseId(vendorId);
	deviceId = normaliseId(deviceId);

	for (const char *path : {"/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids", "/usr/share/pci.ids"}) {
		std::ifstream f(path);
		if (!f)
			continue;
		std::string line, vendorName;
		bool inVendor = false;
		while (std::getline(f, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			if (line[0] != '\t') {
				if (inVendor)
					break; // passed our vendor without finding the device
				if (line.rfind(vendorId + "  ", 0) == 0) {
					inVendor = true;
					vendorName = line.substr(6);
				}
			} else if (inVendor && line.size() > 1 && line[1] != '\t' &&
			           line.rfind("\t" + deviceId + "  ", 0) == 0) {
				return {vendorName, line.substr(7)};
			}
		}
		if (inVendor)
			return {vendorName, ""};
	}
	return {"", ""};
}

std::string PciIds::lookupSubsystem(std::string vendorId, std::string deviceId,
                                    std::string subVendorId, std::string subDeviceId)
{
	vendorId = normaliseId(vendorId);
	deviceId = normaliseId(deviceId);
	const std::string prefix = "\t\t" + normaliseId(subVendorId) + " " + normaliseId(subDeviceId) + "  ";

	for (const char *path : {"/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids", "/usr/share/pci.ids"}) {
		std::ifstream f(path);
		if (!f)
			continue;
		std::string line;
		bool inVendor = false, inDevice = false;
		while (std::getline(f, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			if (line[0] != '\t') {
				if (inVendor)
					return "";
				inVendor = line.rfind(vendorId + "  ", 0) == 0;
			} else if (inVendor && line.size() > 1 && line[1] != '\t') {
				if (inDevice)
					return ""; // left our device's subsystem list
				inDevice = line.rfind("\t" + deviceId + "  ", 0) == 0;
			} else if (inDevice && line.rfind(prefix, 0) == 0) {
				return line.substr(prefix.size());
			}
		}
		return "";
	}
	return "";
}

std::string PciIds::shortVendor(const std::string &vendorId)
{
	std::string id = normaliseId(vendorId);
	if (id == "10de") return "NVIDIA";
	if (id == "1002") return "AMD";
	if (id == "8086") return "Intel";
	if (id == "1af4") return "Red Hat (virtio)";
	if (id == "15ad") return "VMware";
	if (id == "1234") return "QEMU";
	if (id == "1414") return "Microsoft (Hyper-V)";
	return "0x" + id;
}

std::string PciIds::gpuName(const fs::path &pciDevice)
{
	// Some cards report their exact retail name themselves.
	std::string product = readLine(pciDevice / "product_name");
	if (!product.empty())
		return product;

	std::string vendorId = readLine(pciDevice / "vendor");
	std::string deviceId = readLine(pciDevice / "device");

	// The card maker's entry often names the exact model ("Pulse Radeon RX 9070 XT").
	std::string board = lookupSubsystem(vendorId, deviceId,
	                                    readLine(pciDevice / "subsystem_vendor"),
	                                    readLine(pciDevice / "subsystem_device"));
	if (!board.empty())
		return board;

	auto [vendorName, deviceName] = lookup(vendorId, deviceId);
	if (!deviceName.empty())
		return marketingName(deviceName);
	return shortVendor(vendorId) + " GPU";
}

bool PciIds::isIntegratedGpu(const fs::path &pciDevice)
{
	std::string vendorId = normaliseId(readLine(pciDevice / "vendor"));
	std::string name = gpuName(pciDevice);

	// Intel: everything except Arc cards is built into the CPU.
	if (vendorId == "8086")
		return name.find("Arc") == std::string::npos;

	// AMD: CPU graphics are named plainly "Radeon Graphics" or with an "M" model (680M, 780M ...).
	if (name == "Radeon Graphics" || name.find("Radeon Vega") != std::string::npos)
		return true;
	for (const char *m : {"610M", "660M", "680M", "740M", "760M", "780M", "880M", "890M", "8060S", "8050S"})
		if (name.find(m) != std::string::npos)
			return true;

	// Otherwise fall back to memory size: built-in graphics only borrow a little system RAM.
	std::string vram = readLine(pciDevice / "mem_info_vram_total");
	if (!vram.empty()) {
		try {
			return std::stod(vram) < 2.0 * 1024 * 1024 * 1024;
		} catch (...) {
		}
	}
	return false;
}
