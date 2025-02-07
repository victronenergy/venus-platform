#pragma once

#include <QObject>

#include <veutil/qt/ve_qitem_utils.hpp>

class VeQItemModificationChecksStartCheck : public VeQItemAction {
	Q_OBJECT

public:
	VeQItemModificationChecksStartCheck(VeQItem *modChecks) : VeQItemAction(), mModChecks(modChecks) {}
	int setValue(const QVariant &value) override;

private:
	VeQItem *mModChecks;
};

class ModificationChecks : public QObject {
	Q_OBJECT

public:
	explicit ModificationChecks(VeQItem *modChecks, VeQItemSettings *settings, QObject *parent = 0);

private slots:
	void onAllModificationsEnabledChanged(QVariant var);

private:
	void disableModifications();
	void restoreModifications();

	VeQItem *mModChecks;
	VeQItemSettings *mSettings;
};
