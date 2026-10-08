#include "theme.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QStyleFactory>

#include <map>

void Theme::apply(QApplication &app)
{
	app.setStyle(QStyleFactory::create("Fusion"));

	QPalette p;
	p.setColor(QPalette::Window, Background);
	p.setColor(QPalette::WindowText, Text);
	p.setColor(QPalette::Base, Surface);
	p.setColor(QPalette::AlternateBase, SurfaceAlt);
	p.setColor(QPalette::Text, Text);
	p.setColor(QPalette::Button, Raised);
	p.setColor(QPalette::ButtonText, Text);
	p.setColor(QPalette::Highlight, QColor(0x1f, 0x4e, 0x5f));
	p.setColor(QPalette::HighlightedText, Qt::white);
	p.setColor(QPalette::ToolTipBase, Raised);
	p.setColor(QPalette::ToolTipText, Text);
	p.setColor(QPalette::PlaceholderText, Muted);
	p.setColor(QPalette::Link, Accent);
	p.setColor(QPalette::Disabled, QPalette::Text, Muted);
	p.setColor(QPalette::Disabled, QPalette::ButtonText, Muted);
	app.setPalette(p);

	app.setStyleSheet(R"(
		QMainWindow, QWidget#Page { background: #0d1117; }

		/* Top bar */
		QFrame#HeaderBar {
			background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #101823, stop:1 #0d1117);
			border-bottom: 1px solid #263241;
		}
		QLabel#AppTitle { color: #dce3ec; font-size: 17px; font-weight: 700; letter-spacing: 1px; }
		QLabel#AppTitle[accent="true"] { color: #36d1dc; }
		QLabel#AppSubtitle { color: #8494a7; font-size: 11px; }

		/* Navigation buttons (Summary / Sensors) */
		QToolButton#NavButton {
			color: #8494a7; background: transparent; border: none;
			border-bottom: 2px solid transparent;
			padding: 8px 16px; font-size: 13px; font-weight: 600;
		}
		QToolButton#NavButton:hover { color: #dce3ec; }
		QToolButton#NavButton:checked { color: #36d1dc; border-bottom: 2px solid #36d1dc; }

		/* Normal buttons */
		QPushButton, QToolButton#Action {
			background: #1c2632; color: #dce3ec;
			border: 1px solid #263241; border-radius: 6px;
			padding: 5px 12px;
		}
		QPushButton:hover, QToolButton#Action:hover { border-color: #36d1dc; }
		QPushButton:pressed, QToolButton#Action:pressed { background: #13202a; }
		QPushButton:checked, QToolButton#Action:checked { background: #133a40; border-color: #36d1dc; color: #36d1dc; }
		QPushButton#Danger:checked { background: #3a1820; border-color: #ff4d5e; color: #ff4d5e; }

		QComboBox {
			background: #1c2632; color: #dce3ec;
			border: 1px solid #263241; border-radius: 6px; padding: 4px 10px;
		}
		QComboBox:hover { border-color: #36d1dc; }
		QComboBox QAbstractItemView { background: #1c2632; border: 1px solid #263241; selection-background-color: #1f4e5f; }

		QLineEdit {
			background: #131a23; color: #dce3ec;
			border: 1px solid #263241; border-radius: 6px; padding: 5px 10px;
		}
		QLineEdit:focus { border-color: #36d1dc; }

		/* Tables / trees */
		QTreeView {
			background: #131a23; alternate-background-color: #161e28;
			border: 1px solid #263241; border-radius: 8px;
			outline: 0;
		}
		QTreeView::item { padding: 3px 2px; border: none; }
		QTreeView::item:selected { background: #1f4e5f; color: #ffffff; }
		QTreeView::item:hover:!selected { background: #1a2633; }
		QHeaderView::section {
			background: #1c2632; color: #8494a7;
			border: none; border-right: 1px solid #263241; border-bottom: 1px solid #263241;
			padding: 6px 8px; font-weight: 600; text-transform: uppercase; font-size: 11px;
		}
		QHeaderView { background: #1c2632; border-top-left-radius: 8px; border-top-right-radius: 8px; }

		/* Cards */
		QFrame#Card { background: #131a23; border: 1px solid #263241; border-radius: 10px; }
		QLabel#CardTitle { color: #8494a7; font-size: 11px; font-weight: 600; }
		QLabel#CardValue { color: #dce3ec; font-size: 22px; font-weight: 700; }
		QLabel#CardSub { color: #8494a7; font-size: 11px; }
		QLabel#DeviceTitle { color: #dce3ec; font-size: 18px; font-weight: 700; }
		QLabel#DeviceKind { color: #36d1dc; font-size: 11px; font-weight: 700; letter-spacing: 1px; }
		QLabel#SectionLabel { color: #8494a7; font-size: 11px; font-weight: 700; letter-spacing: 1px; }

		/* Feature pills */
		QLabel#PillOn {
			background: #12322a; color: #3ddc84; border: 1px solid #1f6b4a;
			border-radius: 4px; padding: 3px 6px; font-size: 11px; font-weight: 700;
		}
		QLabel#PillOff {
			background: #161c24; color: #4a5666; border: 1px solid #222b37;
			border-radius: 4px; padding: 3px 6px; font-size: 11px; font-weight: 700;
		}

		/* Thin progress bars on cards */
		QProgressBar { background: #1c2632; border: none; border-radius: 2px; max-height: 4px; }
		QProgressBar::chunk { background: #36d1dc; border-radius: 2px; }

		QSplitter::handle { background: #0d1117; width: 8px; }

		QStatusBar { background: #101823; color: #8494a7; border-top: 1px solid #263241; }
		QStatusBar QLabel { color: #8494a7; padding: 0 8px; }

		QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
		QScrollBar::handle:vertical { background: #263241; border-radius: 4px; min-height: 30px; }
		QScrollBar::handle:vertical:hover { background: #36d1dc; }
		QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
		QScrollBar::handle:horizontal { background: #263241; border-radius: 4px; min-width: 30px; }
		QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
		QScrollBar::add-page, QScrollBar::sub-page { background: none; }

		QToolTip { background: #1c2632; color: #dce3ec; border: 1px solid #36d1dc; padding: 4px; }
	)");
}

QColor Theme::categoryColour(Category c)
{
	switch (c) {
	case Category::Cpu: return QColor(0x36, 0xd1, 0xdc);     // cyan
	case Category::Gpu: return QColor(0x3d, 0xdc, 0x84);     // green
	case Category::Memory: return QColor(0xa7, 0x8b, 0xfa);  // violet
	case Category::Board: return QColor(0xff, 0xb0, 0x20);   // amber
	case Category::Drive: return QColor(0x4f, 0x8c, 0xff);   // blue
	case Category::Network: return QColor(0xff, 0x6e, 0xc7); // pink
	default: return QColor(0x84, 0x94, 0xa7);                // grey
	}
}

QString Theme::categoryName(Category c)
{
	switch (c) {
	case Category::Cpu: return "PROCESSOR";
	case Category::Gpu: return "GRAPHICS";
	case Category::Memory: return "MEMORY";
	case Category::Board: return "MOTHERBOARD";
	case Category::Drive: return "STORAGE";
	case Category::Network: return "NETWORK";
	default: return "SYSTEM";
	}
}

// Small line-art icons, drawn in code so there are no image files to ship.
QIcon Theme::categoryIcon(Category c)
{
	static std::map<int, QIcon> cache;
	auto found = cache.find(static_cast<int>(c));
	if (found != cache.end())
		return found->second;

	const int S = 64; // drawn large, Qt scales it down crisply
	QPixmap pm(S, S);
	pm.fill(Qt::transparent);
	QPainter g(&pm);
	g.setRenderHint(QPainter::Antialiasing);

	QColor col = categoryColour(c);
	QColor fill = col;
	fill.setAlpha(50);

	// Rounded tile background
	g.setPen(Qt::NoPen);
	g.setBrush(fill);
	g.drawRoundedRect(QRectF(2, 2, S - 4, S - 4), 14, 14);

	QPen pen(col, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	g.setPen(pen);
	g.setBrush(Qt::NoBrush);

	switch (c) {
	case Category::Cpu: // chip with pins
		g.drawRect(QRectF(20, 20, 24, 24));
		g.drawRect(QRectF(27, 27, 10, 10));
		for (int i = 0; i < 3; i++) {
			double x = 24 + i * 8;
			g.drawLine(QPointF(x, 12), QPointF(x, 18));
			g.drawLine(QPointF(x, 46), QPointF(x, 52));
			g.drawLine(QPointF(12, x), QPointF(18, x));
			g.drawLine(QPointF(46, x), QPointF(52, x));
		}
		break;
	case Category::Gpu: // card with fan
		g.drawRoundedRect(QRectF(10, 18, 44, 26), 4, 4);
		g.drawEllipse(QPointF(26, 31), 7, 7);
		g.drawLine(QPointF(40, 26), QPointF(48, 26));
		g.drawLine(QPointF(40, 34), QPointF(48, 34));
		g.drawLine(QPointF(14, 44), QPointF(14, 50));
		g.drawLine(QPointF(30, 44), QPointF(30, 50));
		break;
	case Category::Memory: // RAM stick
		g.drawRect(QRectF(10, 20, 44, 18));
		for (int i = 0; i < 4; i++)
			g.drawRect(QRectF(15 + i * 9.5, 25, 5, 8));
		for (int i = 0; i < 6; i++)
			g.drawLine(QPointF(14 + i * 7, 38), QPointF(14 + i * 7, 46));
		break;
	case Category::Board: { // board with traces
		g.drawRoundedRect(QRectF(12, 12, 40, 40), 4, 4);
		g.drawRect(QRectF(20, 20, 12, 12));
		QPainterPath path;
		path.moveTo(32, 26); path.lineTo(44, 26); path.lineTo(44, 34);
		path.moveTo(26, 32); path.lineTo(26, 44); path.lineTo(36, 44);
		g.drawPath(path);
		break;
	}
	case Category::Drive: // drive with activity light
		g.drawRoundedRect(QRectF(10, 20, 44, 24), 5, 5);
		g.drawLine(QPointF(16, 36), QPointF(36, 36));
		g.setBrush(col);
		g.drawEllipse(QPointF(46, 32), 2.5, 2.5);
		break;
	case Category::Network: // wifi arcs
		g.drawArc(QRectF(12, 16, 40, 40), 45 * 16, 90 * 16);
		g.drawArc(QRectF(20, 24, 24, 24), 45 * 16, 90 * 16);
		g.setBrush(col);
		g.drawEllipse(QPointF(32, 44), 3, 3);
		break;
	default: // monitor
		g.drawRoundedRect(QRectF(12, 16, 40, 26), 3, 3);
		g.drawLine(QPointF(32, 42), QPointF(32, 48));
		g.drawLine(QPointF(24, 48), QPointF(40, 48));
		break;
	}
	g.end();

	QIcon icon(pm);
	cache[static_cast<int>(c)] = icon;
	return icon;
}

void Theme::SectionDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const
{
	if (!idx.data(SectionRole).toBool()) {
		QStyledItemDelegate::paint(p, opt, idx);
		return;
	}

	QColor accent = idx.data(AccentRole).value<QColor>();
	if (!accent.isValid())
		accent = Accent;

	p->save();
	p->setRenderHint(QPainter::Antialiasing);

	// Band background with a soft tint of the accent colour
	QRect r = opt.rect;
	QLinearGradient band(r.topLeft(), r.topRight());
	QColor tint = accent;
	tint.setAlpha(38);
	band.setColorAt(0, tint);
	band.setColorAt(0.6, Raised);
	band.setColorAt(1, Raised);
	p->fillRect(r, band);
	p->fillRect(QRect(r.left(), r.top(), 3, r.height()), accent);
	p->setPen(Border);
	p->drawLine(r.bottomLeft(), r.bottomRight());

	// Icon + text
	int x = r.left() + 10;
	QIcon icon = idx.data(Qt::DecorationRole).value<QIcon>();
	if (!icon.isNull()) {
		int s = 18;
		icon.paint(p, QRect(x, r.top() + (r.height() - s) / 2, s, s));
		x += s + 8;
	}
	QFont f = opt.font;
	QVariant fontData = idx.data(Qt::FontRole);
	if (fontData.isValid())
		f = fontData.value<QFont>();
	f.setBold(true);
	p->setFont(f);
	QVariant fg = idx.data(Qt::ForegroundRole);
	p->setPen(fg.isValid() ? fg.value<QBrush>().color() : Text);
	p->drawText(QRect(x, r.top(), r.right() - x - 8, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
	            idx.data(Qt::DisplayRole).toString());
	p->restore();
}

QColor Theme::valueColour(double v, const std::string &unit)
{
	if (unit == "°C") {
		if (v >= 85.0) return Hot;
		if (v >= 70.0) return Warm;
		return Text;
	}
	if (unit == "%") {
		if (v >= 90.0) return Hot;
		if (v >= 70.0) return Warm;
		return Text;
	}
	return Text;
}
