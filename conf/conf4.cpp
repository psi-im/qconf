/*
Copyright (C) 2004-2008  Justin Karneges

This file is free software; unlimited permission is given to copy and/or
distribute it, with or without modifications, as long as this notice is
preserved.
*/

#include "conf4.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef PATH_MAX
#ifdef Q_OS_WIN
#define PATH_MAX 260
#endif
#endif

class MocTestObject : public QObject {

    Q_OBJECT
public:
    MocTestObject() {}
};

QString qc_getenv(const QString &var)
{
    char *p = ::getenv(var.toLatin1().data());
    if (!p)
        return QString();
    return QString(p);
}

QStringList qc_pathlist()
{
    QStringList list;
    QString     path = qc_getenv("PATH");
    if (!path.isEmpty()) {
#if QT_VERSION >= 0x060000
        Qt::SplitBehavior flags = Qt::SkipEmptyParts;
#else
        QString::SplitBehavior flags = QString::SkipEmptyParts;
#endif
#ifdef Q_OS_WIN
        list = path.split(';', flags);
#else
        list = path.split(':', flags);
#endif
    }
#ifdef Q_OS_WIN
    list.prepend(".");
#endif
    return list;
}

QString qc_findprogram(const QString &prog)
{
    QString     out;
    QStringList list = qc_pathlist();
    for (int n = 0; n < list.count(); ++n) {
        QFileInfo fi(list[n] + '/' + prog);
        if (fi.exists() && fi.isExecutable()) {
            out = fi.filePath();
            break;
        }

#ifdef Q_OS_WIN
        // on windows, be sure to look for .exe
        if (prog.right(4).toLower() != ".exe") {
            fi = QFileInfo(list[n] + '/' + prog + ".exe");
            if (fi.exists() && fi.isExecutable()) {
                out = fi.filePath();
                break;
            }
        }
#endif
    }
    return out;
}

QString qc_findself(const QString &argv0)
{
#ifdef Q_OS_WIN
    if (argv0.contains('\\'))
#else
    if (argv0.contains('/'))
#endif
        return argv0;
    else
        return qc_findprogram(argv0);
}

int qc_run_program_or_command(const QString &prog, const QStringList &args, const QString &command, QByteArray *out,
                              bool showOutput)
{
    if (out)
        out->clear();

    QProcess process;
    process.setReadChannel(QProcess::StandardOutput);

    if (!prog.isEmpty())
        process.start(prog, args);
    else if (!command.isEmpty())
        process.start(command);
    else
        return -1;

    if (!process.waitForStarted(-1))
        return -1;

    QByteArray buf;

    while (process.waitForReadyRead(-1)) {
        buf = process.readAllStandardOutput();
        if (out)
            out->append(buf);
        if (showOutput)
            fprintf(stdout, "%s", buf.data());

        buf = process.readAllStandardError();
        if (showOutput)
            fprintf(stderr, "%s", buf.data());
    }

    buf = process.readAllStandardError();
    if (showOutput)
        fprintf(stderr, "%s", buf.data());

    // calling waitForReadyRead will cause the process to eventually be
    //   marked as finished, so we should not need to separately call
    //   waitForFinished. however, we will do it anyway just to be safe.
    //   we won't check the return value since false could still mean
    //   success (if the process had already been marked as finished).
    process.waitForFinished(-1);

    if (process.exitStatus() != QProcess::NormalExit)
        return -1;

    return process.exitCode();
}

int qc_runcommand(const QString &command, QByteArray *out, bool showOutput)
{
    return qc_run_program_or_command(QString(), QStringList(), command, out, showOutput);
}

int qc_runprogram(const QString &prog, const QStringList &args, QByteArray *out, bool showOutput)
{
    return qc_run_program_or_command(prog, args, QString(), out, showOutput);
}

bool qc_removedir(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists())
        return false;
    QStringList list = dir.entryList();
    foreach (QString s, list) {
        if (s == "." || s == "..")
            continue;
        QFileInfo fi(dir.filePath(s));
        if (fi.isDir()) {
            if (!qc_removedir(fi.filePath()))
                return false;
        } else {
            if (!dir.remove(s))
                return false;
        }
    }
    QString dirName = dir.dirName();
    if (!dir.cdUp())
        return false;
    if (!dir.rmdir(dirName))
        return false;
    return true;
}

