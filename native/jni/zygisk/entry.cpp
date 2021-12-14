#include <libgen.h>
#include <dlfcn.h>
#include <sys/prctl.h>
#include <android/log.h>

#include <utils.hpp>
#include <daemon.hpp>
#include <magisk.hpp>
#include <db.hpp>

#include "zygisk.hpp"
#include "deny/deny.hpp"

using namespace std;

void *self_handle = nullptr;

static int zygisk_log(int prio, const char *fmt, va_list ap);

#define zlog(prio) [](auto fmt, auto ap){ return zygisk_log(ANDROID_LOG_##prio, fmt, ap); }
static void zygisk_logging() {
    log_cb.d = zlog(DEBUG);
    log_cb.i = zlog(INFO);
    log_cb.w = zlog(WARN);
    log_cb.e = zlog(ERROR);
    log_cb.ex = nop_ex;
}

static char *first_stage_path = nullptr;
void unload_first_stage() {
    if (first_stage_path) {
        unmap_all(first_stage_path);
        free(first_stage_path);
        first_stage_path = nullptr;
    }
}

// Make sure /proc/self/environ is sanitized
// Filter env and reset MM_ENV_END
static void sanitize_environ() {
    char *cur = environ[0];

    for (int i = 0; environ[i]; ++i) {
        // Copy all env onto the original stack
        int len = strlen(environ[i]);
        memmove(cur, environ[i], len + 1);
        environ[i] = cur;
        cur += len + 1;
    }

    prctl(PR_SET_MM, PR_SET_MM_ENV_END, cur, 0, 0);
}

__attribute__((destructor))
static void zygisk_cleanup_wait() {
    if (self_handle) {
        // Wait 10us to make sure none of our code is executing
        timespec ts = { .tv_sec = 0, .tv_nsec = 10000L };
        nanosleep(&ts, nullptr);
    }
}

#define SECOND_STAGE_PTR "ZYGISK_PTR"

static void second_stage_entry(void *handle, const char *tmp, char *path) {
    self_handle = handle;
    MAGISKTMP = tmp;
    unsetenv(INJECT_ENV_2);
    unsetenv(SECOND_STAGE_PTR);

    zygisk_logging();
    ZLOGD("inject 2nd stage\n");
    hook_functions();

    // First stage will be unloaded before the first fork
    first_stage_path = path;
}

static void first_stage_entry() {
    android_logging();
    ZLOGD("inject 1st stage\n");

    char *ld = getenv("LD_PRELOAD");
    char tmp[128];
    strlcpy(tmp, getenv("MAGISKTMP"), sizeof(tmp));
    char *path;
    if (char *c = strrchr(ld, ':')) {
        *c = '\0';
        setenv("LD_PRELOAD", ld, 1);  // Restore original LD_PRELOAD
        path = strdup(c + 1);
    } else {
        unsetenv("LD_PRELOAD");
        path = strdup(ld);
    }
    unsetenv(INJECT_ENV_1);
    unsetenv("MAGISKTMP");
    sanitize_environ();

    char *num = strrchr(path, '.') - 1;

    // Update path to 2nd stage lib
    *num = '2';

    // Load second stage
    setenv(INJECT_ENV_2, "1", 1);
    void *handle = dlopen(path, RTLD_LAZY);
    remap_all(path);

    // Revert path to 1st stage lib
    *num = '1';

    // Run second stage entry
    char *env = getenv(SECOND_STAGE_PTR);
    decltype(&second_stage_entry) second_stage;
    sscanf(env, "%p", &second_stage);
    second_stage(handle, tmp, path);
}

__attribute__((constructor))
static void zygisk_init() {
    if (getenv(INJECT_ENV_2)) {
        // Return function pointer to first stage
        char buf[128];
        snprintf(buf, sizeof(buf), "%p", &second_stage_entry);
        setenv(SECOND_STAGE_PTR, buf, 1);
    } else if (getenv(INJECT_ENV_1)) {
        first_stage_entry();
    }
}

// The following code runs in zygote/app process

