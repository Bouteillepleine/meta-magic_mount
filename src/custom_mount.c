#include "custom_mount.h"
#include "ksu.h"
#include "magic_mount.h"
#include "module_tree.h"
#include "utils.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CUSTOM_LIST_PATH "/data/adb/magic_mount/custom"
#define KSUD_PATH "/data/adb/ksud"

#define KNSU_DIR "/data/adb/modules/kernelnosu"
#define KNSU_SU "/data/adb/modules/kernelnosu/system/bin/su"
#define KNSU_TARGET "/system/bin/su"

/* kernelnosu is installed, enabled, and ships a su binary. */
static bool knsu_active(void) {
    char buf[PATH_MAX];

    if (!path_exists(KNSU_SU))
        return false;
    if (path_join(KNSU_DIR, DISABLE_FILE_NAME, buf, sizeof(buf)) == 0 && path_exists(buf))
        return false;
    if (path_join(KNSU_DIR, REMOVE_FILE_NAME, buf, sizeof(buf)) == 0 && path_exists(buf))
        return false;

    return true;
}

/* Toggle KernelSU's sucompat feature (id 0) by exec-ing ksud. Runtime-only, so a
 * reboot restores the on-disk config; this can never permanently break root. */
static void set_sucompat(bool enable) {
    pid_t pid = fork();

    if (pid == 0) {
        execl(KSUD_PATH, "ksud", "feature", "set", "0", enable ? "1" : "0", (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
        LOGI("kernelnosu: sucompat -> %s", enable ? "on" : "off");
    } else {
        LOGW("kernelnosu: fork failed: %s", strerror(errno));
    }
}

void knsu_pre_mount(MagicMount *ctx) {
    (void)ctx;

    if (knsu_active()) {
        LOGI("kernelnosu: active, disabling sucompat before mount");
        set_sucompat(false);
    }
}

/* Mirror every entry of src_dir into work_dir (reuses the engine's mirroring). */
static int mirror_dir_contents(MagicMount *ctx, const char *src_dir, const char *work_dir) {
    DIR *d = opendir(src_dir);
    if (!d) {
        LOGE("custom: opendir %s: %s", src_dir, strerror(errno));
        return -1;
    }

    int rc = 0;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;
        if (mm_mirror_entry(ctx, src_dir, work_dir, de->d_name) != 0) {
            rc = -1;
            break;
        }
    }

    closedir(d);
    return rc;
}

/* Bind source over an existing target, read-only. */
static int bind_existing(MagicMount *ctx, const char *source, const char *target) {
    if (mount(source, target, NULL, MS_BIND, NULL) < 0) {
        LOGE("custom: bind %s->%s: %s", source, target, strerror(errno));
        return -1;
    }

    (void)mount(NULL, target, NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL);

    if (ctx->enable_unmountable)
        ksu_send_unmountable(target);

    return 0;
}

/* Create a missing target as a new file: build a tmpfs skeleton of the target's
 * parent directory, add the target bound to source, and MS_MOVE it into place. */
static int bind_missing(MagicMount *ctx, const char *source, const char *target,
                        const char *tmp_root) {
    char parent[PATH_MAX];
    char base[PATH_MAX];

    snprintf(parent, sizeof(parent), "%s", target);
    char *slash = strrchr(parent, '/');
    if (!slash || slash == parent) {
        LOGE("custom: invalid target %s", target);
        return -1;
    }
    *slash = '\0';
    snprintf(base, sizeof(base), "%s", slash + 1);

    if (!path_is_dir(parent)) {
        LOGE("custom: parent %s is not a directory", parent);
        return -1;
    }

    char work[PATH_MAX];
    if (snprintf(work, sizeof(work), "%s/custom_workdir_%s", tmp_root, base) >= (int)sizeof(work)) {
        LOGE("custom: workdir path too long for %s", target);
        return -1;
    }

    if (mkdir_p(work) != 0)
        return -1;

    if (mount(ctx->mount_source, work, "tmpfs", 0, "") < 0) {
        LOGE("custom: tmpfs %s: %s", work, strerror(errno));
        (void)rmdir(work);
        return -1;
    }
    (void)mount(NULL, work, NULL, MS_REC | MS_PRIVATE, NULL);

    struct stat pst;
    if (stat(parent, &pst) == 0) {
        chmod(work, pst.st_mode & 07777);
        chown(work, pst.st_uid, pst.st_gid);
        (void)copy_selcon(parent, work);
    }

    int rc = mirror_dir_contents(ctx, parent, work);

    if (rc == 0) {
        char target_file[PATH_MAX];
        if (path_join(work, base, target_file, sizeof(target_file)) != 0) {
            rc = -1;
        } else {
            int fd = open(target_file, O_CREAT | O_WRONLY, 0755);
            if (fd >= 0)
                close(fd);

            if (mount(source, target_file, NULL, MS_BIND, NULL) < 0) {
                LOGE("custom: bind %s->%s: %s", source, target_file, strerror(errno));
                rc = -1;
            } else {
                (void)mount(NULL, target_file, NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL);
            }
        }
    }

    if (rc == 0) {
        (void)mount(NULL, work, NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL);
        if (mount(work, parent, NULL, MS_MOVE, NULL) < 0) {
            LOGE("custom: move %s->%s: %s", work, parent, strerror(errno));
            rc = -1;
        } else {
            (void)mount(NULL, parent, NULL, MS_REC | MS_PRIVATE, NULL);
            if (ctx->enable_unmountable)
                ksu_send_unmountable(parent);
            LOGI("custom: created new file %s (from %s)", target, source);
        }
    }

    if (rc != 0) {
        (void)umount2(work, MNT_DETACH);
        (void)rmdir(work);
    }

    return rc;
}

static int do_custom_bind(MagicMount *ctx, const char *source, const char *target,
                          const char *tmp_root) {
    if (!path_exists(source)) {
        LOGW("custom: source missing, skip: %s", source);
        return 0;
    }

    LOGI("custom mount: %s -> %s", source, target);

    if (path_exists(target))
        return bind_existing(ctx, source, target);

    return bind_missing(ctx, source, target, tmp_root);
}

/* Parse a "bind <source> <target>" line (whitespace-separated). */
static int parse_bind_line(char *line, char **source, char **target) {
    char *tok = strtok(line, " \t");
    if (!tok || strcmp(tok, "bind") != 0)
        return -1;

    *source = strtok(NULL, " \t");
    *target = strtok(NULL, " \t");

    return (*source && *target) ? 0 : -1;
}

int custom_mount_apply(MagicMount *ctx, const char *tmp_root) {
    /* 1) kernelnosu real su (auto) */
    if (knsu_active())
        do_custom_bind(ctx, KNSU_SU, KNSU_TARGET, tmp_root);

    /* 2) user-defined custom bind list */
    FILE *fp = fopen(CUSTOM_LIST_PATH, "r");
    if (!fp)
        return 0;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *trimmed = str_trim(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#')
            continue;

        char copy[1024];
        snprintf(copy, sizeof(copy), "%s", trimmed);

        char *source = NULL;
        char *target = NULL;
        if (parse_bind_line(copy, &source, &target) == 0)
            do_custom_bind(ctx, source, target, tmp_root);
        else
            LOGW("custom: invalid line: %s", trimmed);
    }

    fclose(fp);
    return 0;
}

void knsu_post_mount(MagicMount *ctx) {
    (void)ctx;

    if (!knsu_active())
        return;

    struct stat mounted;
    struct stat source;

    if (stat(KNSU_TARGET, &mounted) == 0 && stat(KNSU_SU, &source) == 0 &&
        mounted.st_size == source.st_size) {
        LOGI("kernelnosu: real su active at %s", KNSU_TARGET);
    } else {
        LOGW("kernelnosu: real su did not land, restoring sucompat fallback");
        set_sucompat(true);
    }
}
