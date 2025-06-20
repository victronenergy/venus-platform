#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <veutil/qt/ve_qitem_utils.hpp>


/**
 * @brief The VrmTokenRegistrator class is meant a a short-lived class, while registration is going on.
 */
class VrmTokenRegistrator : public QObject
{
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(VrmTokenRegistrator)

	QNetworkAccessManager mNetworkManager;
	QSslConfiguration mSslConfig;
	const QString mVrmId;
	const QVariant mVrmPortalMode;
	QString mCurrentConfig;
	QString mBrokerUser;
	QString mBrokerPassword;
	QString mBridgeSettingsRpcTemplate;
	QString mBridgeSettingsDbusTemplate;
	bool stopping = false;
	bool quiet = false;

	static bool generateAndOrGetPassword(QString &output);
	void setupSsl();
	bool registerWithVrm();
	void writeFlashMQConfig();
	QString expandFlashMQConfigTemplate(QString str) const;
	QString getBrokerHostname() const;

private slots:
	void onNetworkRequestFinished(QNetworkReply *reply);

public:
	VrmTokenRegistrator(const QString &vrmId, const QVariant &vrmPortalMode);

	bool start();
	void stop();

signals:
	void done(bool configChanged);

};

class VeQItemMqttBridgeRegistrator: public VeQItemAction
{
	Q_OBJECT

public:
	VeQItemMqttBridgeRegistrator(VeQItem *pltService);

	int setValue(const QVariant &value) override;
	void setVrmPortalMode(const QVariant &mode);
	int check();

signals:
	void bridgeConfigChanged();

private:
	bool mRegistrationIsPending = false;
	QString mVrmId;
	QVariant mVrmPortalMode;
	QScopedPointer<VrmTokenRegistrator, QScopedPointerDeleteLater> registrator;

private slots:
	void onRegistratorDone(bool configChanged);
};
