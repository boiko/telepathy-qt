/*
 * This file is part of TelepathyQt
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <TelepathyQt/OutgoingDBusTubeChannel>

#include "TelepathyQt/_gen/outgoing-dbus-tube-channel.moc.hpp"

#include "TelepathyQt/debug-internal.h"

#include <TelepathyQt/Connection>
#include <TelepathyQt/ContactManager>
#include <TelepathyQt/PendingDBusTubeConnection>
#include <TelepathyQt/PendingString>
#include <TelepathyQt/Types>

namespace Tp
{

struct TP_QT_NO_EXPORT OutgoingDBusTubeChannel::Private
{
    Private(OutgoingDBusTubeChannel *parent);
    virtual ~Private();

    // Public object
    OutgoingDBusTubeChannel *parent;
};

OutgoingDBusTubeChannel::Private::Private(OutgoingDBusTubeChannel *parent)
        : parent(parent)
{
}

OutgoingDBusTubeChannel::Private::~Private()
{
}

/**
 * \class OutgoingDBusTubeChannel
 * \headerfile TelepathyQt/outgoing-dbus-tube-channel.h <TelepathyQt/OutgoingDBusTubeChannel>
 *
 * An high level wrapper for managing an outgoing DBus tube
 *
 * \c OutgoingDBusTubeChannel is an high level wrapper for managing Telepathy interface
 * #TELEPATHY_INTERFACE_CHANNEL_TYPE_DBUS_TUBE.
 * In particular, this class is meant to be used as a comfortable way for exposing new tubes.
 *
 * \section outgoing_dbus_tube_usage_sec Usage
 *
 * \subsection outgoing_dbus_tube_create_sec Creating an outgoing DBus tube
 *
 * The easiest way to create a DBus tube is through Account. One can
 * just use the Account convenience methods such as
 * Account::createDBusTube() to get a brand new DBus tube channel ready to be used.
 *
 * To create such a channel, it is required to pass Account::createDBusTube()
 * the contact identifier and the DBus service name which will be used over the tube.
 * For example:
 *
 * \code
 * AccountPtr myaccount = getMyAccountSomewhere();
 * ContactPtr myfriend = getMyFriendSomewhereElse();
 *
 * PendingChannelRequest *tube = myaccount->createDBusTube(myfriend, "org.my.service");
 * \endcode
 *
 * \subsection outgoing_dbus_tube_offer_sec Offering the tube
 *
 * Before being ready to offer the tube, we must be sure the required features on our object
 * are ready. In this case, we need to enable TubeChannel::FeatureTube
 * and DBusTubeChannel::FeatureDBusTube.
 *
 * DBusTubeChannel features can be enabled by constructing a ChannelFactory and enabling the desired features,
 * and passing it to ChannelRequest or ClientRegistrar when creating them as appropriate. However,
 * if a particular feature is only ever used in a specific circumstance, such as an user opening
 * some settings dialog separate from the general view of the application, features can be later
 * enabled as needed by calling becomeReady().
 *
 * You can also enable DBusTubeChannel::FeatureBusNameMonitoring to monitor connections
 * to the tube.
 *
 * Once your object is ready, you can use offerTube to create a brand new DBus connection and offer
 * it over the tube.
 *
 * You can now monitor the returned operation to know when the tube will be ready.
 * It is guaranteed that when the operation finishes,
 * the tube will be already opened and ready to be used.
 *
 * See \ref async_model, \ref shared_ptr
 */

/**
 * Create a new OutgoingDBusTubeChannel channel.
 *
 * \param connection Connection owning this channel, and specifying the
 *                   service.
 * \param objectPath The object path of this channel.
 * \param immutableProperties The immutable properties of this channel.
 * \return A OutgoingDBusTubeChannelPtr object pointing to the newly created
 *         OutgoingDBusTubeChannel object.
 */
OutgoingDBusTubeChannelPtr OutgoingDBusTubeChannel::create(const ConnectionPtr &connection,
        const QString &objectPath, const QVariantMap &immutableProperties)
{
    return OutgoingDBusTubeChannelPtr(new OutgoingDBusTubeChannel(connection, objectPath,
                immutableProperties));
}

/**
 * Construct a new OutgoingDBusTubeChannel object.
 *
 * \param connection Connection owning this channel, and specifying the
 *                   service.
 * \param objectPath The object path of this channel.
 * \param immutableProperties The immutable properties of this channel.
 */
OutgoingDBusTubeChannel::OutgoingDBusTubeChannel(const ConnectionPtr &connection,
        const QString &objectPath,
        const QVariantMap &immutableProperties)
    : DBusTubeChannel(connection, objectPath, immutableProperties),
      mPriv(new Private(this))
{
}

/**
 * Class destructor.
 */
OutgoingDBusTubeChannel::~OutgoingDBusTubeChannel()
{
    delete mPriv;
}

/**
 * Offer the tube
 *
 * This method sets up a private DBus connection to the channel target(s), and offers it through the tube.
 *
 * The %PendingDBusTubeConnection returned by this method will be completed as soon as the tube is
 * opened and ready to be used.
 *
 * \param parameters A dictionary of arbitrary Parameters to send with the tube offer.
 *                   The other end will receive this QVariantMap in the parameters() method
 *                   of the corresponding IncomingStreamTubeChannel.
 * \param requireCredentials Whether the server should require an SCM_CREDENTIALS message
 *                           upon connection.
 *
 * \returns A %PendingDBusTubeConnection which will finish as soon as the tube is ready to be used
 *          (hence in the Open state)
 */
PendingDBusTubeConnection *OutgoingDBusTubeChannel::offerTube(const QVariantMap &parameters,
        bool requireCredentials)
{
    SocketAccessControl accessControl = requireCredentials ?
                                        SocketAccessControlCredentials :
                                        SocketAccessControlLocalhost;

    if (!isReady(DBusTubeChannel::FeatureDBusTube)) {
        warning() << "DBusTubeChannel::FeatureDBusTube must be ready before "
            "calling offerTube";
        return new PendingDBusTubeConnection(QLatin1String(TP_QT_ERROR_NOT_AVAILABLE),
                QLatin1String("Channel not ready"), OutgoingDBusTubeChannelPtr(this));
    }

    // The tube must be not offered
    if (state() != TubeChannelStateNotOffered) {
        warning() << "You can not expose more than a bus for each DBus Tube";
        return new PendingDBusTubeConnection(QLatin1String(TP_QT_ERROR_NOT_AVAILABLE),
                QLatin1String("Channel busy"), OutgoingDBusTubeChannelPtr(this));
    }

    // Let's offer the tube
    if (requireCredentials && !supportsCredentials()) {
        warning() << "You requested an access control "
            "not supported by this channel";
        return new PendingDBusTubeConnection(QLatin1String(TP_QT_ERROR_NOT_IMPLEMENTED),
                QLatin1String("The requested access control is not supported"),
                OutgoingDBusTubeChannelPtr(this));
    }

    PendingString *ps = new PendingString(
        interface<Client::ChannelTypeDBusTubeInterface>()->Offer(
            parameters,
            accessControl),
        OutgoingDBusTubeChannelPtr(this));

    PendingDBusTubeConnection *op = new PendingDBusTubeConnection(ps, requireCredentials,
                                              0, OutgoingDBusTubeChannelPtr(this));
    return op;
}

}