// simple command line arguemnts splitter able to understand quoted args.
// the splitter removes quotes and unescapes symbols as well.
QStringList qc_splitflags(const QString &flags)
{
    QStringList ret;
    bool        searchStart = true;
    bool        inQuotes    = false;
    bool        escaped     = false;
    QChar       quote, backslash = QLatin1Char('\\');
    QString     buf;
#ifdef PATH_MAX
    buf.reserve(PATH_MAX);
#endif
    for (int i = 0; i < flags.length(); i++) {
        if (searchStart && flags[i].isSpace()) {
            continue;
        }
        if (searchStart) {
            searchStart = false;
            buf.clear();
        }
        if (escaped) {
            buf += flags[i];
            escaped = false;
            continue;
        }
        // buf += flags[i];
        if (inQuotes) {
            if (quote == QLatin1Char('\'')) {
                if (flags[i] == quote) {
                    inQuotes = false;
                    continue;
                }
            } else { // we are in double quoetes
                if (i < flags.length() - 1 && flags[i] == backslash
                    && (flags[i + 1] == QLatin1Char('"') || flags[i + 1] == backslash)) {
                    // if next symbol is one of in parentheses ("\)
                    escaped = true;
                    continue;
                }
            }
        } else {
            if (flags[i].isSpace()) {
                ret.append(buf);
                searchStart = true;
                buf.clear();
                continue;
#ifndef Q_OS_WIN /* on windows backslash is just a path separator */
            } else if (flags[i] == backslash) {
                escaped = true;
                continue; // just add next symbol
#endif
            } else if (flags[i] == QLatin1Char('\'') || flags[i] == QLatin1Char('"')) {
                inQuotes = true;
                quote    = flags[i];
                continue;
            }
        }
        buf += flags[i];
    }
    if (buf.size()) {
        ret.append(buf);
    }
    return ret;
}

void qc_splitcflags(const QString &cflags, QStringList *incs, QStringList *otherflags)
{
    incs->clear();
    otherflags->clear();

    QStringList cflagsList = qc_splitflags(cflags);
    for (int n = 0; n < cflagsList.count(); ++n) {
        QString str = cflagsList[n];
        if (str.startsWith("-I")) {
            // we want everything except the leading "-I"
            incs->append(str.remove(0, 2));
        } else {
            // we want whatever is left
            otherflags->append(str);
        }
    }
}

QString qc_escapeArg(const QString &str)
{
    QString out;
    for (int n = 0; n < (int)str.length(); ++n) {
        if (str[n] == '-')
            out += '_';
        else
            out += str[n];
    }
    return out;
}

QString qc_trim_char(const QString &s, const QChar &ch)
{
    if (s.startsWith(ch) && s.endsWith(ch)) {
        return s.mid(1, s.size() - 2);
    }
    return s;
}

// removes surrounding quotes, removes trailing slashes, converts to native separators.
// accepts unescaped but possible quoted path
QString qc_normalize_path(const QString &str)
{
    QString path = str.trimmed();
    path         = qc_trim_char(path, QLatin1Char('"'));
    path         = qc_trim_char(path, QLatin1Char('\''));

    // It's OK to use unix style'/' paths on windows Qt handles this without any problems.
    // Using Windows-style '\\' can leads strange compilation error with MSYS which uses
    // unix style.
    QLatin1Char nativeSep('/');
#ifdef Q_OS_WIN
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
#endif
    // trim trailing slashes
    while (path.length() && path[path.length() - 1] == nativeSep) {
        path.resize(path.length() - 1);
    }
    return path;
}

// escape filesystem path to be added to qmake pro/pri file.
QString qc_escape_string_var(const QString &str)
{
    QString path = str;
    path.replace(QLatin1Char('\\'), QLatin1String("\\\\")).replace(QLatin1Char('"'), QLatin1String("\\\""));
    if (path.indexOf(QLatin1Char(' ')) != -1) { // has spaces
        return QLatin1Char('"') + path + QLatin1Char('"');
    }
    return path;
}

