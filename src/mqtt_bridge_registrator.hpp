#pragma once

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>

#include <veutil/qt/ve_qitem_utils.hpp>

#include "application.hpp"

class VeQItemMqttBridgeRegistrator: public VeQItemAction {
	Q_OBJECT

public:
	VeQItemMqttBridgeRegistrator(VeQItem *pltService);

	int setValue(const QVariant &value) override;
	int check();

signals:
	void bridgeConfigChanged();

private slots:
	void onRequestFinished(QNetworkReply *reply);

private:
	void setupSsl();
	void registerWithVrm();

	QNetworkAccessManager *mNetworkManager;
	QSslConfiguration mSslConfig;
	VeQItem *mPltService;
};
