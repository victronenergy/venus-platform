#pragma once

#include <QObject>

#include <veutil/qt/ve_qitem_utils.hpp>

class VeQItemTime : public VeQItemExportedLeaf {
	Q_OBJECT

public:
	VeQItemTime() : VeQItemExportedLeaf() {}
	virtual ~VeQItemTime() {}

	int setValue(const QVariant &value) override;
	QVariant getValue() override;
	QString getText() override;
};
