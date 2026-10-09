#pragma once

#include <filesystem>
#include <string>
#include <utility>

// Helpers for naming PCI devices (mainly graphics cards). Plain C++, no Qt.
namespace PciIds {
	// Vendor and device names from the system's pci.ids database, e.g.
	// {"Advanced Micro Devices, Inc. [AMD/ATI]", "Navi 48 [Radeon RX 9070/9070 XT/9070 GRE]"}.
	// Either can be empty if the database isn't installed or has no entry.
	std::pair<std::string, std::string> lookup(std::string vendorId, std::string deviceId);

	// The exact board name for a card from a specific maker (subsystem), e.g.
	// "Pulse Radeon RX 9070 XT", or "" if the database doesn't list it.
	std::string lookupSubsystem(std::string vendorId, std::string deviceId,
	                            std::string subVendorId, std::string subDeviceId);

	// Short, friendly vendor name: "AMD", "NVIDIA", "Intel" ...
	std::string shortVendor(const std::string &vendorId);

	// Friendly name for the GPU behind a PCI device folder (e.g. /sys/class/drm/card1/device),
	// such as "Radeon RX 9070 XT" or "Radeon Graphics". Falls back to "AMD GPU" etc.
	std::string gpuName(const std::filesystem::path &pciDevice);

	// True for graphics built into the CPU rather than a separate card.
	bool isIntegratedGpu(const std::filesystem::path &pciDevice);
}
