#pragma once

#include "sensorreader.h" // for Category

#include <string>
#include <utility>
#include <vector>

// One labelled value on the Summary page, e.g. {"Model", "AMD Ryzen 7 5800X"}.
// A property with an empty value is a section heading.
struct Property {
	std::string name;
	std::string value;
};

// A device in the Summary tree (CPU, motherboard, a GPU, a drive ...).
struct Device {
	Category category = Category::Other;
	std::string title;                         // shown in the tree
	std::vector<Property> properties;          // shown in the detail panel
	std::vector<std::pair<std::string, bool>> features; // CPU instruction sets (CPU only)
};

// Reads static hardware information once at startup. Plain C++, no Qt.
namespace SystemInfo {
	std::vector<Device> collect();

	std::string hostname();
	std::string kernel();
	std::string osName();
	std::string uptime(); // e.g. "3d 04:12:55"
}
