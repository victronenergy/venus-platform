#pragma once

#include <QObject>
#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>
#include <connman/cmmanager.h>

class VeQItemScan : public VeQItemAction {
	Q_OBJECT

public:
	VeQItemScan(CmManager *manager) :
		VeQItemAction(), mConnman(manager)
	{}

	int setValue(const QVariant &value) override;

private:
	CmManager *mConnman;
};

class VeQItemJson: public VeQItemAction {
	Q_OBJECT

public:
	VeQItemJson() :
		VeQItemAction(){}

	int setValue(const QVariant &value) override;

signals:
	void jsonParsed(const QJsonDocument &doc);
};

class VeQItemGatewayEnabled : public VeQItemQuantity {
	Q_OBJECT

public:
	VeQItemGatewayEnabled(CmTechnology *tech);
	int setValue(const QVariant &value) override;

private slots:
	void updateGatewayEnabled();

private:
	CmTechnology *mTech;
};

class NetworkController : public QObject
{
	Q_OBJECT
public:
	explicit NetworkController(VeQItem *parentItem, QObject *parent = 0);

private slots:
	void handleCommand(const QJsonDocument &doc);
	void buildServicesList();
	void updateLinkLocal();
	void updateWifiState();
	void onServiceAdded(const QString &);
	void onServiceRemoved(const QString &);
	void updateWifiSignalStrength();

private:
	QString getState(const QString &state);
	QString getLinkLocalAddr();
	void setServiceProperties(CmService *service, const QVariantMap &data);
	void setIpConfiguration(CmService *service, QVariant var);
	void setIpv4Property(CmService *service, QString name, QVariant var);
	void setDnsServer(CmService *service, QVariant var);
	void connectServiceSignals(CmService *service);

	CmManager *mConnman;
	CmService *mWifiService;
	CmService *mEthernetService;
	CmAgent *mAgent;
	VeQItem *mItem;
};
