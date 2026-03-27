#include "commanddispatch.h"
#include "config.h"

#include <QtTest/QTest>

namespace {

class CommandDispatchTest final : public QObject
{
    Q_OBJECT

private slots:
    void commandIndexSkipsGlobalOptionValues();
    void commandIndexTreatsDoubleDashAsCommandBoundary();
    void commandIndexSkipsInlineGlobalOptionValues();
    void bareConfigShowsDedicatedHelp();
    void configHelpFlagShowsDedicatedHelp();
    void nonConfigCommandsDoNotShowConfigHelp();
    void configHelpTextMentionsSubcommands();
    void configHelpTextListsAllSupportedConfigKeys();
    void bareModelShowsDedicatedHelp();
    void modelHelpTextMentionsSubcommands();
};

} // namespace

void CommandDispatchTest::commandIndexSkipsGlobalOptionValues()
{
    // WHAT: Verify that command discovery finds the real subcommand position.
    // HOW: Pass arguments that include global options and their values before the command,
    // then check that the returned index points at "config" instead of one of the option values.
    // WHY: Command routing depends on this index being correct, and a wrong index would make
    // valid CLI input behave like an unknown or malformed command.
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

void CommandDispatchTest::commandIndexTreatsDoubleDashAsCommandBoundary()
{
    // WHAT: Verify that `--` forces command discovery to stop parsing global options.
    // HOW: Build an argument list where `config` appears only after `--` and check that the
    // returned command index points to that post-separator command.
    // WHY: The CLI needs a predictable escape hatch for command parsing so option-like values
    // or explicit separators do not confuse dispatch.
    const QStringList arguments{
        QStringLiteral("mutterkey"),
        QStringLiteral("--log-level"),
        QStringLiteral("DEBUG"),
        QStringLiteral("--"),
        QStringLiteral("config"),
        QStringLiteral("set"),
    };

    QCOMPARE(commandIndexFromArguments(arguments), 4);
}

void CommandDispatchTest::commandIndexSkipsInlineGlobalOptionValues()
{
    // WHAT: Verify that `--option=value` global arguments do not get mistaken for commands.
    // HOW: Pass a command line that uses only inline-assignment global options before
    // `config` and confirm that command discovery still finds the real subcommand.
    // WHY: Modern CLI invocations commonly use inline option syntax, so dispatch must handle
    // it consistently with separate option-value pairs.
    const QStringList arguments{
        QStringLiteral("mutterkey"),
        QStringLiteral("--config=/tmp/config.json"),
        QStringLiteral("--log-level=DEBUG"),
        QStringLiteral("config"),
        QStringLiteral("init"),
    };

    QCOMPARE(commandIndexFromArguments(arguments), 3);
}

void CommandDispatchTest::bareConfigShowsDedicatedHelp()
{
    // WHAT: Verify that the bare "config" command opens config-specific help.
    // HOW: Build an argument list with only the top-level "config" command and assert that
    // the helper decides to show the dedicated config help text.
    // WHY: Users often start by typing a command name on its own, so this keeps the first
    // help experience clear instead of falling through to generic CLI behavior.
    const QStringList arguments{
        QStringLiteral("mutterkey"),
        QStringLiteral("config"),
    };

    const int commandIndex = commandIndexFromArguments(arguments);
    QVERIFY(shouldShowConfigHelp(arguments, commandIndex));
}

void CommandDispatchTest::configHelpFlagShowsDedicatedHelp()
{
    // WHAT: Verify that "--help" after the "config" command still selects config help.
    // HOW: Include a global option before "config", add "--help" after it, and confirm
    // the help-selection logic chooses the config-specific help path.
    // WHY: This protects a common support path where users ask the CLI for targeted help
    // while still using global overrides such as a custom config file.
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
    // WHAT: Verify that non-config commands do not accidentally trigger config help.
    // HOW: Use a normal "diagnose" invocation and assert that the helper reports that
    // config help should not be shown.
    // WHY: Help detection must stay narrow, otherwise unrelated commands would become
    // confusing or unusable because they would be redirected to the wrong documentation.
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
    // WHAT: Verify that the config help text mentions the main config workflows.
    // HOW: Read the generated help text and check that it includes the expected command
    // shapes and examples such as "init", "set", and key option hints.
    // WHY: This test protects the CLI's self-documentation, which is especially important
    // when users rely on the help text instead of external documentation.
    const QString helpText = configHelpText();

    QVERIFY(helpText.contains(QStringLiteral("config <subcommand>")));
    QVERIFY(helpText.contains(QStringLiteral("init")));
    QVERIFY(helpText.contains(QStringLiteral("set <key> <value>")));
    QVERIFY(helpText.contains(QStringLiteral("--language <code|auto>")));
    QVERIFY(helpText.contains(QStringLiteral("transcriber.model_path")));
}

void CommandDispatchTest::configHelpTextListsAllSupportedConfigKeys()
{
    // WHAT: Verify that every supported config key is listed in the help text.
    // HOW: Iterate through the canonical supported-key list and assert that each key
    // appears in the rendered help output.
    // WHY: The help text should stay aligned with the implementation so users can discover
    // every editable setting without guessing or reading the source code.
    const QString helpText = configHelpText();

    for (const QString &key : supportedConfigKeys()) {
        QVERIFY2(helpText.contains(key), qPrintable(QStringLiteral("Missing help entry for %1").arg(key)));
    }
}

void CommandDispatchTest::bareModelShowsDedicatedHelp()
{
    // WHAT: Verify that the bare "model" command opens model-specific help.
    // HOW: Build an argument list with only the top-level "model" command and assert that
    // the helper decides to show the dedicated model help text.
    // WHY: The new Phase 4 model tooling should be discoverable directly from the CLI without
    // requiring users to guess the subcommand shape or read the README first.
    const QStringList arguments{
        QStringLiteral("mutterkey"),
        QStringLiteral("model"),
    };

    const int commandIndex = commandIndexFromArguments(arguments);
    QVERIFY(shouldShowModelHelp(arguments, commandIndex));
}

void CommandDispatchTest::modelHelpTextMentionsSubcommands()
{
    // WHAT: Verify that the model help text mentions the import and inspect workflows.
    // HOW: Render the generated model help text and assert that it contains both subcommands.
    // WHY: The new native-package tooling needs clear self-documentation because it is now the
    // canonical way to migrate and inspect model artifacts.
    const QString helpText = modelHelpText();

    QVERIFY(helpText.contains(QStringLiteral("model <subcommand>")));
    QVERIFY(helpText.contains(QStringLiteral("import <raw-whisper-bin>")));
    QVERIFY(helpText.contains(QStringLiteral("inspect <path>")));
}

QTEST_APPLESS_MAIN(CommandDispatchTest)

#include "commanddispatchtest.moc"
