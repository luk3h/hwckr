#pragma once

#include "sensorreader.h" // for Category

#include <QColor>
#include <QIcon>
#include <QString>
#include <QStyledItemDelegate>

class QApplication;

// hwckr's look: dark navy panels, cyan accent, colour-coded device categories.
namespace Theme {
	// Palette
	inline const QColor Background {0x0d, 0x11, 0x17};
	inline const QColor Surface    {0x13, 0x1a, 0x23};
	inline const QColor SurfaceAlt {0x17, 0x1f, 0x2a};
	inline const QColor Raised     {0x1c, 0x26, 0x32};
	inline const QColor Border     {0x26, 0x32, 0x41};
	inline const QColor Text       {0xdc, 0xe3, 0xec};
	inline const QColor Muted      {0x84, 0x94, 0xa7};
	inline const QColor Accent     {0x36, 0xd1, 0xdc};
	inline const QColor Good       {0x3d, 0xdc, 0x84};
	inline const QColor Warm       {0xff, 0xb0, 0x20};
	inline const QColor Hot        {0xff, 0x4d, 0x5e};

	void apply(QApplication &app);

	QColor categoryColour(Category c);
	QIcon categoryIcon(Category c);
	QString categoryName(Category c);

	// Colour for a value: temperatures go cyan -> amber -> red, loads go green -> amber -> red.
	QColor valueColour(double value, const std::string &unit);

	// Item data roles shared by the tables.
	constexpr int SectionRole = Qt::UserRole + 20; // true on banded header rows
	constexpr int AccentRole = Qt::UserRole + 21;  // QColor for the header stripe

	// Draws rows marked with SectionRole as banded headers with a coloured
	// accent stripe; all other rows are drawn normally.
	class SectionDelegate : public QStyledItemDelegate {
	public:
		using QStyledItemDelegate::QStyledItemDelegate;
		void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override;
	};
}
