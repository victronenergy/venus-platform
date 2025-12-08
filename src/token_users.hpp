#pragma once

#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QTimer>

#include <veutil/qt/ve_qitem.hpp>
#include <veutil/qt/ve_qitem_utils.hpp>

class TokenUser {
public:
	explicit TokenUser() {}
	explicit TokenUser(const QString &user, const QString passwordHash)
		: mTokenName(user), mPasswordHash(passwordHash)
	{}

	QString tokenname() const { return mTokenName; }
	QString passwordHash() const { return mPasswordHash; }
	QString toString() const { return "username: " + mTokenName + " pwd:" + mPasswordHash; }
	bool isValid() const { return !mTokenName.isEmpty(); }

	static TokenUser fromJson(const QJsonObject &json);
	QJsonObject toJson(bool withHash = true) const;

private:
	QString mTokenName;
	QString mPasswordHash;
};

QDebug operator<<(QDebug debug, const TokenUser &user);

class TokenUsers {
public:
	TokenUsers() {}

	bool fromJson(const QJsonArray &users);
	QJsonArray toJson(bool withHash = true) const;
	bool load(QString const &filename);
	bool save(QString const &filename);

	int removeTokenUser(const QString &username);
	bool addTokenUser(const TokenUser &user);

private:
	QList<TokenUser> mTokenUsers;
};

class TokenUserWatcher : public QObject {
	Q_OBJECT
public:
	TokenUserWatcher(VeQItem *platform);
	~TokenUserWatcher();

	void updateTokens();

private slots:
	void scanDirectory();

private:
	QFileSystemWatcher mWatcher;
	VeQItem *mTokensItem;
	VeQItem *mPairingCountDown = nullptr;

};

class TokenRemoveItem : public VeQItemAction
{
	Q_OBJECT

public:
	explicit TokenRemoveItem(TokenUserWatcher *watcher) :
		VeQItemAction(),
		mWatcher(watcher)
	{
		produceValue("");
	}
	int setValue(const QVariant &value) override;

private:
	TokenUserWatcher *mWatcher;
};

class TokenPairingEnableItem : public VeQItemAction
{
	Q_OBJECT

	VeQItem *mCountDownItem = nullptr;
	int mPairingCountDownValue = 0;
	QTimer mPairingCountDownTimer;

	void startCountDown();

private slots:
	void onCountDownTimeout();

public:
	explicit TokenPairingEnableItem(VeQItem *countDownItem);
	int setValue(const QVariant &value) override;
};