static int zygisk_log(int prio, const char *fmt, va_list ap) {
    // If we don't have log pipe set, ask magiskd for it
    // This could happen multiple times in zygote because it was closed to prevent crashing
    if (logd_fd < 0) {
        // Change logging temporarily to prevent infinite recursion and stack overflow
        android_logging();
        if (int fd = connect_daemon(); fd >= 0) {
            write_int(fd, ZYGISK_REQUEST);
            write_int(fd, ZYGISK_GET_LOG_PIPE);
            if (read_int(fd) == 0) {
                logd_fd = recv_fd(fd);
            }
            close(fd);
        }
        zygisk_logging();
    }

    sigset_t mask;
    sigset_t orig_mask;
    bool sig = false;
    // Make sure SIGPIPE won't crash zygote
    if (logd_fd >= 0) {
        sig = true;
        sigemptyset(&mask);
        sigaddset(&mask, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &mask, &orig_mask);
    }
    int ret = magisk_log(prio, fmt, ap);
    if (sig) {
        timespec ts{};
        sigtimedwait(&mask, nullptr, &ts);
        pthread_sigmask(SIG_SETMASK, &orig_mask, nullptr);
    }
    return ret;
}

std::vector<int> remote_get_info(int uid, const char *process, AppInfo *info) {
    vector<int> fds;
    if (int fd = connect_daemon(); fd >= 0) {
        write_int(fd, ZYGISK_REQUEST);
        write_int(fd, ZYGISK_GET_INFO);

        write_int(fd, uid);
        write_string(fd, process);
        xxread(fd, info, sizeof(*info));
        if (!info->on_denylist) {
            fds = recv_fds(fd);
        }
        close(fd);
    }
    return fds;
}

// The following code runs in magiskd

static bool get_exe(int pid, char *buf, size_t sz) {
    snprintf(buf, sz, "/proc/%d/exe", pid);
    return xreadlink(buf, buf, sz) > 0;
}

static int zygiskd_sockets[] = { -1, -1 };
#define zygiskd_socket zygiskd_sockets[is_64_bit]

static void connect_companion(int client, bool is_64_bit) {
    if (zygiskd_socket >= 0) {
        // Make sure the socket is still valid
        pollfd pfd = { zygiskd_socket, 0, 0 };
        poll(&pfd, 1, 0);
        if (pfd.revents) {
            // Any revent means error
            close(zygiskd_socket);
            zygiskd_socket = -1;
        }
    }
    if (zygiskd_socket < 0) {
        int fds[2];
        socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds);
        zygiskd_socket = fds[0];
        if (fork_dont_care() == 0) {
            string exe = MAGISKTMP + "/magisk" + (is_64_bit ? "64" : "32");
            // This fd has to survive exec
            fcntl(fds[1], F_SETFD, 0);
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", fds[1]);
            execl(exe.data(), "zygisk", "companion", buf, (char *) nullptr);
            exit(-1);
        }
        close(fds[1]);
        vector<int> module_fds = zygisk_module_fds(is_64_bit);
        send_fds(zygiskd_socket, module_fds.data(), module_fds.size());
        // Wait for ack
        if (read_int(zygiskd_socket) != 0) {
            return;
        }
    }
    send_fd(zygiskd_socket, client);
}

static timespec last_zygote_start;
static int zygote_start_counts[] = { 0, 0 };
#define zygote_start_count zygote_start_counts[is_64_bit]
#define zygote_started (zygote_start_counts[0] + zygote_start_counts[1])
#define zygote_start_reset(val) { zygote_start_counts[0] = val; zygote_start_counts[1] = val; }

