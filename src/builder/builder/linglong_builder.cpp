/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "linglong_builder.h"

#include <csignal>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#include <QCoreApplication>
#include <QUrl>
#include <QDir>
#include <QTemporaryFile>
#include <QThread>

#include "module/util/yaml.h"
#include "module/util/uuid.h"
#include "module/util/xdg.h"
#include "module/runtime/oci.h"
#include "module/runtime/container.h"
#include "module/repo/ostree.h"

#include "project.h"
#include "source_fetcher.h"
#include "builder_config.h"
#include "depend_fetcher.h"
#include "module/util/desktop_entry.h"
#include "module/util/sysinfo.h"
#include "module/runtime/app.h"

namespace linglong {
namespace builder {

util::Error commitBuildOutput(Project *project, AnnotationsOverlayfsRootfs *overlayfs)
{
    auto output = project->config().cacheInstallPath("files");
    ensureDir(output);

    auto upperDir = QStringList {overlayfs->upper, project->config().targetInstallPath("")}.join("/");
    ensureDir(upperDir);

    auto lowerDir = project->config().cacheAbsoluteFilePath({"overlayfs", "lower"});
    // if combine runtime, lower is ${PROJECT_CACHE}/runtime/files
    if (PackageKindRuntime == project->package->kind) {
        lowerDir = project->config().cacheAbsoluteFilePath({"runtime", "files"});
    }
    ensureDir(lowerDir);

    QProcess fuseOverlayfs;
    fuseOverlayfs.setProgram("fuse-overlayfs");
    fuseOverlayfs.setArguments({
        "-f",
        "-o",
        "auto_unmount",
        "-o",
        "upperdir=" + upperDir,
        "-o",
        "workdir=" + overlayfs->workdir,
        "-o",
        "lowerdir=" + lowerDir,
        output,
    });

    qint64 fuseOverlayfsPid = -1;
    fuseOverlayfs.startDetached(&fuseOverlayfsPid);
    fuseOverlayfs.waitForStarted(-1);

    // FIXME: must wait fuse mount filesystem
    QThread::sleep(1);

    // write entries
    QDir applicationsDir(output + "/share/applications");
    auto desktopFileInfoList = applicationsDir.entryInfoList({"*.desktop"}, QDir::Files);
    qDebug() << "found" << desktopFileInfoList << "in" << applicationsDir;

    // replace desktop to entries
    auto entriesPath = project->config().cacheInstallPath("entries");
    ensureDir(entriesPath);

    auto targetPath = QStringList {entriesPath, "applications"}.join(QDir::separator());
    ensureDir(targetPath);

    for (auto const &fileInfo : desktopFileInfoList) {
        util::DesktopEntry desktopEntry(fileInfo.filePath());

        // FIXME: set all section
        auto exec = desktopEntry.rawValue("Exec", DesktopEntry::SectionDesktopEntry);
        exec = QString("ll-cli run %1 %2").arg(project->package->id, exec);
        desktopEntry.set(DesktopEntry::SectionDesktopEntry, "Exec", exec);

        auto ret = desktopEntry.save(QStringList {targetPath, fileInfo.fileName()}.join(QDir::separator()));
        if (!ret.success()) {
            return WrapError(ret, "save desktop failed");
        }
    }

    repo::OSTree repo(BuilderConfig::instance()->repoPath());

    auto ret = repo.importDirectory(project->ref(), project->config().cacheInstallPath(""));

    //        fuseOverlayfs.kill();
    kill(fuseOverlayfsPid, SIGTERM);

    return ret;
};

package::Ref fuzzyRef(const JsonSerialize *obj)
{
    if (!obj) {
        return package::Ref("");
    }
    auto id = obj->property("id").toString();
    auto version = obj->property("version").toString();
    auto arch = obj->property("arch").toString();

    package::Ref ref(id);

    if (!version.isEmpty()) {
        ref.version = version;
    }

    if (!arch.isEmpty()) {
        ref.arch = arch;
    } else {
        ref.arch = util::hostArch();
    }

    return ref;
}

class BuilderContainer
{
public:
    int startContainer(const Project &project) { return 0; }
};

// FIXME: should merge with runtime
int startContainer(Container *c, Runtime *r)
{
#define LINGLONG 118

#define LL_VAL(str) #str
#define LL_TOSTRING(str) LL_VAL(str)

    QList<QList<quint64>> uidMaps = {
        {getuid(), 0, 1},
    };
    for (auto const &uidMap : uidMaps) {
        auto idMap = new IdMap(r->linux);
        idMap->hostId = uidMap.value(0);
        idMap->containerId = uidMap.value(1);
        idMap->size = uidMap.value(2);
        r->linux->uidMappings.push_back(idMap);
    }

    QList<QList<quint64>> gidMaps = {
        {getgid(), 0, 1},
    };
    for (auto const &gidMap : gidMaps) {
        auto idMap = new IdMap(r->linux);
        idMap->hostId = gidMap.value(0);
        idMap->containerId = gidMap.value(1);
        idMap->size = gidMap.value(2);
        r->linux->gidMappings.push_back(idMap);
    }

    r->root->path = c->workingDirectory + "/root";
    util::ensureDir(r->root->path);

    QProcess process;

    // write pid file
    QFile pidFile(c->workingDirectory + QString("/%1.pid").arg(getpid()));
    pidFile.open(QIODevice::WriteOnly);
    pidFile.close();

    qDebug() << "start container at" << r->root->path;
    auto json = QJsonDocument::fromVariant(toVariant<Runtime>(r)).toJson();
    auto data = json.toStdString();

    int pipeEnds[2];
    if (pipe(pipeEnds) != 0) {
        return EXIT_FAILURE;
    }

    prctl(PR_SET_PDEATHSIG, SIGKILL);

    pid_t boxPid = fork();
    if (boxPid < 0) {
        return -1;
    }

    if (0 == boxPid) {
        // child process
        (void)close(pipeEnds[1]);
        if (dup2(pipeEnds[0], 118) == -1) {
            return EXIT_FAILURE;
        }
        (void)close(pipeEnds[0]);

        char const *const args[] = {"/usr/bin/ll-box", LL_TOSTRING(118), NULL};
        int ret = execvp(args[0], (char **)args);
        exit(ret);
    } else {
        close(pipeEnds[0]);
        write(pipeEnds[1], data.c_str(), data.size());
        close(pipeEnds[1]);

        c->pid = boxPid;
        waitpid(boxPid, nullptr, 0);
    }

    return EXIT_SUCCESS;
}

util::Error LinglongBuilder::create(const QString &projectName)
{
    auto projectPath = QStringList {QDir::currentPath(), projectName}.join("/");
    auto configFilePath = QStringList {projectPath, "linglong.yaml"}.join("/");

    //TODO: 判断projectName名称合法性
    //在当前目录创建项目文件夹
    auto ret = QDir().mkdir(projectPath);
    if (!ret) {
        return NewError(-1, "project already exists");
    }

    if(!QFile::copy(":templete.yaml",configFilePath)){
        return NewError(-1, "templete file is not found");
    }

    return NoError();
}

util::Error LinglongBuilder::build()
{
    util::Error ret(NoError());

    auto projectConfigPath = QStringList {BuilderConfig::instance()->projectRoot(), "linglong.yaml"}.join("/");
    QScopedPointer<Project> project(formYaml<Project>(YAML::LoadFile(projectConfigPath.toStdString())));
    project->setConfigFilePath(projectConfigPath);

    if (!project->package || project->package->kind.isEmpty()) {
        return NewError(-1, "unknown package kind");
    }

    SourceFetcher sf(project->source, project.get());
    if (project->source) {
        auto ret = sf.fetch();
        if (!ret.success()) {
            return NewError(ret, -1, "fetch source failed");
        }
    }

    util::removeDir(project->config().cacheRuntimePath({}));

    package::Ref baseRef("");

    QString hostBasePath;
    if (project->base) {
        baseRef = fuzzyRef(project->base);
    }

    if (project->runtime) {
        auto runtimeRef = fuzzyRef(project->runtime);

        BuildDepend runtimeDepend;
        runtimeDepend.id = runtimeRef.appId;
        runtimeDepend.version = runtimeRef.version;
        DependFetcher df(runtimeDepend, project.get());
        ret = df.fetch("", project->config().cacheRuntimePath(""));
        if (!ret.success()) {
            return NewError(ret, -1, "fetch runtime failed");
        }

        if (!project->base) {
            QFile infoFile(project->config().cacheRuntimePath("info.json"));
            if (!infoFile.open(QIODevice::ReadOnly)) {
                return NewError(infoFile.error(), infoFile.errorString());
            }
            auto info = fromVariant<package::Info>(QJsonDocument::fromJson(infoFile.readAll()).toVariant());

            package::Ref runtimeBaseRef(info->base);

            baseRef.appId = runtimeBaseRef.appId;
            baseRef.version = runtimeBaseRef.version;
        }
    }

    BuildDepend baseDepend;
    baseDepend.id = baseRef.appId;
    baseDepend.version = baseRef.version;
    DependFetcher baseFetcher(baseDepend, project.get());
    // TODO: the base filesystem hierarchy is just an debian now. we should change it
    hostBasePath = BuilderConfig::instance()->layerPath({baseRef.toLocalRefString(), "files"});
    ret = baseFetcher.fetch("", hostBasePath);
    if (!ret.success()) {
        return NewError(ret, -1, "fetch runtime failed");
    }

    // depends fetch
    for (auto const &depend : project->depends) {
        DependFetcher df(*depend, project.get());
        ret = df.fetch("files", project->config().cacheRuntimePath("files"));
        if (!ret.success()) {
            return NewError(ret, -1, "fetch dependency failed");
        }
    }

    QFile jsonFile(":/config.json");
    jsonFile.open(QIODevice::ReadOnly);
    auto json = QJsonDocument::fromJson(jsonFile.readAll());

    QScopedPointer<Runtime> rt(fromVariant<Runtime>(json.toVariant()));
    auto r = rt.get();

    auto container = new Container(this);
    ret = container->create();
    if (!ret.success()) {
        return WrapError(ret, "create container failed");
    }

    if (!r->annotations) {
        r->annotations = new Annotations(r);
    }

    if (!r->annotations->overlayfs) {
        r->annotations->overlayfs = new AnnotationsOverlayfsRootfs(r);

        util::removeDir(project->config().cacheAbsoluteFilePath({"overlayfs"}));

        r->annotations->overlayfs->lowerParent = project->config().cacheAbsoluteFilePath({"overlayfs", "lp"});
        r->annotations->overlayfs->workdir = project->config().cacheAbsoluteFilePath({"overlayfs", "wk"});
        r->annotations->overlayfs->upper = project->config().cacheAbsoluteFilePath({"overlayfs", "up"});
    }

    // get runtime, and parse base from runtime
    for (const auto &rpath : QStringList {"/usr", "/etc", "/var"}) {
        auto m = new Mount(r);
        m->type = "bind";
        m->options = QStringList {"ro", "rbind"};
        m->source = QStringList {hostBasePath, rpath}.join("/");
        m->destination = rpath;
        r->annotations->overlayfs->mounts.push_back(m);
    }

    // mount project build result path
    // for runtime/lib, use overlayfs
    // for app, mount to opt
    auto hostInstallPath = project->config().cacheInstallPath("files");
    ensureDir(hostInstallPath);
    QList<QPair<QString, QString>> overlaysMountMap = {};

    if (project->runtime || !project->depends.isEmpty()) {
        overlaysMountMap.push_back({project->config().cacheRuntimePath("files"), "/runtime"});
    }

    for (const auto &pair : overlaysMountMap) {
        auto m = new Mount(r);
        m->type = "bind";
        m->options = QStringList {"ro"};
        m->source = pair.first;
        m->destination = pair.second;

        r->annotations->overlayfs->mounts.push_back(m);
    }
    r->annotations->containerRootPath = container->workingDirectory;

    auto containerSourcePath = "/source";

    QList<QPair<QString, QString>> mountMap = {
        {sf.sourceRoot(), containerSourcePath},
        {project->buildScriptPath(), BuildScriptPath},
    };

    for (const auto &pair : mountMap) {
        auto m = new Mount(r);
        m->type = "bind";
        m->source = pair.first;
        m->destination = pair.second;
        r->mounts.push_back(m);
    }

    r->process->args = QStringList {"/bin/bash", BuildScriptPath};
    r->process->cwd = containerSourcePath;
    r->process->env.push_back(
        "PATH=/runtime/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games:/sbin:/usr/sbin");
    r->process->env.push_back("PKG_CONFIG_PATH=/runtime/lib/pkgconfig");
    r->process->env.push_back("PREFIX=" + project->config().targetInstallPath(""));

    if (!r->hooks) {
        r->hooks = new Hooks(r);
        auto hook = new Hook(r->hooks);
        hook->path = "/usr/sbin/ldconfig";
        r->hooks->prestart.push_back(hook);
    }

    // FIXME: get result
    startContainer(container, r);

    auto createInfo = [](Project *project) -> util::Error {
        package::Info info;

        info.kind = project->package->kind;
        info.version = project->package->version;

        info.base = project->baseRef().toLocalRefString();

        info.runtime = project->runtimeRef().toLocalRefString();

        info.appid = project->package->id;
        info.arch = QStringList {project->config().targetArch()};

        QFile infoFile(project->config().cacheInstallPath("info.json"));
        if (!infoFile.open(QIODevice::WriteOnly)) {
            return NewError(infoFile.error(), infoFile.errorString() + " " + infoFile.fileName());
        }
        if (infoFile.write(QJsonDocument::fromVariant(toVariant<package::Info>(&info)).toJson()) < 0) {
            return NewError(infoFile.error(), infoFile.errorString());
        }
        infoFile.close();

        QFile sourceConfigFile(project->configFilePath());
        if (!sourceConfigFile.copy(project->config().cacheInstallPath("linglong.yaml"))) {
            return NewError(sourceConfigFile.error(), sourceConfigFile.errorString());
        }

        return NoError();
    };

    removeDir(project->config().cacheInstallPath(""));
    ensureDir(project->config().cacheInstallPath(""));

    ret = createInfo(project.get());
    if (!ret.success()) {
        return NewError(ret, -1, "createInfo failed");
    }

    ret = commitBuildOutput(project.get(), r->annotations->overlayfs);
    if (!ret.success()) {
        return NewError(ret, -1, "commitBuildOutput failed");
    }
    return NoError();
}

util::Error LinglongBuilder::exportBundle(const QString &outputFilePath)
{
    QScopedPointer<Project> project(formYaml<Project>(YAML::LoadFile("linglong.yaml")));

    repo::OSTree repo(BuilderConfig::instance()->repoPath());

    // checkout data from local ostree
    auto exportPath = QStringList {BuilderConfig::instance()->projectRoot(), "export"}.join("/");
    util::ensureDir(exportPath);

    auto ret = repo.checkout(project->ref(), "", exportPath);
    if (!ret.success()) {
        return NewError(-1, "checkout files failed, you need build first");
    }

    // TODO: if the kind is not app, don't make bundle
    // make bundle package
    linglong::package::Bundle uabBundle;

    auto makeBundleResult = uabBundle.make(exportPath, outputFilePath);
    if (!makeBundleResult.success()) {
        return NewError(makeBundleResult, -1, "make bundle failed");
    }

    return NoError();
}

util::Error LinglongBuilder::push(const QString &bundleFilePath, bool force)
{
    // TODO: if the kind is not app, don't push bundle
    qInfo() << "start upload ...";

    linglong::package::Bundle uabBundle;

    auto pushBundleResult = uabBundle.push(bundleFilePath, force);
    if (!pushBundleResult.success()) {
        return NewError(pushBundleResult) << "push bundle failed!!!";
    }

    return NoError();
}

util::Error LinglongBuilder::run()
{
    repo::OSTree repo(BuilderConfig::instance()->repoPath());

    util::Error ret(NoError());

    auto projectConfigPath = QStringList {BuilderConfig::instance()->projectRoot(), "linglong.yaml"}.join("/");
    QScopedPointer<Project> project(formYaml<Project>(YAML::LoadFile(projectConfigPath.toStdString())));
    project->setConfigFilePath(projectConfigPath);

    // check app
    auto targetPath = BuilderConfig::instance()->layerPath({project->ref().toLocalRefString()});
    ensureDir(targetPath);
    ret = repo.checkout(project->ref(), "", targetPath);
    if (!ret.success()) {
        return NewError(-1, "checkout app files failed");
    }

    // check runtime
    targetPath = BuilderConfig::instance()->layerPath({project->runtimeRef().toLocalRefString()});
    ensureDir(targetPath);
    ret = repo.checkout(project->runtimeRef(), "", targetPath);
    if (!ret.success()) {
        return NewError(-1, "checkout runtime files failed");
    }

    auto app = runtime::App::load(&repo, project->ref(), BuilderConfig::instance()->exec(), false);
    if (nullptr == app) {
        return NewError(-1, "load App::load failed");
    }
    app->start();
    return ret;
}

} // namespace builder
} // namespace linglong