// escapes each path in incs and join into single string suiable for INCLUDEPATH var
QString qc_prepare_includepath(const QStringList &incs)
{
    if (incs.empty()) {
        return QString();
    }
    QStringList ret;
    foreach (const QString &path, incs) {
        ret.append(qc_escape_string_var(path));
    }
    return ret.join(QLatin1String(" "));
}

// escapes each path in libs and to make it suiable for LIBS var
// notice, entries of libs are every single arg for linker.
QString qc_prepare_libs(const QStringList &libs)
{
    if (libs.isEmpty()) {
        return QString();
    }
    QSet<QString> pathSet;
    QStringList   paths;
    QStringList   ordered;
    foreach (const QString &arg, libs) {
        if (arg.startsWith(QLatin1String("-L"))) {
            QString path = qc_escape_string_var(arg.mid(2));
            if (!pathSet.contains(path)) {
                pathSet.insert(path);
                paths.append(path);
            }
        } else if (arg.startsWith(QLatin1String("-l"))) {
            ordered.append(arg);
        } else {
            ordered.append(qc_escape_string_var(arg));
        }
    }
    QString ret;
    if (paths.size()) {
        ret += (QLatin1String(" -L") + paths.join(QLatin1String(" -L")) + QLatin1Char(' '));
    }
    return ret + ordered.join(QLatin1String(" "));
}

//----------------------------------------------------------------------------
// ConfObj
//----------------------------------------------------------------------------
ConfObj::ConfObj(Conf *c)
{
    conf = c;
    conf->added(this);
    required = false;
    disabled = false;
    success  = false;
}

ConfObj::~ConfObj() {}

QString ConfObj::checkString() const { return QString("Checking for %1 ...").arg(name()); }

QString ConfObj::resultString() const
{
    if (success)
        return "yes";
    else
        return "no";
}

//----------------------------------------------------------------------------
// qc_internal_pkgconfig
//----------------------------------------------------------------------------
class qc_internal_pkgconfig : public ConfObj {
public:
    QString     pkgname, desc;
    VersionMode mode;
    QString     req_ver;

    qc_internal_pkgconfig(Conf *c, const QString &_name, const QString &_desc, VersionMode _mode,
                          const QString &_req_ver) :
        ConfObj(c),
        pkgname(_name), desc(_desc), mode(_mode), req_ver(_req_ver)
    {
    }

    QString name() const { return desc; }
    QString shortname() const { return pkgname; }

    bool exec()
    {
        QStringList incs;
        QString     version, libs, other;
        if (!conf->findPkgConfig(pkgname, mode, req_ver, &version, &incs, &libs, &other))
            return false;

        for (int n = 0; n < incs.count(); ++n)
            conf->addIncludePath(incs[n]);
        if (!libs.isEmpty())
            conf->addLib(libs);
        // if(!other.isEmpty())
        //	conf->addExtra(QString("QMAKE_CFLAGS += %1\n").arg(other));

        if (!required)
            conf->addDefine("HAVE_PKG_" + qc_escapeArg(pkgname).toUpper());

        return true;
    }
};

//----------------------------------------------------------------------------
// Conf
//----------------------------------------------------------------------------
Conf::Conf()
{
    // TODO: no more vars?
    // vars.insert("QMAKE_INCDIR_X11", new QString(X11_INC));
    // vars.insert("QMAKE_LIBDIR_X11", new QString(X11_LIBDIR));
    // vars.insert("QMAKE_LIBS_X11",   new QString(X11_LIB));
    // vars.insert("QMAKE_CC", CC);

    debug_enabled = false;
    first_debug   = true;
}

Conf::~Conf() { qDeleteAll(list); }

void Conf::added(ConfObj *o) { list.append(o); }

QString Conf::getenv(const QString &var) { return qc_getenv(var); }

void Conf::debug(const QString &s)
{
    if (debug_enabled) {
        if (first_debug)
            printf("\n");
        first_debug = false;
        printf(" * %s\n", qPrintable(s));
    }
}

