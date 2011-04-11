#include <QtCore/QDebug>
#include <QtCore/QTimer>

#include <QtDBus/QtDBus>

#include <QtTest/QtTest>

#define TP_QT4_ENABLE_LOWLEVEL_API

#include <TelepathyQt4/ChannelFactory>
#include <TelepathyQt4/Connection>
#include <TelepathyQt4/ConnectionLowlevel>
#include <TelepathyQt4/ContactFactory>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/Debug>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>

#include <tests/lib/glib/bug16307-conn.h>
#include <tests/lib/glib/simple-conn.h>
#include <tests/lib/test.h>

using namespace Tp;

class TestConnIntrospectCornercases : public Test
{
    Q_OBJECT

public:
    TestConnIntrospectCornercases(QObject *parent = 0)
        : Test(parent), mConnService(0)
    { }

protected Q_SLOTS:
    void expectConnInvalidated();

private Q_SLOTS:
    void initTestCase();
    void init();

    void testStatusChange();
    void testSlowpath();

    void cleanup();
    void cleanupTestCase();

private:
    TpBaseConnection *mConnService;
    ConnectionPtr mConn;
    QList<ConnectionStatus> mStatuses;
};

void TestConnIntrospectCornercases::expectConnInvalidated()
{
    qDebug() << "conn invalidated";

    mLoop->exit(0);
}

void TestConnIntrospectCornercases::initTestCase()
{
    initTestCaseImpl();

    g_type_init();
    g_set_prgname("conn-introspect-cornercases");
    tp_debug_set_flags("all");
    dbus_g_bus_get(DBUS_BUS_STARTER, 0);
}

void TestConnIntrospectCornercases::init()
{
    initImpl();

    QVERIFY(mConn.isNull());
    QVERIFY(mConnService == 0);

    QVERIFY(mStatuses.empty());

    // don't create the client- or service-side connection objects here, as it's expected that many
    // different types of service connections with different initial states need to be used
}

void TestConnIntrospectCornercases::testSlowpath()
{
    gchar *name;
    gchar *connPath;
    GError *error = 0;

    TpTestsBug16307Connection *bugConnService =
        TP_TESTS_BUG16307_CONNECTION(
            g_object_new(
                TP_TESTS_TYPE_BUG16307_CONNECTION,
                "account", "me@example.com",
                "protocol", "simple",
                NULL));
    QVERIFY(bugConnService != 0);

    mConnService = TP_BASE_CONNECTION(bugConnService);
    QVERIFY(mConnService != 0);

    QVERIFY(tp_base_connection_register(mConnService, "simple",
                &name, &connPath, &error));
    QVERIFY(error == 0);

    mConn = Connection::create(QLatin1String(name), QLatin1String(connPath),
            ChannelFactory::create(QDBusConnection::sessionBus()),
            ContactFactory::create());
    QCOMPARE(mConn->isReady(), false);

    g_free(name); name = 0;
    g_free(connPath); connPath = 0;

    PendingOperation *op = mConn->becomeReady();
    QVERIFY(connect(op,
                    SIGNAL(finished(Tp::PendingOperation*)),
                    SLOT(expectSuccessfulCall(Tp::PendingOperation*))));

    tp_tests_bug16307_connection_inject_get_status_return(bugConnService);

    QCOMPARE(mLoop->exec(), 0);
    QCOMPARE(op->isFinished(), true);
    QCOMPARE(mConn->isReady(Connection::FeatureCore), true);
    QCOMPARE(static_cast<uint>(mConn->status()),
             static_cast<uint>(ConnectionStatusConnected));
}

