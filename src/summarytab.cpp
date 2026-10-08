#include "summarytab.h"
#include "theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <map>

namespace {

constexpr int DeviceIndexRole = Qt::UserRole + 1;

QString fmt(double v, int decimals)
{
	return QString::number(v, 'f', decimals);
}

} // namespace

SummaryTab::SummaryTab(QWidget *parent)
	: QWidget(parent),
	  devices(SystemInfo::collect()),
	  deviceTree(new QTreeWidget(this)),
	  deviceIcon(new QLabel(this)),
	  deviceKind(new QLabel(this)),
	  deviceTitle(new QLabel(this)),
	  featureBox(new QWidget(this)),
	  featureGrid(new QGridLayout()),
	  details(new QTreeWidget(this))
{
	setObjectName("Page");
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(14, 12, 14, 12);
	layout->setSpacing(12);

	//--------------------------
	// Live stat cards
	//--------------------------
	auto *cards = new QWidget(this);
	auto *cardGrid = new QGridLayout(cards);
	cardGrid->setContentsMargins(0, 0, 0, 0);
	cardGrid->setHorizontalSpacing(12);
	cpuLoad = makeCard("CPU USAGE", Category::Cpu, cards, cardGrid, 0);
	cpuClock = makeCard("CPU CLOCK", Category::Cpu, cards, cardGrid, 1);
	cpuTemp = makeCard("CPU TEMPERATURE", Category::Cpu, cards, cardGrid, 2);
	memLoad = makeCard("MEMORY", Category::Memory, cards, cardGrid, 3);
	layout->addWidget(cards);

	//--------------------------
	// Device tree (left)
	//--------------------------
	deviceTree->setHeaderHidden(true);
	deviceTree->setIconSize(QSize(20, 20));
	deviceTree->setIndentation(14);
	deviceTree->setMinimumWidth(260);

	auto *root = new QTreeWidgetItem(deviceTree);
	root->setText(0, QString::fromStdString(SystemInfo::hostname()));
	root->setIcon(0, Theme::categoryIcon(Category::Other));
	QFont bold = root->font(0);
	bold.setBold(true);
	root->setFont(0, bold);
	root->setSizeHint(0, QSize(0, 30));

	// Drives and network adapters get a folder each when there's more than one.
	std::map<Category, int> counts;
	for (auto &d : devices)
		counts[d.category]++;
	std::map<Category, QTreeWidgetItem *> folders;

	for (int i = 0; i < static_cast<int>(devices.size()); i++) {
		const Device &d = devices[i];
		QTreeWidgetItem *parentItem = root;
		bool folder = (d.category == Category::Drive || d.category == Category::Network || d.category == Category::Gpu) && counts[d.category] > 1;
		if (folder) {
			QTreeWidgetItem *&f = folders[d.category];
			if (!f) {
				f = new QTreeWidgetItem(root);
				QString name = d.category == Category::Drive ? "Storage" : d.category == Category::Gpu ? "Graphics" : "Network";
				f->setText(0, name + "  (" + QString::number(counts[d.category]) + ")");
				f->setIcon(0, Theme::categoryIcon(d.category));
				f->setForeground(0, Theme::Muted);
				f->setSizeHint(0, QSize(0, 28));
				f->setFlags(Qt::ItemIsEnabled);
			}
			parentItem = f;
		}
		auto *item = new QTreeWidgetItem(parentItem);
		item->setText(0, QString::fromStdString(d.title));
		item->setToolTip(0, QString::fromStdString(d.title));
		item->setIcon(0, Theme::categoryIcon(d.category));
		item->setData(0, DeviceIndexRole, i);
		item->setSizeHint(0, QSize(0, 28));
	}
	deviceTree->expandAll();

	connect(deviceTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *cur) {
		if (cur && cur->data(0, DeviceIndexRole).isValid())
			showDevice(cur->data(0, DeviceIndexRole).toInt());
	});

	//--------------------------
	// Device detail panel (right)
	//--------------------------
	auto *panel = new QFrame(this);
	panel->setObjectName("Card");
	auto *panelLayout = new QVBoxLayout(panel);
	panelLayout->setContentsMargins(18, 16, 18, 16);
	panelLayout->setSpacing(12);

	auto *titleRow = new QHBoxLayout();
	titleRow->setSpacing(14);
	deviceIcon->setFixedSize(44, 44);
	auto *titleText = new QVBoxLayout();
	titleText->setSpacing(2);
	deviceKind->setObjectName("DeviceKind");
	deviceTitle->setObjectName("DeviceTitle");
	deviceTitle->setWordWrap(true);
	titleText->addWidget(deviceKind);
	titleText->addWidget(deviceTitle);
	titleRow->addWidget(deviceIcon);
	titleRow->addLayout(titleText, 1);
	panelLayout->addLayout(titleRow);

	// CPU instruction-set pills
	auto *featureLayout = new QVBoxLayout(featureBox);
	featureLayout->setContentsMargins(0, 4, 0, 4);
	featureLayout->setSpacing(8);
	auto *featureLabel = new QLabel("INSTRUCTION SETS", featureBox);
	featureLabel->setObjectName("SectionLabel");
	featureLayout->addWidget(featureLabel);
	featureGrid->setHorizontalSpacing(6);
	featureGrid->setVerticalSpacing(6);
	featureLayout->addLayout(featureGrid);
	panelLayout->addWidget(featureBox);

	details->setColumnCount(2);
	details->setHeaderLabels({"Property", "Value"});
	details->setRootIsDecorated(false);
	details->setAlternatingRowColors(true);
	details->setSelectionMode(QAbstractItemView::NoSelection);
	details->setFocusPolicy(Qt::NoFocus);
	details->setItemDelegate(new Theme::SectionDelegate(details));
	details->header()->setSectionResizeMode(0, QHeaderView::Interactive);
	details->setColumnWidth(0, 230);
	details->header()->setStretchLastSection(true);
	panelLayout->addWidget(details, 1);

	auto *splitter = new QSplitter(Qt::Horizontal, this);
	splitter->addWidget(deviceTree);
	splitter->addWidget(panel);
	splitter->setStretchFactor(0, 0);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({300, 700});
	splitter->setChildrenCollapsible(false);
	layout->addWidget(splitter, 1);

	// Start with the CPU selected
	if (root->childCount() > 0)
		deviceTree->setCurrentItem(root->child(0));
}

