#include "netlink_monitor.hpp"

#include <QDebug>

#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

NetlinkMonitor::NetlinkMonitor(QObject *parent)
	: QObject(parent)
{
	mFd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (mFd < 0)
		qFatal("Failed to create netlink socket");

	sockaddr_nl sa {};
	sa.nl_family = AF_NETLINK;
	// ipv6 is disabled for now, since it sends periodic updates and isn't needed
	// for the linklocal.
	sa.nl_groups = RTMGRP_IPV4_IFADDR /* | RTMGRP_IPV6_IFADDR */;

	if (bind(mFd, reinterpret_cast<sockaddr *>(&sa), sizeof(sa)) < 0)
		qFatal("Failed to bind netlink socket");

	mNotifier = new QSocketNotifier(mFd, QSocketNotifier::Read, this);
	connect(mNotifier, &QSocketNotifier::activated,
			this, &NetlinkMonitor::readNetlink);
}

NetlinkMonitor::~NetlinkMonitor()
{
	if (mFd >= 0)
		close(mFd);
}

void NetlinkMonitor::readNetlink()
{
	char buf[8192];

	ssize_t len = recv(mFd, buf, sizeof(buf), 0);
	if (len <= 0)
		return;

	for (nlmsghdr *nlh = reinterpret_cast<nlmsghdr *>(buf);
		 NLMSG_OK(nlh, len);
		 nlh = NLMSG_NEXT(nlh, len)) {

		if (nlh->nlmsg_type == NLMSG_DONE)
			break;

		if (nlh->nlmsg_type == NLMSG_ERROR) {
			qWarning() << "Netlink error";
			continue;
		}

		if (nlh->nlmsg_type == RTM_NEWADDR || nlh->nlmsg_type == RTM_DELADDR)
			handleAddrMsg(nlh);
	}
}

void NetlinkMonitor::handleAddrMsg(struct nlmsghdr *nlh)
{
	auto *ifa = reinterpret_cast<ifaddrmsg *>(NLMSG_DATA(nlh));
	int len = nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa));

	char ifname[IF_NAMESIZE] {};
	if_indextoname(ifa->ifa_index, ifname);

	for (rtattr *rta = IFA_RTA(ifa); RTA_OK(rta, len);
		 rta = RTA_NEXT(rta, len)) {

		if (rta->rta_type != IFA_ADDRESS) //  && rta->rta_type != IFA_LOCAL
			continue;

		char addrbuf[INET6_ADDRSTRLEN] {};
		void *addr = RTA_DATA(rta);

		if (ifa->ifa_family == AF_INET)
			inet_ntop(AF_INET, addr, addrbuf, sizeof(addrbuf));
		else if (ifa->ifa_family == AF_INET6)
			inet_ntop(AF_INET6, addr, addrbuf, sizeof(addrbuf));
		else
			continue;

		QString iface = QString::fromLatin1(ifname);
		QString address = QString::fromLatin1(addrbuf);

		if (nlh->nlmsg_type == RTM_NEWADDR)
			emit addressAdded(iface, address);
		else if (nlh->nlmsg_type == RTM_DELADDR)
			emit addressRemoved(iface, address);
	}
}

bool NetlinkMonitor::requestInitialAddresses()
{
	struct {
		nlmsghdr nlh;
		ifaddrmsg ifa;
	} req {};

	req.nlh.nlmsg_len   = NLMSG_LENGTH(sizeof(ifaddrmsg));
	req.nlh.nlmsg_type  = RTM_GETADDR;
	req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req.nlh.nlmsg_seq   = 1;
	req.nlh.nlmsg_pid   = getpid();

	req.ifa.ifa_family = AF_UNSPEC; // IPv4 + IPv6

	ssize_t ret = send(mFd, &req, req.nlh.nlmsg_len, 0);
	return ret >= 0;
}