bool Conf::exec()
{
    for (int n = 0; n < list.count(); ++n) {
        ConfObj *o = list[n];

        // if this was a disabled-by-default option, check if it was enabled
        if (o->disabled) {
            QString v = QString("QC_ENABLE_") + qc_escapeArg(o->shortname());
            if (getenv(v) != "Y")
                continue;
        }
        // and the opposite?
        else {
            QString v = QString("QC_DISABLE_") + qc_escapeArg(o->shortname());
            if (getenv(v) == "Y")
                continue;
        }

        bool    output = true;
        QString check  = o->checkString();
        if (check.isEmpty())
            output = false;

        if (output) {
            printf("%s", check.toLatin1().data());
            fflush(stdout);
        }

        first_debug = true;
        bool ok     = o->exec();
        o->success  = ok;

        if (output) {
            QString result = o->resultString();
            if (!first_debug)
                printf(" -> %s\n", result.toLatin1().data());
            else
                printf(" %s\n", result.toLatin1().data());
        }

        if (!ok && o->required) {
            printf("\nError: need %s!\n", o->name().toLatin1().data());
            return false;
        }
    }
    return true;
}

QString Conf::qvar(const QString &s) { return vars.value(s); }

QString Conf::normalizePath(const QString &s) const { return qc_normalize_path(s); }

QString Conf::escapeQmakeVar(const QString &s) const { return qc_escape_string_var(s); }

QString Conf::escapedIncludes() const { return qc_prepare_includepath(INCLUDEPATH); }

QString Conf::escapedLibs() const { return qc_prepare_libs(LIBS); }

QString Conf::expandIncludes(const QString &inc) { return QLatin1String("-I") + inc; }

QString Conf::expandLibs(const QString &lib) { return QLatin1String("-L") + lib; }

int Conf::doCommand(const QString &s, QByteArray *out)
{
    debug(QString("[%1]").arg(s));
    int r = qc_runcommand(s, out, debug_enabled);
    debug(QString("returned: %1").arg(r));
    return r;
}

int Conf::doCommand(const QString &prog, const QStringList &args, QByteArray *out)
{
    QString fullcmd = prog;
    QString argstr  = args.join(QLatin1String(" "));
    if (!argstr.isEmpty())
        fullcmd += QString(" ") + argstr;
    debug(QString("[%1]").arg(fullcmd));
    int r = qc_runprogram(prog, args, out, debug_enabled);
    debug(QString("returned: %1").arg(r));
    return r;
}

bool Conf::doCompileAndLink(const QString &filedata, const QStringList &incs, const QString &libs,
                            const QString &proextra, int *retcode)
{
#ifdef Q_OS_WIN
    QDir tmp("qconftemp");
#else
    QDir tmp(".qconftemp");
#endif
    QStringList normalizedLibs;
    foreach (const QString &l, qc_splitflags(libs)) {
        normalizedLibs.append(qc_normalize_path(l));
    }

    if (!tmp.mkdir("atest")) {
        debug(QString("unable to create atest dir: %1").arg(tmp.absoluteFilePath("atest")));
        return false;
    }
    QDir dir(tmp.filePath("atest"));
    if (!dir.exists()) {
        debug("atest dir does not exist");
        return false;
    }

    QString fname = dir.filePath("atest.cpp");
    QString out   = "atest";
    QFile   f(fname);
    if (!f.open(QFile::WriteOnly | QFile::Truncate)) {
        debug("unable to open atest.cpp for writing");
        return false;
    }
    if (f.write(filedata.toLatin1()) == -1) {
        debug("error writing to atest.cpp");
        return false;
    }
    f.close();

    debug(QString("Wrote atest.cpp:\n%1").arg(filedata));

    QString pro = QString("CONFIG  += console\n"
                          "CONFIG  -= qt app_bundle\n"
                          "DESTDIR  = $$PWD\n"
                          "SOURCES += atest.cpp\n");
    QString inc = qc_prepare_includepath(incs);
    if (!inc.isEmpty())
        pro += "INCLUDEPATH += " + inc + '\n';
    QString escaped_libs = qc_prepare_libs(normalizedLibs);
    if (!escaped_libs.isEmpty())
        pro += "LIBS += " + escaped_libs + '\n';
    pro += proextra;

    fname = dir.filePath("atest.pro");
    f.setFileName(fname);
    if (!f.open(QFile::WriteOnly | QFile::Truncate)) {
        debug("unable to open atest.pro for writing");
        return false;
    }
    if (f.write(pro.toLatin1()) == -1) {
        debug("error writing to atest.pro");
        return false;
    }
    f.close();

    debug(QString("Wrote atest.pro:\n%1").arg(pro));

    QString oldpath = QDir::currentPath();
    QDir::setCurrent(dir.path());

    bool ok = false;
    int  r  = doCommand(qmake_path, QStringList() << "atest.pro");
    if (r == 0) {
        r = doCommand(maketool, QStringList());
        if (r == 0) {
            ok = true;
            if (retcode) {
                QString runatest = out;
#ifdef Q_OS_UNIX
                runatest.prepend("./");
#endif
                *retcode = doCommand(runatest, QStringList());
            }
        }
        r = doCommand(maketool, QStringList() << "distclean");
        if (r != 0)
            debug("error during atest distclean");
    }

    QDir::setCurrent(oldpath);

    // cleanup
    // dir.remove("atest.pro");
    // dir.remove("atest.cpp");
    // tmp.rmdir("atest");

    // remove whole dir since distclean doesn't always work
    qc_removedir(tmp.filePath("atest"));

    if (!ok)
        return false;
    return true;
}

