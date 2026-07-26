#ifndef CUSTOM_MOUNT_H
#define CUSTOM_MOUNT_H

#include "magic_mount.h"

/*
 * Custom bind mounts + kernelnosu integration.
 *
 * Custom mounts are read from /data/adb/magic_mount/custom, one per line:
 *
 *     bind <source> <target>
 *
 * Unlike the module tree (which is /system-relative), a custom mount binds an
 * arbitrary source onto an arbitrary target, and CREATES the target as a new
 * file if it does not exist (by mirroring the target's parent dir into a tmpfs
 * skeleton, adding the target, and MS_MOVE-ing it into place). Lines starting
 * with '#' are comments.
 *
 * kernelnosu: if /data/adb/modules/kernelnosu is installed with a `su` binary,
 * it is auto-mounted at /system/bin/su the same way, so a real, consistent su
 * replaces KernelSU's sucompat path-hook and is hidden by SUSFS like any other
 * metamodule mount (no global kernel_umount needed).
 */

/* Disable sucompat before the mount when kernelnosu is active, so /system/bin/su
 * is a clear target (the spoof would otherwise block the real mount). No-op
 * when kernelnosu is not installed. */
void knsu_pre_mount(MagicMount *ctx);

/* Apply custom bind mounts (the kernelnosu su first, then the user list).
 * Call AFTER magic_mount(); tmp_root is the tmpfs parent used for skeletons. */
int custom_mount_apply(MagicMount *ctx, const char *tmp_root);

/* Self-heal: if kernelnosu's real su did not land at /system/bin/su, re-enable
 * sucompat so root can never get stuck off. No-op when kernelnosu is inactive. */
void knsu_post_mount(MagicMount *ctx);

#endif /* CUSTOM_MOUNT_H */
