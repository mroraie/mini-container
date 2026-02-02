#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <memory>
#include "../include/filesystem_manager.hpp"
using namespace std;
static const char *essential_dirs[] = {
    "/bin",
    "/sbin",
    "/lib",
    "/lib64",
    "/usr",
    "/usr/bin",
    "/usr/sbin",
    "/usr/lib",
    "/usr/lib64",
    "/etc",
    "/proc",
    "/sys",
    "/dev",
    "/tmp",
    "/var",
    "/var/tmp",
    nullptr
};
static struct {
    const char *path;
    mode_t mode;
    dev_t dev;
} essential_devices[] = {
    {"/dev/null", S_IFCHR | 0666, makedev(1, 3)},
    {"/dev/zero", S_IFCHR | 0666, makedev(1, 5)},
    {"/dev/random", S_IFCHR | 0666, makedev(1, 8)},
    {"/dev/urandom", S_IFCHR | 0666, makedev(1, 9)},
    {"/dev/tty", S_IFCHR | 0666, makedev(5, 0)},
    {"/dev/console", S_IFCHR | 0600, makedev(5, 1)},
    {nullptr, 0, 0}
};
static int mkdir_p(const char *path) {
    if (!path) return -1;
    if (mkdir(path, 0755) == 0) {
        return 0;
    }
    if (errno == ENOENT) {
        std::unique_ptr<char[]> path_copy(new char[strlen(path) + 1]);
        strcpy(path_copy.get(), path);
        char *parent = dirname(path_copy.get());
        if (mkdir_p(parent) != 0) {
            return -1;
        }
        return mkdir(path, 0755);
    }
    if (errno == EEXIST) {
        return 0;
    }
    return -1;
}
static int copy_file(const char *src, const char *dst) {
    int src_fd = open(src, O_RDONLY);
    if (src_fd == -1) {
        return -1;
    }
    int dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (dst_fd == -1) {
        close(src_fd);
        return -1;
    }
    char buffer[8192];
    ssize_t bytes_read, bytes_written;
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(dst_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            close(src_fd);
            close(dst_fd);
            return -1;
        }
    }
    close(src_fd);
    close(dst_fd);
    return (bytes_read == -1) ? -1 : 0;
}
void fs_config_init(fs_config_t *config) {
    if (!config) return;
    config->root_path = nullptr;
    config->method = FS_CHROOT;
    config->create_minimal_fs = 0;
}
int fs_create_minimal_root(const char *root_path) {
    if (!root_path) {
        fprintf(stderr, "Error: root path cannot be NULL\n");
        return -1;
    }
    for (int i = 0; essential_dirs[i] != nullptr; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s%s", root_path, essential_dirs[i]);
        if (mkdir_p(path) != 0) {
            fprintf(stderr, "Failed to create directory: %s\n", path);
            return -1;
        }
    }
    for (int i = 0; essential_devices[i].path != nullptr; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s%s", root_path, essential_devices[i].path);
        if (mknod(path, essential_devices[i].mode, essential_devices[i].dev) == -1) {
            if (errno != EEXIST) {
                perror("mknod failed");
                return -1;
            }
        }
    }
    return 0;
}
int fs_setup_chroot(const char *root_path) {
    if (!root_path) {
        fprintf(stderr, "Error: root path cannot be NULL\n");
        return -1;
    }
    if (chdir(root_path) == -1) {
        perror("chdir to root path failed");
        return -1;
    }
    if (chroot(root_path) == -1) {
        perror("chroot failed");
        return -1;
    }
    if (chdir("/") == -1) {
        perror("chdir to / inside chroot failed");
        return -1;
    }
    return 0;
}
int fs_mount_container_filesystems(const char *root_path) {
    if (!root_path) {
        fprintf(stderr, "Error: root path cannot be NULL\n");
        return -1;
    }
    char mount_path[PATH_MAX];
    snprintf(mount_path, sizeof(mount_path), "%s/proc", root_path);
    if (mount("proc", mount_path, "proc", 0, nullptr) == -1) {
        perror("mount proc failed");
        return -1;
    }
    snprintf(mount_path, sizeof(mount_path), "%s/sys", root_path);
    if (mount("sysfs", mount_path, "sysfs", 0, nullptr) == -1) {
        perror("mount sysfs failed");
        return -1;
    }
    snprintf(mount_path, sizeof(mount_path), "%s/tmp", root_path);
    if (mount("tmpfs", mount_path, "tmpfs", 0, nullptr) == -1) {
        perror("mount tmpfs failed");
        return -1;
    }
    snprintf(mount_path, sizeof(mount_path), "%s/dev", root_path);
    if (mount("dev", mount_path, "devtmpfs", 0, nullptr) == -1) {
        perror("mount devtmpfs failed");
        return -1;
    }
    return 0;
}
int fs_populate_container_root(const char *root_path, const char *host_root) {
    if (!root_path || !host_root) {
        fprintf(stderr, "Error: root paths cannot be NULL\n");
        return -1;
    }
    const char *essential_files[] = {
        "/bin/sh",
        "/bin/bash",
        "/lib/ld-linux.so.2",
        "/lib64/ld-linux-x86-64.so.2",
        nullptr
    };
    for (int i = 0; essential_files[i] != nullptr; i++) {
        char src_path[PATH_MAX];
        char dst_path[PATH_MAX];
        snprintf(src_path, sizeof(src_path), "%s%s", host_root, essential_files[i]);
        snprintf(dst_path, sizeof(dst_path), "%s%s", root_path, essential_files[i]);
        if (copy_file(src_path, dst_path) != 0) {
            fprintf(stderr, "Warning: failed to copy %s\n", essential_files[i]);
        }
    }
    return 0;
}
int fs_cleanup_container_root(const char *root_path) {
    if (!root_path) {
        fprintf(stderr, "Error: root path cannot be NULL\n");
        return -1;
    }
    char mount_path[PATH_MAX];
    const char *mounts_to_unmount[] = {
        "/dev",
        "/tmp",
        "/sys",
        "/proc",
        nullptr
    };
    for (int i = 0; mounts_to_unmount[i] != nullptr; i++) {
        snprintf(mount_path, sizeof(mount_path), "%s%s", root_path, mounts_to_unmount[i]);
        if (umount2(mount_path, MNT_DETACH) == -1) {
            if (errno != EINVAL) {
                perror("umount2 failed");
            }
        }
    }
    if (rmdir(root_path) == -1) {
        if (errno != ENOTEMPTY) {
            perror("rmdir root path failed");
            return -1;
        }
    }
    return 0;
}