SummaryTab::StatCard SummaryTab::makeCard(const QString &title, Category category, QWidget *parent, QGridLayout *grid, int column)
{
	auto *card = new QFrame(parent);
	card->setObjectName("Card");
	auto *v = new QVBoxLayout(card);
	v->setContentsMargins(14, 12, 14, 12);
	v->setSpacing(4);

	auto *top = new QHBoxLayout();
	top->setSpacing(8);
	auto *icon = new QLabel(card);
	icon->setPixmap(Theme::categoryIcon(category).pixmap(18, 18));
	auto *label = new QLabel(title, card);
	label->setObjectName("CardTitle");
	top->addWidget(icon);
	top->addWidget(label, 1);
	v->addLayout(top);

	StatCard c;
	c.value = new QLabel("—", card);
	c.value->setObjectName("CardValue");
	c.sub = new QLabel(" ", card);
	c.sub->setObjectName("CardSub");
	c.bar = new QProgressBar(card);
	c.bar->setRange(0, 1000);
	c.bar->setValue(0);
	c.bar->setTextVisible(false);
	c.bar->setFixedHeight(4);
	c.bar->setStyleSheet(QString("QProgressBar::chunk { background: %1; }").arg(Theme::categoryColour(category).name()));

	v->addWidget(c.value);
	v->addWidget(c.sub);
	v->addSpacing(4);
	v->addWidget(c.bar);

	grid->addWidget(card, 0, column);
	grid->setColumnStretch(column, 1);
	return c;
}

