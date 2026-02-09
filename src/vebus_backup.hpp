#pragma once

#include <QObject>
#include <QProcess>
#include <QVector>
#include <QPair>

#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>

#include "venus_services.hpp"
#include "vebus_backup.hpp"

class VebusBackupServiceRegistrator : public QObject
{
	Q_OBJECT

public:
	explicit VebusBackupServiceRegistrator(VeQItem *parentItem, VenusServices *services, QObject *parent = 0);

private:
	VeQItem *mParentItemRef;

private slots:
	void onVenusServiceFound(VenusService *service);
};

class VebusBackupService : public QObject
{
	Q_OBJECT

public:
	VebusBackupService(VeQItem *parentItem, VenusService *service, QObject *parent = 0);

	enum Action {
		idle = 0,
		backup,
		restore,
		fileDelete,
	};

	enum Notification {
		backupSuccessful = 1,
		restoreSuccessful = 2,
		deleteSuccessful = 3,
		backupException = 101,
		restoreException = 102,
		deleteFailed = 103,
	};


	// mk2vsc application state codes
	QVector<QPair<int, QString>> mk2VscStateLookup = {
		{10, "Init"},
		{11, "Query products"},
		{12, "Done"},
		{21, "Read setting data"},
		{22, "Read assistants"},
		{23, "Read VE.Bus configuration"},
		{24, "Read grid info"},
		{30, "Write settings info"},
		{31, "Write Settings Data"},
		{32, "Write assistants"},
		{33, "Write VE.Bus configuration"},
		{40, "Resetting VE.Bus products"},
		{41, "Waiting for password entry"}
	};

private:
	void updateAvailableBackups();
	void deleteBackupFile();
	void onBackupFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void runBackupAction();
	void onRestoreFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void runRestoreAction();
	bool getProductIdAndVersions();
	bool checkFirmwareVersionCompatibility(const QString& prodIdAndVersion, const QString& subVersion);

	VenusService *mVebusInterfaceService;

	bool mWorking;
	bool mAvailableBackupsListValid;
	bool mOffline;
	bool mInitialized;
	const QString backupDir = QStringLiteral("/data/conf/");
	const QString mk2vscProg = QStringLiteral("/opt/victronenergy/mk2vsc/mk2vsc");
	const QString mk2vscCacheDir = QStringLiteral("/tmp");

	QString mConnection;
	QString mFileName;
	QString mVebusFirmwareVersionString;
	QString mVebusFirmwareVersionNumber;
	QString mVebusFirmwareSubVersionNumber;
	QString mVebusProductId;
	QStringList mBackupFiles;
	QStringList mCompatibleBackupFiles;
	VeQItem *mVenusPlatformParentItem;
	VeQItem *mActionItem;
	VeQItem *mInfoItem;
	VeQItem *mErrorItem;
	VeQItem *mNotifyItem;
	VeQItem *mFileItem;
	VeQItem *mAvailableBackupsItem;
	VeQItem *mIncompatibleBackupsItem; // Backups from other/older firmware versions
	VeQItem *mMk2DbusMk2ConnectionItem;
	VeQItem *mMk2DbusFirmwareVersionItem;
	VeQItem *mMk2DbusSubVersionItem;
	VeQItem *mMk2DbusProductIdItem;
	VeQItem *mRestoreNotifyItem;
	VeQItem *mRestoreActionItem;
	VeQItem *mRestorePasswordInputItem;
	VeQItem *mRestorePasswordInputPendingItem;
	VeQItem *mRestorePasswordAccessLevelItem;
	VeQItem *mRestorePasswordCancelStateItem;
	VeQItem *mRestoreInfoItem;
	VeQItem *mk2VscPasswInputItem;
	VeQItem *mk2VscPasswCancelStateItem;
	VeQItem *mPasswordInputItem;
	VeQItem *mPasswordInputPendingItem;
	VeQItem *mPasswordInputCancelStateItem;
	VeQItem *mPasswordInputTimeoutCounterItem;
	VeQItem *mPasswordForAccessLevelItem;

private slots:
	//void onVenusServiceFound(VenusService *service);	// We need this to monitor mk2vsc

	void onMk2ConnectionItemChanged(QVariant var);
	void onActionChanged(QVariant var);
	void onFileNameChanged(QVariant var);
	void onFileIndexChanged(QVariant var);
	void onRestoreActionItemChanged(QVariant var);
	void onRestorePasswordInputChanged(QVariant var);
	void onPasswordInputChanged(QVariant var);
	void onPasswordCancelChanged(QVariant var);
	void onMk2VscStateChanged(QVariant var);
	void onMk2VscPasswPendingChanged(QVariant var);
	void onMk2VscPasswCancelStateChanged(QVariant var);
	void onMk2VscPasswTimeoutCounterChanged(QVariant var);
	void onMk2VscPasswAccessLevelChanged(QVariant var);

	void onVebusProductIdOrVersionChanged(QVariant var);
	void onVebusConnectedChanged(QVariant var);
};
