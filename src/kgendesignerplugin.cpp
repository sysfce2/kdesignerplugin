/* Copyright (C) 2004-2005 ian reinhart geiser <geiseri@sourcextreme.com> */

#include <kconfig.h>
#include <kmacroexpander.h>
#include <kconfiggroup.h>
#include <kaboutdata.h>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QTextStream>

static const char classHeader[] =   "/**\n"
                                    "* This file was autogenerated by kgendesignerplugin. Any changes will be lost!\n"
                                    "* The generated code in this file is licensed under the same license that the\n"
                                    "* input file.\n"
                                    "*/\n"
                                    "#include <QIcon>\n"
                                    "#include <QtDesigner/QDesignerContainerExtension>\n"
                                    "#if QT_VERSION >= 0x050500\n"
                                    "# include <QtUiPlugin/QDesignerCustomWidgetInterface>\n"
                                    "#else\n"
                                    "# include <QDesignerCustomWidgetInterface>\n"
                                    "#endif\n"
                                    "#include <qplugin.h>\n"
                                    "#include <qdebug.h>\n";

static const char collClassDef[] = "class %CollName : public QObject, public QDesignerCustomWidgetCollectionInterface\n"
                                   "{\n"
                                   "    Q_OBJECT\n"
                                   "    Q_INTERFACES(QDesignerCustomWidgetCollectionInterface)\n"
                                   "       Q_PLUGIN_METADATA(IID \"org.qt-project.Qt.QDesignerCustomWidgetInterface\")\n"
                                   "public:\n"
                                   "    %CollName(QObject *parent = nullptr);\n"
                                   "    virtual ~%CollName() {}\n"
                                   "    QList<QDesignerCustomWidgetInterface*> customWidgets() const Q_DECL_OVERRIDE { return m_plugins; } \n"
                                   "    \n"
                                   "private:\n"
                                   "    QList<QDesignerCustomWidgetInterface*> m_plugins;\n"
                                   "};\n\n"
                                   ;

static const char collClassImpl[] = "%CollName::%CollName(QObject *parent)\n"
                                    "   : QObject(parent)"
                                    "{\n"
                                    "%CollectionAdd\n"
                                    "}\n\n";

static const char classDef[] =  "class %PluginName : public QObject, public QDesignerCustomWidgetInterface\n"
                                "{\n"
                                "       Q_OBJECT\n"
                                "       Q_INTERFACES(QDesignerCustomWidgetInterface)\n"
                                "public:\n"
                                "       %PluginName(QObject *parent = nullptr) :\n            QObject(parent), mInitialized(false) {}\n"
                                "       virtual ~%PluginName() {}\n"
                                "       \n"
                                "       bool isContainer() const Q_DECL_OVERRIDE { return %IsContainer; }\n"
                                "       bool isInitialized() const Q_DECL_OVERRIDE { return mInitialized; }\n"
                                "       QIcon icon() const Q_DECL_OVERRIDE { return QIcon(QStringLiteral(\"%IconName\")); }\n"
                                "       QString codeTemplate() const Q_DECL_OVERRIDE { return QStringLiteral(\"%CodeTemplate\"); }\n"
                                "       QString domXml() const Q_DECL_OVERRIDE { return %DomXml; }\n"
                                "       QString group() const Q_DECL_OVERRIDE { return QStringLiteral(\"%Group\"); }\n"
                                "       QString includeFile() const Q_DECL_OVERRIDE { return QStringLiteral(\"%IncludeFile\"); }\n"
                                "       QString name() const Q_DECL_OVERRIDE { return QStringLiteral(\"%Class\"); }\n"
                                "       QString toolTip() const Q_DECL_OVERRIDE { return QStringLiteral(\"%ToolTip\"); }\n"
                                "       QString whatsThis() const Q_DECL_OVERRIDE { return QStringLiteral(\"%WhatsThis\"); }\n\n"
                                "       QWidget* createWidget( QWidget* parent ) Q_DECL_OVERRIDE \n       {%CreateWidget\n       }\n"
                                "       void initialize(QDesignerFormEditorInterface *core) Q_DECL_OVERRIDE \n       {%Initialize\n       }\n"
                                "\n"
                                "private:\n"
                                "       bool mInitialized;\n"
                                "};\n\n";