void SummaryTab::showDevice(int index)
{
	if (index < 0 || index >= static_cast<int>(devices.size()))
		return;
	const Device &d = devices[index];

	deviceIcon->setPixmap(Theme::categoryIcon(d.category).pixmap(44, 44));
	deviceKind->setText(Theme::categoryName(d.category));
	deviceKind->setStyleSheet(QString("color: %1;").arg(Theme::categoryColour(d.category).name()));
	deviceTitle->setText(QString::fromStdString(d.title));

	// Feature pills (CPU only)
	while (QLayoutItem *item = featureGrid->takeAt(0)) {
		delete item->widget();
		delete item;
	}
	featureBox->setVisible(!d.features.empty());
	const int perRow = 9;
	for (int i = 0; i < static_cast<int>(d.features.size()); i++) {
		auto *pill = new QLabel(QString::fromStdString(d.features[i].first), featureBox);
		pill->setObjectName(d.features[i].second ? "PillOn" : "PillOff");
		pill->setAlignment(Qt::AlignCenter);
		pill->setToolTip(d.features[i].second ? "Supported" : "Not supported");
		featureGrid->addWidget(pill, i / perRow, i % perRow);
	}

	// Property table
	details->clear();
	for (const Property &p : d.properties) {
		auto *item = new QTreeWidgetItem(details);
		if (p.value.empty()) {
			// Section heading
			item->setText(0, QString::fromStdString(p.name).toUpper());
			item->setForeground(0, Theme::categoryColour(d.category));
			item->setData(0, Theme::SectionRole, true);
			item->setData(0, Theme::AccentRole, Theme::categoryColour(d.category));
			item->setSizeHint(0, QSize(0, 28));
			item->setFirstColumnSpanned(true);
		} else {
			item->setText(0, QString::fromStdString(p.name));
			item->setText(1, QString::fromStdString(p.value));
			item->setForeground(0, Theme::Muted);
			item->setSizeHint(0, QSize(0, 24));
			item->setToolTip(1, QString::fromStdString(p.value));
		}
	}
}

void SummaryTab::updateLive(const std::vector<Reading> &readings)
{
	const Reading *usage = nullptr, *avgClock = nullptr, *maxClock = nullptr, *temp = nullptr;
	const Reading *memPct = nullptr, *memUsed = nullptr, *memAvail = nullptr;
	int threads = 0;
	bool tempPreferred = false;

	for (const Reading &r : readings) {
		if (r.group == "CPU Usage") {
			if (r.key == "cpu") usage = &r;
			else threads++;
		} else if (r.group == "CPU Clocks") {
			if (r.key == "avg") avgClock = &r;
			if (r.key == "max") maxClock = &r;
		} else if (r.group == "Memory") {
			if (r.key == "load") memPct = &r;
			if (r.key == "used") memUsed = &r;
			if (r.key == "avail") memAvail = &r;
		} else if (r.category == Category::Cpu && r.unit == "°C") {
			// Prefer the package/die temperature over individual cores.
			QString label = QString::fromStdString(r.label);
			bool preferred = label.contains("Package") || label.contains("Tctl") || label.contains("Tdie");
			if (!temp || (preferred && !tempPreferred)) {
				temp = &r;
				tempPreferred = preferred;
			}
		}
	}

	if (usage) {
		cpuLoad.value->setText(fmt(usage->value, 1) + " %");
		cpuLoad.value->setStyleSheet(QString("color: %1;").arg(Theme::valueColour(usage->value, "%").name()));
		cpuLoad.sub->setText(QString::number(threads) + " threads");
		cpuLoad.bar->setValue(static_cast<int>(usage->value * 10));
	}
	if (avgClock) {
		cpuClock.value->setText(fmt(avgClock->value, 0) + " MHz");
		if (maxClock)
			cpuClock.sub->setText("Peak core " + fmt(maxClock->value, 0) + " MHz");
		cpuClock.bar->setValue(static_cast<int>(std::min(1000.0, avgClock->value / 6.0))); // 6 GHz = full bar
	} else {
		cpuClock.sub->setText("Clock speed not exposed");
	}
	if (temp) {
		cpuTemp.value->setText(fmt(temp->value, 1) + " °C");
		cpuTemp.value->setStyleSheet(QString("color: %1;").arg(Theme::valueColour(temp->value, "°C").name()));
		cpuTemp.sub->setText(QString::fromStdString(temp->label));
		cpuTemp.bar->setValue(static_cast<int>(std::min(1000.0, temp->value * 10)));
	} else {
		cpuTemp.sub->setText("No sensor found (try lm-sensors)");
	}
	if (memPct) {
		memLoad.value->setText(fmt(memPct->value, 1) + " %");
		memLoad.value->setStyleSheet(QString("color: %1;").arg(Theme::valueColour(memPct->value, "%").name()));
		if (memUsed && memAvail)
			memLoad.sub->setText(fmt(memUsed->value, 1) + " GB used of " + fmt(memUsed->value + memAvail->value, 1) + " GB");
		memLoad.bar->setValue(static_cast<int>(memPct->value * 10));
	}
}