void TestConnIntrospectCornercases::testStatusChange()
{
    gchar *name;
    gchar *connPath;
    GError *error = 0;

    TpTestsSimpleConnection *simpleConnService =
        TP_TESTS_SIMPLE_CONNECTION(
            g_object_new(
                TP_TESTS_TYPE_SIMPLE_CONNECTION,
                "account", "me@example.com",
                "protocol", "simple",
                NULL));
    QVERIFY(simpleConnService != 0);

    mConnService = TP_BASE_CONNECTION(simpleConnService);
    QVERIFY(mConnService != 0);

    QVERIFY(tp_base_connection_register(mConnService, "simple",
                &name, &connPath, &error));
    QVERIFY(error == 0);

    mConn = Connection::create(QLatin1String(name), QLatin1String(connPath),
            ChannelFactory::create(QDBusConnection::sessionBus()),
            ContactFactory::create());
    QCOMPARE(mConn->isReady(), false);

    g_free(name); name = 0;
    g_free(connPath); connPath = 0;

    // Make core ready first, because Connection has internal handling for the status changing
    // during core introspection, and we rather want to test the more general ReadinessHelper
    // mechanism

    PendingOperation *op = mConn->becomeReady();
    QVERIFY(connect(op,
                    SIGNAL(finished(Tp::PendingOperation*)),
                    SLOT(expectSuccessfulCall(Tp::PendingOperation*))));

    QCOMPARE(mLoop->exec(), 0);
    QCOMPARE(op->isFinished(), true);
    QCOMPARE(mConn->isReady(Connection::FeatureCore), true);
    QCOMPARE(static_cast<uint>(mConn->status()),
             static_cast<uint>(ConnectionStatusDisconnected));

    // Now, begin making Connected ready

    op = mConn->becomeReady(Connection::FeatureConnected);
    QVERIFY(connect(op,
                    SIGNAL(finished(Tp::PendingOperation*)),
                    SLOT(expectSuccessfulCall(Tp::PendingOperation*))));

    mLoop->processEvents();

    // But disturb it by changing the status!

    TpHandleRepoIface *contact_repo = tp_base_connection_get_handles(mConnService,
            TP_HANDLE_TYPE_CONTACT);

    mConnService->self_handle = tp_handle_ensure(contact_repo, "me@example.com",
            NULL, NULL);

    tp_base_connection_change_status(mConnService,
            TP_CONNECTION_STATUS_CONNECTING,
            TP_CONNECTION_STATUS_REASON_REQUESTED);

    // Do that again! (The earlier op stil hasn't finished by definition)
    mConn->becomeReady(Features() << Connection::FeatureConnected);

    tp_base_connection_change_status(mConnService,
            TP_CONNECTION_STATUS_CONNECTED,
            TP_CONNECTION_STATUS_REASON_REQUESTED);

    QCOMPARE(mLoop->exec(), 0);
    QCOMPARE(op->isFinished(), true);
    QCOMPARE(mConn->isReady(Connection::FeatureCore), true);
    QCOMPARE(mConn->isReady(Connection::FeatureConnected), true);
    QCOMPARE(static_cast<uint>(mConn->status()),
             static_cast<uint>(ConnectionStatusConnected));
}

void TestConnIntrospectCornercases::cleanup()
{
    if (mConn) {
        QVERIFY(mConnService != 0);

        // Disconnect and wait for invalidation
        tp_base_connection_change_status(
                mConnService,
                TP_CONNECTION_STATUS_DISCONNECTED,
                TP_CONNECTION_STATUS_REASON_REQUESTED);

        QVERIFY(connect(mConn.data(),
                    SIGNAL(invalidated(Tp::DBusProxy *,
                            const QString &, const QString &)),
                    SLOT(expectConnInvalidated())));
        QCOMPARE(mLoop->exec(), 0);
        QVERIFY(!mConn->isValid());

        processDBusQueue(mConn.data());

        mConn.reset();
    }

    if (mConnService != 0) {
        g_object_unref(mConnService);
        mConnService = 0;
    }

    mStatuses.clear();

    cleanupImpl();
}

void TestConnIntrospectCornercases::cleanupTestCase()
{
    cleanupTestCaseImpl();
}

QTEST_MAIN(TestConnIntrospectCornercases)
#include "_gen/conn-introspect-cornercases.cpp.moc.hpp"