bool Conf::checkHeader(const QString &path, const QString &h) { return QDir(path).exists(h); }

bool Conf::findHeader(const QString &h, const QStringList &ext, QString *inc)
{
    if (checkHeader("/usr/include", h)) {
        *inc = "";
        return true;
    }
    QStringList dirs;
    dirs += "/usr/local/include";
    dirs += ext;

    QString prefix = qc_getenv("PREFIX");
    if (!prefix.isEmpty()) {
        prefix += "/include";
        prefix = qc_normalize_path(prefix);
        if (!dirs.contains(prefix))
            dirs << prefix;
    }

    for (QStringList::ConstIterator it = dirs.begin(); it != dirs.end(); ++it) {
        if (checkHeader(*it, h)) {
            *inc = *it;
            return true;
        }
    }
    return false;
}

bool Conf::checkLibrary(const QString &path, const QString &name)
{
    QString str =
        //"#include <stdio.h>\n"
        "int main()\n"
        "{\n"
        //"    printf(\"library checker running\\\\n\");\n"
        "    return 0;\n"
        "}\n";

    QString libs;
    if (!path.isEmpty())
        libs += QString("-L") + path + ' ';
    libs += QString("-l") + name;
    if (!doCompileAndLink(str, QStringList(), libs, QString()))
        return false;
    return true;
}

bool Conf::findLibrary(const QString &name, QString *lib)
{
    if (checkLibrary("", name)) {
        *lib = "";
        return true;
    }
    if (checkLibrary("/usr/local/lib", name)) {
        *lib = "/usr/local/lib";
        return true;
    }

    QString prefix = qc_getenv("PREFIX");
    if (!prefix.isEmpty()) {
        prefix += "/lib";
        prefix = qc_normalize_path(prefix);
        if (checkLibrary(prefix, name)) {
            *lib = prefix;
            return true;
        }
    }

    return false;
}

QString Conf::findProgram(const QString &prog) { return qc_findprogram(prog); }

bool Conf::findSimpleLibrary(const QString &incvar, const QString &libvar, const QString &incname,
                             const QString &libname, QString *incpath, QString *libs)
{
    QString inc, lib;
    QString s;

    s = getenv(incvar).trimmed();
    if (!s.isEmpty()) {
        if (!checkHeader(s, incname)) {
            if (debug_enabled)
                printf("%s is not found in \"%s\"\n", qPrintable(incname), qPrintable(s));
            return false;
        }
        inc = s;
    } else {
        if (!findHeader(incname, QStringList(), &s)) {
            if (debug_enabled)
                printf("%s is not found anywhere\n", qPrintable(incname));
            return false;
        }
        inc = s;
    }

    s = getenv(libvar).trimmed();
    if (!s.isEmpty()) {
        if (!checkLibrary(s, libname)) {
            if (debug_enabled)
                printf("%s is not found in \"%s\"\n", qPrintable(libname), qPrintable(s));
            return false;
        }
        lib = s;
    } else {
        if (!findLibrary(libname, &s)) {
            if (debug_enabled)
                printf("%s is not found anywhere\n", qPrintable(libname));
            return false;
        }
        lib = s;
    }

    QString lib_out;
    if (!lib.isEmpty())
        lib_out += QString("-L") + s + " ";
    lib_out += QString("-l") + libname;

    *incpath = inc;
    *libs    = lib_out;
    return true;
}

