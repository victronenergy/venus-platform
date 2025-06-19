#pragma once

#include <QObject>

#include <veutil/qt/ve_qitem_utils.hpp>

class ModificationChecks : public QObject {
	Q_OBJECT
	Q_ENUMS(FsModifiedState)

public:
	explicit ModificationChecks(VeQItem *modChecks, QObject *parent = 0);

	enum Action {
		actionIdle,
		actionStartCheck,
		actionSystemHooksEnable,
		actionSystemHooksDisable,
	};

	enum FsModifiedState {
		fsModifiedStateUnknown  = -1,
		fsModifiedStateClean    = 0,
		fsModifiedStateModified = 1,
	};

	// not yet used
	enum SystemHooksState {
		SystemHooksState_NonePresent         = 0b00000,
		SystemHooksState_RcLocalDisabled     = 0b00001,
		SystemHooksState_RcSLocalDisabled    = 0b00010,
		SystemHooksState_RcLocal             = 0b00100,
		SystemHooksState_RcSLocal            = 0b01000,
		SystemHooksState_HookLoadedAtStartup = 0b10000
	};

private slots:
	void onModificationChecksAction(QVariant var);

private:
	void startCheck();
	void disableSystemHooks();
	void enableSystemHooks();

	VeQItem *mActionItem;
	VeQItem *mModChecks;
};