static QString denamespace(const QString &str);
static QString buildCollClass(KConfig &input, const QStringList &classes, const QString &group);
static QString buildWidgetClass(const QString &name, KConfig &input, const QString &group);
static QString buildWidgetInclude(const QString &name, KConfig &input);
static void buildFile(QTextStream &stream, const QString &group, const QString &fileName, const QString &pluginName);

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QString description = QCoreApplication::translate("main", "Builds Qt widget plugins from an ini style description file.");
    const QString version = QStringLiteral("0.5");
    app.setApplicationVersion(version);

    QCommandLineParser parser;
    parser.addVersionOption();
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("file"), QCoreApplication::translate("main",  "Input file."));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("o"), QCoreApplication::translate("main", "Output file."), QStringLiteral("file")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("n"), QCoreApplication::translate("main", "Name of the plugin class to generate (deprecated, use PluginName in the input file)."), QStringLiteral("name"), QStringLiteral("WidgetsPlugin")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("g"), QCoreApplication::translate("main", "Default widget group name to display in designer (deprecated, use DefaultGroup in the input file)."), QStringLiteral("group"), QStringLiteral("Custom")));

    KAboutData about(QStringLiteral("kgendesignerplugin"),
            QCoreApplication::translate("kgendesignerplugin about data", "kgendesignerplugin"),
            version,
            description,
            KAboutLicense::GPL,
            QCoreApplication::translate("kgendesignerplugin about data", "(C) 2004-2005 Ian Reinhart Geiser"),
            QString(),
            QString(),
            QStringLiteral("geiseri@kde.org"));
    about.addAuthor(QCoreApplication::translate("kgendesignerplugin about data", "Ian Reinhart Geiser"), QString(), QStringLiteral("geiseri@kde.org"));
    about.addAuthor(QCoreApplication::translate("kgendesignerplugin about data", "Daniel Molkentin"), QString(), QStringLiteral("molkentin@kde.org"));
    about.setupCommandLine(&parser);

    parser.process(app);
    about.processCommandLine(&parser);
    if (parser.positionalArguments().count() < 1) {
        parser.showHelp();
        return 1;
    }

    QFileInfo fi(parser.positionalArguments().at(0));

    QString outputFile = parser.value(QStringLiteral("o"));
    QString pluginName = parser.value(QStringLiteral("n"));
    QString group = parser.value(QStringLiteral("g"));
    QString fileName = fi.absoluteFilePath();

    if (parser.isSet(QStringLiteral("o"))) {
        QFile output(outputFile);
        if (output.open(QIODevice::WriteOnly)) {
            QTextStream ts(&output);
            buildFile(ts, group, fileName, pluginName);
            QString mocFile = output.fileName();
            mocFile.replace(QStringLiteral(".cpp"), QStringLiteral(".moc"));
            ts << QStringLiteral("#include <%1>\n").arg(mocFile) << '\n';
        }
        output.close();
    } else {
        QTextStream ts(stdout, QIODevice::WriteOnly);
        buildFile(ts, group, fileName, pluginName);
    }
}

void buildFile(QTextStream &ts, const QString &group, const QString &fileName, const QString &pluginName)
{
    KConfig input(fileName, KConfig::NoGlobals);
    KConfigGroup cg(&input, "Global");
    ts << classHeader << '\n';

    QString defaultGroup = cg.readEntry("DefaultGroup", group);
    QStringList includes = cg.readEntry("Includes", QStringList());
    QStringList classes = input.groupList();
    classes.removeAll(QStringLiteral("Global"));

    for (const QString &myInclude : std::as_const(classes)) {
        includes += buildWidgetInclude(myInclude, input);
    }

    for (const QString &myInclude : std::as_const(includes)) {
        ts << "#include <" << myInclude << ">\n";
    }

    ts << QLatin1String("\n\n");

    // Autogenerate widget defs here
    for (const QString &myClass : std::as_const(classes)) {
        ts << buildWidgetClass(myClass, input, defaultGroup) << "\n";
    }

    ts << buildCollClass(input, classes, pluginName);
    ts.flush();

}

