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
                                    "#include <QtUiPlugin/QDesignerCustomWidgetInterface>\n"
                                    "#include <qplugin.h>\n"
                                    "#include <qdebug.h>\n";

static const char collClassDef[] = "class %CollName : public QObject, public QDesignerCustomWidgetCollectionInterface\n"
                                   "{\n"
                                   "	Q_OBJECT\n"
                                   "	Q_INTERFACES(QDesignerCustomWidgetCollectionInterface)\n"
                                   "       Q_PLUGIN_METADATA(IID \"org.qt-project.Qt.QDesignerCustomWidgetInterface\")\n"
                                   "public:\n"
                                   "	%CollName(QObject *parent = 0);\n"
                                   "	virtual ~%CollName() {}\n"
                                   "	QList<QDesignerCustomWidgetInterface*> customWidgets() const { return m_plugins; } \n"
                                   "	\n"
                                   "private:\n"
                                   "	QList<QDesignerCustomWidgetInterface*> m_plugins;\n"
                                   "};\n\n"
                                   ;

static const char collClassImpl[] = "%CollName::%CollName(QObject *parent)\n"
                                    "	: QObject(parent)"
                                    "{\n"
                                    "%CollectionAdd\n"
                                    "}\n\n";

static const char classDef[] =  "class %PluginName : public QObject, public QDesignerCustomWidgetInterface\n"
                                "{\n"
                                "	Q_OBJECT\n"
                                "	Q_INTERFACES(QDesignerCustomWidgetInterface)\n"
                                "public:\n"
                                "	%PluginName(QObject *parent = 0) :\n\t\tQObject(parent), mInitialized(false) {}\n"
                                "	virtual ~%PluginName() {}\n"
                                "	\n"
                                "	bool isContainer() const { return %IsContainer; }\n"
                                "	bool isInitialized() const { return mInitialized; }\n"
                                "	QIcon icon() const { return QIcon(QLatin1String(\"%IconName\")); }\n"
                                "	QString codeTemplate() const { return QLatin1String(\"%CodeTemplate\");}\n"
                                "	QString domXml() const { return %DomXml; }\n"
                                "	QString group() const { return QLatin1String(\"%Group\"); }\n"
                                "	QString includeFile() const { return QLatin1String(\"%IncludeFile\"); }\n"
                                "	QString name() const { return QLatin1String(\"%Class\"); }\n"
                                "	QString toolTip() const { return QLatin1String(\"%ToolTip\"); }\n"
                                "	QString whatsThis() const { return QLatin1String(\"%WhatsThis\"); }\n\n"
                                "	QWidget* createWidget( QWidget* parent ) \n\t{%CreateWidget\n\t}\n"
                                "	void initialize(QDesignerFormEditorInterface *core) \n\t{%Initialize\n\t}\n"
                                "\n"
                                "private:\n"
                                "	bool mInitialized;\n"
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
    const char version[] = "0.5";
    app.setApplicationVersion(version);

    QCommandLineParser parser;
    parser.addVersionOption();
    parser.addHelpOption();
    parser.addPositionalArgument("file", QCoreApplication::translate("main",  "Input file."));
    parser.addOption(QCommandLineOption(QStringList() << "o", QCoreApplication::translate("main", "Output file."), "file"));
    parser.addOption(QCommandLineOption(QStringList() << "n", QCoreApplication::translate("main", "Name of the plugin class to generate (deprecated, use PluginName in the input file)."), "name", "WidgetsPlugin"));
    parser.addOption(QCommandLineOption(QStringList() << "g", QCoreApplication::translate("main", "Default widget group name to display in designer (deprecated, use DefaultGroup in the input file)."), "group", "Custom"));

    KAboutData about("kgendesignerplugin",
            QCoreApplication::translate("kgendesignerplugin about data", "kgendesignerplugin"),
            version,
            description,
            KAboutLicense::GPL,
            QCoreApplication::translate("kgendesignerplugin about data", "(C) 2004-2005 Ian Reinhart Geiser"),
            QString(),
            0,
            "geiseri@kde.org");
    about.addAuthor(QCoreApplication::translate("kgendesignerplugin about data", "Ian Reinhart Geiser"), QString(), "geiseri@kde.org");
    about.addAuthor(QCoreApplication::translate("kgendesignerplugin about data", "Daniel Molkentin"), QString(), "molkentin@kde.org");
    about.setupCommandLine(&parser);

    parser.process(app);
    about.processCommandLine(&parser);
    if (parser.positionalArguments().count() < 1) {
        parser.showHelp();
        return 1;
    }

    QFileInfo fi(parser.positionalArguments().at(0));

    QString outputFile = parser.value("o");
    QString pluginName = parser.value("n");
    QString group = parser.value("g");
    QString fileName = fi.absoluteFilePath();

    if (parser.isSet("o")) {
        QFile output(outputFile);
        if (output.open(QIODevice::WriteOnly)) {
            QTextStream ts(&output);
            buildFile(ts, group, fileName, pluginName);
            QString mocFile = output.fileName();
            mocFile.replace(".cpp", ".moc");
            ts << QString("#include <%1>\n").arg(mocFile) << endl;
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
    ts << classHeader << endl;

    QString defaultGroup = cg.readEntry("DefaultGroup", group);
    QStringList includes = cg.readEntry("Includes", QStringList());
    QStringList classes = input.groupList();
    classes.removeAll("Global");

    foreach (const QString &myInclude, classes) {
        includes += buildWidgetInclude(myInclude, input);
    }

    foreach (const QString &myInclude, includes) {
        ts << "#include <" << myInclude << ">" << endl;
    }

    ts << QLatin1String("\n\n");

    // Autogenerate widget defs here
    foreach (const QString &myClass, classes) {
        ts << buildWidgetClass(myClass, input, defaultGroup) << endl;
    }

    ts << buildCollClass(input, classes, pluginName);

}

QString denamespace(const QString &str)
{
    QString denamespaced = str;
    denamespaced.remove("::");
    return denamespaced;
}

QString buildCollClass(KConfig &_input, const QStringList &classes, const QString &pluginName)
{
    KConfigGroup input(&_input, "Global");
    QHash<QString, QString> defMap;
    const QString collName = input.readEntry("PluginName", pluginName);
    Q_ASSERT(!collName.isEmpty());
    defMap.insert("CollName", collName);
    QString genCode;

    foreach (const QString &myClass, classes) {
        genCode += QString("\t\tm_plugins.append( new %1(this) );\n").arg(denamespace(myClass) + "Plugin");
    }

    defMap.insert("CollectionAdd", genCode);

    QString str = KMacroExpander::expandMacros(collClassDef, defMap);
    str += KMacroExpander::expandMacros(collClassImpl, defMap);
    return str;
}

QString buildWidgetClass(const QString &name, KConfig &_input, const QString &group)
{
    KConfigGroup input(&_input, name);
    QHash<QString, QString> defMap;

    defMap.insert("Group", input.readEntry("Group", group).replace('\"', "\\\""));
    defMap.insert("IncludeFile", input.readEntry("IncludeFile", QString(name.toLower() + ".h")).remove(':'));
    defMap.insert("ToolTip", input.readEntry("ToolTip", QString(name + " Widget")).replace('\"', "\\\""));
    defMap.insert("WhatsThis", input.readEntry("WhatsThis", QString(name + " Widget")).replace('\"', "\\\""));
    defMap.insert("IsContainer", input.readEntry("IsContainer", "false"));
    defMap.insert("IconName", input.readEntry("IconName", QString::fromLatin1(":/pics/%1.png").arg(denamespace(name).toLower())));
    defMap.insert("Class", name);
    defMap.insert("PluginName", denamespace(name) + QLatin1String("Plugin"));

    // FIXME: ### make this more useful, i.e. outsource to separate file
    QString domXml = input.readEntry("DomXML", QString());
    // If domXml is empty then we should call base class function
    if (domXml.isEmpty()) {
        domXml = QLatin1String("QDesignerCustomWidgetInterface::domXml()");
    } else {
        // Wrap domXml value into QLatin1String
        domXml = QString(QLatin1String("QLatin1String(\"%1\")")).arg(domXml.replace('\"', "\\\""));
    }
    defMap.insert("DomXml", domXml);
    defMap.insert("CodeTemplate", input.readEntry("CodeTemplate"));
    defMap.insert("CreateWidget", input.readEntry("CreateWidget",
                  QString("\n\t\treturn new %1%2;")
                  .arg(input.readEntry("ImplClass", name))
                  .arg(input.readEntry("ConstructorArgs", "( parent )"))));
    defMap.insert("Initialize", input.readEntry("Initialize", "\n\t\tQ_UNUSED(core);\n\t\tif (mInitialized) return;\n\t\tmInitialized=true;"));

    return KMacroExpander::expandMacros(classDef, defMap);
}

QString buildWidgetInclude(const QString &name, KConfig &_input)
{
    KConfigGroup input(&_input, name);
    return input.readEntry("IncludeFile", QString(name.toLower() + ".h"));
}
