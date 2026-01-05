#ifndef FILESYSTEM_MANAGER_HPP
#define FILESYSTEM_MANAGER_HPP

typedef enum {
    FS_CHROOT
} fs_isolation_method_t;

typedef struct {
    char *root_path;
    fs_isolation_method_t method;
    int create_minimal_fs;
} fs_config_t;

#ifdef __cplusplus
extern "C" {
#endif

void fs_config_init(fs_config_t *config);

int fs_create_minimal_root(const char *root_path);

int fs_setup_chroot(const char *root_path);

int fs_mount_container_filesystems(const char *root_path);

int fs_populate_container_root(const char *root_path, const char *host_root);

int fs_cleanup_container_root(const char *root_path);

#ifdef __cplusplus
}
#endif

#endif

