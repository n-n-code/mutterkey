#include "commanddispatch.h"
#include "config.h"

#include <QtTest/QTest>

class CommandDispatchTest final : public QObject
{
    Q_OBJECT

private slots:
    void commandIndexSkipsGlobalOptionValues();
    void bareConfigShowsDedicatedHelp();
    void configHelpFlagShowsDedicatedHelp();
    void nonConfigCommandsDoNotShowConfigHelp();
    void configHelpTextMentionsSubcommands();
    void configHelpTextListsAllSupportedConfigKeys();
};

void CommandDispatchTest::commandIndexSkipsGlobalOptionValues()
{
    const QStringList arguments{
        QStringLiteral("mutterkey"),
        QStringLiteral("--config"),
        QStringLiteral("/tmp/config.json"),
        QStringLiteral("--model-path=/tmp/model.bin"),
        QStringLiteral("config"),
        QStringLiteral("set"),
    };

    QCOMPARE(commandIndexFromArguments(arguments), 4);
}

void CommandDispatchTest::bareConfigShowsDedicatedHelp()
{
    const QStringList arguments{
        QStringLiteral("mutterkey"),
        QStringLiteral("config"),
    };

    const int commandIndex = commandIndexFromArguments(arguments);
    QVERIFY(shouldShowConfigHelp(arguments, commandIndex));
}

void CommandDispatchTest::configHelpFlagShowsDedicatedHelp()
{
    const QStringList arguments{
        QStringLiteral("mutterkey"),
        QStringLiteral("--config"),
        QStringLiteral("/tmp/config.json"),
        QStringLiteral("config"),
        QStringLiteral("--help"),
    };

    const int commandIndex = commandIndexFromArguments(arguments);
    QVERIFY(shouldShowConfigHelp(arguments, commandIndex));
}

void CommandDispatchTest::nonConfigCommandsDoNotShowConfigHelp()
{
    const QStringList arguments{
        QStringLiteral("mutterkey"),
        QStringLiteral("diagnose"),
        QStringLiteral("10"),
    };

    const int commandIndex = commandIndexFromArguments(arguments);
    QVERIFY(!shouldShowConfigHelp(arguments, commandIndex));
}

void CommandDispatchTest::configHelpTextMentionsSubcommands()
{
    const QString helpText = configHelpText();

    QVERIFY(helpText.contains(QStringLiteral("config <subcommand>")));
    QVERIFY(helpText.contains(QStringLiteral("init")));
    QVERIFY(helpText.contains(QStringLiteral("set <key> <value>")));
    QVERIFY(helpText.contains(QStringLiteral("--language <code|auto>")));
    QVERIFY(helpText.contains(QStringLiteral("transcriber.model_path")));
}

void CommandDispatchTest::configHelpTextListsAllSupportedConfigKeys()
{
    const QString helpText = configHelpText();

    for (const QString &key : supportedConfigKeys()) {
        QVERIFY2(helpText.contains(key), qPrintable(QStringLiteral("Missing help entry for %1").arg(key)));
    }
}

QTEST_APPLESS_MAIN(CommandDispatchTest)

#include "commanddispatchtest.moc"
