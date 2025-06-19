#pragma once

#include <QObject>

#include <veutil/qt/ve_qitem_utils.hpp>

class ModificationChecks : public QObject {
	Q_OBJECT

public:
	explicit ModificationChecks(VeQItem *modChecks, QObject *parent = 0);

	enum FsModifiedState {
		fsModifiedStateUnknown  = -1,
		fsModifiedStateClean    = 0,
		fsModifiedStateModified = 1,
	};

	enum Action {
		actionIdle,
		actionStartCheck,
		actionSystemHooksEnable,
		actionSystemHooksDisable,
	};

protected:
	void startCheck();
	void disableSystemHooks();
	void enableSystemHooks();

private:
	VeQItem *mActionItem;
	VeQItem *mModChecks;

	friend class VeQItemModificationAction;
};

class VeQItemModificationAction : public VeQItemAction {
	Q_OBJECT

public:
	VeQItemModificationAction(ModificationChecks *modChecks) : VeQItemAction(), mModChecks(modChecks) {}
	int setValue(const QVariant &var) override;

private:
	ModificationChecks *mModChecks;
};
