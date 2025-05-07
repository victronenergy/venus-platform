#include <QTimer>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>

#include "application.hpp"
#include "tank_backup.hpp"
#include "venus_services.hpp"

TankBackupService::TankBackupService(VeQItem *parentItem, QObject *parent) :
	QObject(parent)
{
	qDebug() << "[Tank Backup] Service constructor";

	mTankBackupItem = parentItem->itemGetOrCreate("BackupRestore/Tank");

	mInfoItem = mTankBackupItem->itemGetOrCreate("Info");
	mErrorItem = mTankBackupItem->itemGetOrCreate("Error");
	mNotifyItem = mTankBackupItem->itemGetOrCreate("Notify");
	mActionItem = mTankBackupItem->itemGetOrCreate("Action");

	mActionItem->produceValue(0);
	mActionItem->getValueAndChanges(this, SLOT(onActionChanged(QVariant)));

}

void TankBackupService::onActionChanged(QVariant var)
{
	qDebug() << "[Tank Backup] action changed to:" << var;

	int action;

	/*
	0 - idle
	1 - create USB
	2 - backup
	3 - restore
	4 - delete
	5 - create USB successful
	6 - backup successful
	7 - restore successful
	8 - delete successful
	*/
	QThread::sleep(5);

	if (var.isValid()) {
		action = var.toInt();

		switch (action) {
			case Action::actionIdle:
				break;
			case Action::actionCreateUsb:
				runCreateUsbAction();
				break;
			case Action::actionBackup:
				runBackupAction();
				break;
			case Action::actionRestore:
				runRestoreAction();
				break;
			case Action::actionDelete:
				runDeleteAction();
				break;
			default:
				break;
		}
	}
}

void TankBackupService::onCreateUsbFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	qDebug() << "Tank backup - Create USB finished";
}

void TankBackupService::runCreateUsbAction()
{
	qDebug() << "Tank backup - Run create USB";
	mActionItem->setValue(Action::actionCreateUsbSuccessful);
	QThread::sleep(5);
	mActionItem->setValue(Action::actionIdle);
	return;
}

void TankBackupService::onBackupFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	qDebug() << "Tank backup - Backup finished";
}

void TankBackupService::runBackupAction()
{
	qDebug() << "Tank backup - Run backup";
	mActionItem->setValue(Action::actionBackupSuccessful);
	QThread::sleep(5);
	mActionItem->setValue(Action::actionIdle);
	return;
}


void TankBackupService::onRestoreFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	qDebug() << "Tank backup - Restore finished";
}

void TankBackupService::runRestoreAction()
{
	qDebug() << "Tank backup - Run restore";
	mActionItem->produceValue(Action::actionRestoreSuccessful);
	QThread::sleep(5);
	mActionItem->produceValue(Action::actionIdle);
	return;
}

void TankBackupService::onDeleteFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	qDebug() << "Tank backup - Delete backup";
	mActionItem->produceValue(Action::actionDeleteSuccessful);
	QThread::sleep(5);
	mActionItem->produceValue(Action::actionIdle);
	return;
}

void TankBackupService::runDeleteAction()
{
	qDebug() << "Tank backup - Delete backup";
	mActionItem->produceValue(Action::actionDeleteSuccessful);
	QThread::sleep(5);
	mActionItem->produceValue(Action::actionIdle);
	return;
}