bool Conf::findFooConfig(const QString &path, QString *version, QStringList *incs, QString *libs, QString *otherflags)
{
    QStringList args;
    QByteArray  out;
    int         ret;

    args += "--version";
    ret = doCommand(path, args, &out);
    if (ret != 0)
        return false;

    QString version_out = QString::fromLatin1(out).trimmed();

    args.clear();
    args += "--libs";
    ret = doCommand(path, args, &out);
    if (ret != 0)
        return false;

    QString libs_out = QString::fromLatin1(out).trimmed();

    args.clear();
    args += "--cflags";
    ret = doCommand(path, args, &out);
    if (ret != 0)
        return false;

    QString cflags = QString::fromLatin1(out).trimmed();

    QStringList incs_out, otherflags_out;
    qc_splitcflags(cflags, &incs_out, &otherflags_out);

    *version    = version_out;
    *incs       = incs_out;
    *libs       = libs_out;
    *otherflags = otherflags_out.join(QLatin1String(" "));
    return true;
}

bool Conf::findPkgConfig(const QString &name, VersionMode mode, const QString &req_version, QString *version,
                         QStringList *incs, QString *libs, QString *otherflags)
{
    QStringList args;
    QByteArray  out;
    int         ret;

    args += name;
    args += "--exists";
    ret = doCommand("pkg-config", args, &out);
    if (ret != 0)
        return false;

    if (mode != VersionAny) {
        args.clear();
        args += name;
        if (mode == VersionMin)
            args += QString("--atleast-version=%1").arg(req_version);
        else if (mode == VersionMax)
            args += QString("--max-version=%1").arg(req_version);
        else
            args += QString("--exact-version=%1").arg(req_version);
        ret = doCommand("pkg-config", args, &out);
        if (ret != 0)
            return false;
    }

    args.clear();
    args += name;
    args += "--modversion";
    ret = doCommand("pkg-config", args, &out);
    if (ret != 0)
        return false;

    QString version_out = QString::fromLatin1(out).trimmed();

    args.clear();
    args += name;
    args += "--libs";
    ret = doCommand("pkg-config", args, &out);
    if (ret != 0)
        return false;

    QString libs_out = QString::fromLatin1(out).trimmed();

    args.clear();
    args += name;
    args += "--cflags";
    ret = doCommand("pkg-config", args, &out);
    if (ret != 0)
        return false;

    QString cflags = QString::fromLatin1(out).trimmed();

    QStringList incs_out, otherflags_out;
    qc_splitcflags(cflags, &incs_out, &otherflags_out);

    *version    = version_out;
    *incs       = incs_out;
    *libs       = libs_out;
    *otherflags = otherflags_out.join(QLatin1String(" "));
    return true;
}

void Conf::addDefine(const QString &str)
{
    if (DEFINES.isEmpty())
        DEFINES = str;
    else
        DEFINES += QString(" ") + str;
    debug(QString("DEFINES += %1").arg(str));
}

void Conf::addLib(const QString &str)
{
    QStringList libs = qc_splitflags(str);
    foreach (const QString &lib, libs) {
        if (lib.startsWith("-l")) {
            LIBS.append(lib);
        } else {
            LIBS.append(qc_normalize_path(lib)); // we don't care about -L prefix since normalier does not touch it.
        }
    }
    debug(QString("LIBS += %1").arg(str));
}

void Conf::addIncludePath(const QString &str)
{
    INCLUDEPATH.append(qc_normalize_path(str));
    debug(QString("INCLUDEPATH += %1").arg(str));
}

void Conf::addExtra(const QString &str)
{
    extra += str + '\n';
    debug(QString("extra += %1").arg(str));
}

//----------------------------------------------------------------------------
// main
//----------------------------------------------------------------------------
#include "conf4.moc"

#ifdef HAVE_MODULES
#include "modules.cpp"
#endif

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Conf *           conf = new Conf;
    ConfObj *        o    = 0;
    Q_UNUSED(o);