QString denamespace(const QString &str)
{
    QString denamespaced = str;
    denamespaced.remove(QStringLiteral("::"));
    return denamespaced;
}

QString buildCollClass(KConfig &_input, const QStringList &classes, const QString &pluginName)
{
    KConfigGroup input(&_input, "Global");
    QHash<QString, QString> defMap;
    const QString collName = input.readEntry("PluginName", pluginName);
    Q_ASSERT(!collName.isEmpty());
    defMap.insert(QStringLiteral("CollName"), collName);
    QString genCode;

    for (const QString &myClass : classes) {
        genCode += QStringLiteral("                m_plugins.append( new %1(this) );\n").arg(denamespace(myClass) + QStringLiteral("Plugin"));
    }

    defMap.insert(QStringLiteral("CollectionAdd"), genCode);

    QString str = KMacroExpander::expandMacros(QLatin1String(collClassDef), defMap);
    str += KMacroExpander::expandMacros(QLatin1String(collClassImpl), defMap);
    return str;
}

QString buildWidgetClass(const QString &name, KConfig &_input, const QString &group)
{
    KConfigGroup input(&_input, name);
    QHash<QString, QString> defMap;

    defMap.insert(QStringLiteral("Group"), input.readEntry("Group", group).replace(QLatin1Char('\"'), QStringLiteral("\\\"")));
    defMap.insert(QStringLiteral("IncludeFile"), input.readEntry("IncludeFile", QString(name.toLower() + QStringLiteral(".h"))).remove(QLatin1Char(':')));
    defMap.insert(QStringLiteral("ToolTip"), input.readEntry("ToolTip", QString(name + QStringLiteral(" Widget"))).replace(QLatin1Char('\"'), QStringLiteral("\\\"")));
    defMap.insert(QStringLiteral("WhatsThis"), input.readEntry("WhatsThis", QString(name + QStringLiteral(" Widget"))).replace(QLatin1Char('\"'), QStringLiteral("\\\"")));
    defMap.insert(QStringLiteral("IsContainer"), input.readEntry("IsContainer", QStringLiteral("false")));
    defMap.insert(QStringLiteral("IconName"), input.readEntry("IconName", QString::fromLatin1(":/pics/%1.png").arg(denamespace(name).toLower())));
    defMap.insert(QStringLiteral("Class"), name);
    defMap.insert(QStringLiteral("PluginName"), denamespace(name) + QLatin1String("Plugin"));

    // FIXME: ### make this more useful, i.e. outsource to separate file
    QString domXml = input.readEntry("DomXML", QString());
    // If domXml is empty then we should call base class function
    if (domXml.isEmpty()) {
        domXml = QStringLiteral("QDesignerCustomWidgetInterface::domXml()");
    } else {
        domXml = QStringLiteral("QStringLiteral(\"%1\")").arg(domXml.replace(QLatin1Char('\"'), QStringLiteral("\\\"")));
    }
    defMap.insert(QStringLiteral("DomXml"), domXml);
    defMap.insert(QStringLiteral("CodeTemplate"), input.readEntry("CodeTemplate"));
    defMap.insert(QStringLiteral("CreateWidget"), input.readEntry("CreateWidget",
                  QStringLiteral("\n            return new %1%2;")
                  .arg(input.readEntry("ImplClass", name))
                  .arg(input.readEntry("ConstructorArgs", "( parent )"))));
    defMap.insert(QStringLiteral("Initialize"), input.readEntry("Initialize", "\n            Q_UNUSED(core);\n            if (mInitialized) return;\n            mInitialized=true;"));

    QString code = KMacroExpander::expandMacros(QLatin1String(classDef), defMap);
    return code.replace(QLatin1String("QStringLiteral(\"\")"), QLatin1String("QString()"));
}

QString buildWidgetInclude(const QString &name, KConfig &_input)
{
    KConfigGroup input(&_input, name);
    return input.readEntry("IncludeFile", QString(name.toLower() + QStringLiteral(".h")));
}
