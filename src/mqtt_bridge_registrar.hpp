#pragma once

#include <QProcess>

#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>

class VeQItemMqttBridgeRegistrar: public VeQItemAction {
	Q_OBJECT

public:
	VeQItemMqttBridgeRegistrar() :
		VeQItemAction()
	{}

	int setValue(const QVariant &value) override {
		if (mProc)
			return -1;

		mProc = new QProcess();
		connect(mProc, SIGNAL(finished(int)), this, SLOT(onFinished()));
		connect(mProc, SIGNAL(error(QProcess::ProcessError)), this, SLOT(onErrorOccurred(QProcess::ProcessError)));
		qDebug() << "[MqttBridgeRegistrar]" << "registering";
		mProc->start("mosquitto_bridge_registrator.py");

		return VeQItemAction::setValue(value);
	}

private slots:
	void onFinished() {
		qDebug() << "[MqttBridgeRegistrar]" << "done";
		mProc->deleteLater();
		mProc = nullptr;
	}

	void onErrorOccurred(QProcess::ProcessError error) {
		qDebug() << "[MqttBridgeRegistrar]" << "error during registration" << error;
		mProc->deleteLater();
		mProc = nullptr;
	}

private:
	QProcess *mProc = nullptr;
};