#ifdef HAVE_MODULES
#include "modules_new.cpp"
#endif

    conf->debug_enabled = (qc_getenv("QC_VERBOSE") == "Y") ? true : false;
    if (conf->debug_enabled)
        printf(" -> ok\n");
    else
        printf("ok\n");

    QString confCommand = qc_getenv("QC_COMMAND");
    QString proName     = qc_getenv("QC_PROFILE");
    conf->qmake_path    = qc_getenv("QC_QMAKE");
    conf->qmakespec     = qc_getenv("QC_QMAKESPEC");
    conf->maketool      = qc_getenv("QC_MAKETOOL");

    if (conf->debug_enabled)
        printf("conf command: [%s]\n", qPrintable(confCommand));

    QString confPath = qc_findself(confCommand);
    if (confPath.isEmpty()) {
        printf("Error: cannot find myself; rerun with an absolute path\n");
        return 1;
    }

    QString srcdir   = QFileInfo(confPath).absolutePath();
    QString builddir = QDir::current().absolutePath();
    QString proPath  = QDir(srcdir).filePath(proName);

    if (conf->debug_enabled) {
        printf("conf path:    [%s]\n", qPrintable(confPath));
        printf("srcdir:       [%s]\n", qPrintable(srcdir));
        printf("builddir:     [%s]\n", qPrintable(builddir));
        printf("profile:      [%s]\n", qPrintable(proPath));
        printf("qmake path:   [%s]\n", qPrintable(conf->qmake_path));
        printf("qmakespec:    [%s]\n", qPrintable(conf->qmakespec));
        printf("make tool:    [%s]\n", qPrintable(conf->maketool));
        printf("\n");
    }

    bool success = false;
    if (conf->exec()) {
        QFile f("conf.pri");
        if (!f.open(QFile::WriteOnly | QFile::Truncate)) {
            printf("Error writing %s\n", qPrintable(f.fileName()));
            return 1;
        }

        QString str;
        str += "# qconf\n\n";
        str += "greaterThan(QT_MAJOR_VERSION, 4):CONFIG += c++11\n";

        QString var;
        var = qc_normalize_path(qc_getenv("PREFIX"));
        if (!var.isEmpty())
            str += QString("PREFIX = %1\n").arg(var);
        var = qc_normalize_path(qc_getenv("BINDIR"));
        if (!var.isEmpty())
            str += QString("BINDIR = %1\n").arg(var);
        var = qc_normalize_path(qc_getenv("INCDIR"));
        if (!var.isEmpty())
            str += QString("INCDIR = %1\n").arg(var);
        var = qc_normalize_path(qc_getenv("LIBDIR"));
        if (!var.isEmpty())
            str += QString("LIBDIR = %1\n").arg(var);
        var = qc_normalize_path(qc_getenv("DATADIR"));
        if (!var.isEmpty())
            str += QString("DATADIR = %1\n").arg(var);
        str += '\n';

        if (qc_getenv("QC_STATIC") == "Y")
            str += "CONFIG += staticlib\n";

        // TODO: don't need this?
        // str += "QT_PATH_PLUGINS = " + QString(qInstallPathPlugins()) + '\n';

        if (!conf->DEFINES.isEmpty())
            str += "DEFINES += " + conf->DEFINES + '\n';
        if (!conf->INCLUDEPATH.isEmpty())
            str += "INCLUDEPATH += " + qc_prepare_includepath(conf->INCLUDEPATH) + '\n';
        if (!conf->LIBS.isEmpty())
            str += "LIBS += " + qc_prepare_libs(conf->LIBS) + '\n';
        if (!conf->extra.isEmpty())
            str += conf->extra;
        str += '\n';

        var = qc_getenv("QC_EXTRACONF");
        if (!var.isEmpty())
            str += ("\n# Extra conf from command line\n" + var + "\n");

        QByteArray cs = str.toLatin1();
        f.write(cs);
        f.close();
        success = true;
    }
    QString qmake_path = conf->qmake_path;
    QString qmakespec  = conf->qmakespec;
    delete conf;

    if (!success)
        return 1;

    // run qmake on the project file
    QStringList args;
    if (!qmakespec.isEmpty()) {
        args += "-spec";
        args += qmakespec;
    }
    args += proPath;
    int ret = qc_runprogram(qmake_path, args, 0, true);
    if (ret != 0)
        return 1;

    return 0;
}
