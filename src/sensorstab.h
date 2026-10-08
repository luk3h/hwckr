#pragma once

#include "sensorreader.h"

#include <QWidget>

#include <deque>
#include <fstream>
#include <map>
#include <string>
#include <vector>

class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

// HWiNFO-style sensors table:
// Sensor | Current | Minimum | Maximum | Average | Graph, grouped by device.
class SensorsTab : public QWidget {
	Q_OBJECT

public:
	explicit SensorsTab(QWidget *parent = nullptr);

	// Take a new reading, update the table, and return the readings
	// (so other pages can show them too).
	std::vector<Reading> refresh();

	void resetStats();
	bool startLog(const QString &path);
	void stopLog();
	bool isLogging() const { return log.is_open(); }
	int sensorCount() const { return static_cast<int>(rows.size()); }

private:
	struct Row {
		QTreeWidgetItem *item = nullptr;
		std::string unit;
		double current = 0, min = 0, max = 0, sum = 0;
		long count = 0;
		std::deque<double> history;
	};

	void applyFilter();

	SensorReader reader;
	QTreeWidget *tree;
	QLineEdit *filter;

	std::map<std::string, QTreeWidgetItem *> groups; // by group name
	std::map<std::string, Row> rows;                 // by group + "|" + key
	std::vector<std::string> order;                  // row ids in display order

	std::ofstream log;
	std::vector<std::string> logColumns;
};