static void setup_files(int client, const sock_cred *cred) {
    LOGD("zygisk: setup files for pid=[%d]\n", cred->pid);

    char buf[256];
    if (!get_exe(cred->pid, buf, sizeof(buf))) {
        write_int(client, 1);
        return;
    }

    bool is_64_bit = str_ends(buf, "64");

    if (!zygote_started) {
        // First zygote launch, record time
        clock_gettime(CLOCK_MONOTONIC, &last_zygote_start);
    }

    if (zygote_start_count) {
        // This zygote ABI had started before, kill existing zygiskd
        close(zygiskd_sockets[0]);
        close(zygiskd_sockets[1]);
        zygiskd_sockets[0] = -1;
        zygiskd_sockets[1] = -1;
    }
    ++zygote_start_count;

    if (zygote_start_count >= 5) {
        // Bootloop prevention
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        if (ts.tv_sec - last_zygote_start.tv_sec > 60) {
            // This is very likely manual soft reboot
            memcpy(&last_zygote_start, &ts, sizeof(ts));
            zygote_start_reset(1);
        } else {
            // If any zygote relaunched more than 5 times within a minute,
            // don't do any setups further to prevent bootloop.
            zygote_start_reset(999);
            write_int(client, 1);
            return;
        }
    }

    write_int(client, 0);
    send_fd(client, is_64_bit ? app_process_64 : app_process_32);

    string path = MAGISKTMP + "/" ZYGISKBIN "/zygisk." + basename(buf);
    cp_afc(buf, (path + ".1.so").data());
    cp_afc(buf, (path + ".2.so").data());
    write_string(client, MAGISKTMP);
}

static void magiskd_passthrough(int client) {
    bool is_64_bit = read_int(client);
    write_int(client, 0);
    send_fd(client, is_64_bit ? app_process_64 : app_process_32);
}

int cached_manager_app_id = -1;
static time_t last_modified = 0;

static void get_process_info(int client, const sock_cred *cred) {
    AppInfo info{};
    int uid = read_int(client);
    string process = read_string(client);

    // This function is called on every single zygote process specialization,
    // so performance is critical. get_manager_app_id() is expensive as it goes
    // through a SQLite query and potentially multiple filesystem stats, so we
    // really want to cache the app ID value. Check the last modify timestamp of
    // packages.xml and only re-fetch the manager app ID if something changed since
    // we last checked. Granularity in seconds is good enough.
    // If denylist is enabled, inotify will invalidate the app ID cache for us.
    // In this case, we can skip the timestamp check all together.

    if (uid != 1000) {
        int manager_app_id = cached_manager_app_id;

        // Denylist not enabled, check packages.xml timestamp
        if (!denylist_enabled && manager_app_id > 0) {
            struct stat st{};
            stat("/data/system/packages.xml", &st);
            if (st.st_atim.tv_sec > last_modified) {
                manager_app_id = -1;
                last_modified = st.st_atim.tv_sec;
            }
        }

        if (manager_app_id < 0) {
            manager_app_id = get_manager_app_id();
            cached_manager_app_id = manager_app_id;
        }

        if (to_app_id(uid) == manager_app_id) {
            info.is_magisk_app = true;
        } else if (denylist_enabled) {
            info.on_denylist = is_deny_target(uid, process);
        }
    }

    xwrite(client, &info, sizeof(info));

    if (!info.on_denylist) {
        char buf[256];
        get_exe(cred->pid, buf, sizeof(buf));
        vector<int> fds = zygisk_module_fds(str_ends(buf, "64"));
        send_fds(client, fds.data(), fds.size());
    }
}

static void send_log_pipe(int fd) {
    // There is race condition here, but we can't really do much about it...
    if (logd_fd >= 0) {
        write_int(fd, 0);
        send_fd(fd, logd_fd);
    } else {
        write_int(fd, 1);
    }
}

void zygisk_handler(int client, const sock_cred *cred) {
    int code = read_int(client);
    char buf[256];
    switch (code) {
    case ZYGISK_SETUP:
        setup_files(client, cred);
        break;
    case ZYGISK_PASSTHROUGH:
        magiskd_passthrough(client);
        break;
    case ZYGISK_GET_INFO:
        get_process_info(client, cred);
        break;
    case ZYGISK_GET_LOG_PIPE:
        send_log_pipe(client);
        break;
    case ZYGISK_CONNECT_COMPANION:
        get_exe(cred->pid, buf, sizeof(buf));
        connect_companion(client, str_ends(buf, "64"));
        break;
    }
    close(client);
}
