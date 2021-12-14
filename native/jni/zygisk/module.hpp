#pragma once

#include "api.hpp"

namespace {

using module_abi_v1 = zygisk::internal::module_abi;
struct HookContext;
struct ApiTable;

struct AppSpecializeArgsImpl {
    jint &uid;
    jint &gid;
    jintArray &gids;
    jint &runtime_flags;
    jint &mount_external;
    jstring &se_info;
    jstring &nice_name;
    jstring &instruction_set;
    jstring &app_data_dir;

    /* Optional */
    jboolean *is_child_zygote = nullptr;
    jboolean *is_top_app = nullptr;
    jobjectArray *pkg_data_info_list = nullptr;
    jobjectArray *whitelisted_data_info_list = nullptr;
    jboolean *mount_data_dirs = nullptr;
    jboolean *mount_storage_dirs = nullptr;

    AppSpecializeArgsImpl(
            jint &uid, jint &gid, jintArray &gids, jint &runtime_flags,
            jint &mount_external, jstring &se_info, jstring &nice_name,
            jstring &instruction_set, jstring &app_data_dir) :
            uid(uid), gid(gid), gids(gids), runtime_flags(runtime_flags),
            mount_external(mount_external), se_info(se_info), nice_name(nice_name),
            instruction_set(instruction_set), app_data_dir(app_data_dir) {}
};

struct ServerSpecializeArgsImpl {
    jint &uid;
    jint &gid;
    jintArray &gids;
    jint &runtime_flags;
    jlong &permitted_capabilities;
    jlong &effective_capabilities;

    ServerSpecializeArgsImpl(
            jint &uid, jint &gid, jintArray &gids, jint &runtime_flags,
            jlong &permitted_capabilities, jlong &effective_capabilities) :
            uid(uid), gid(gid), gids(gids), runtime_flags(runtime_flags),
            permitted_capabilities(permitted_capabilities),
            effective_capabilities(effective_capabilities) {}
};

template<typename T>
struct force_cast_wrapper {
    template<typename U>
    operator U() const { return reinterpret_cast<U>(mX); }
    force_cast_wrapper(T &&x) : mX(std::forward<T>(x)) {}
    force_cast_wrapper &operator=(const force_cast_wrapper &) = delete;
private:
    T &&mX;
};

template<typename R>
force_cast_wrapper<R> force_cast(R &&x) {
    return force_cast_wrapper<R>(std::forward<R>(x));
}

struct ZygiskModule {
    void preAppSpecialize(AppSpecializeArgsImpl *args) {
        v1->preAppSpecialize(v1->_this, force_cast(args));
    }
    void postAppSpecialize(const AppSpecializeArgsImpl *args) {
        v1->postAppSpecialize(v1->_this, force_cast(args));
    }
    void preServerSpecialize(ServerSpecializeArgsImpl *args) {
        v1->preServerSpecialize(v1->_this, force_cast(args));
    }
    void postServerSpecialize(const ServerSpecializeArgsImpl *args) {
        v1->postServerSpecialize(v1->_this, force_cast(args));
    }

    int connectCompanion() const;
    void setOption(zygisk::Option opt);
    static bool registerModule(ApiTable *table, long *module);

    ZygiskModule(int id, void *handle) : handle(handle), id(id) {}

    void * const handle;
    ApiTable *table = nullptr;
    bool unload = false;

private:
    int id;
    union {
        long *ver = nullptr;
        module_abi_v1 *v1;
    };
};

struct ApiTable {
    ZygiskModule *module;
    bool (*registerModule)(ApiTable *, long *);

    union {
        void *padding[6] = {};
        struct {
            void (*hookJniNativeMethods)(JNIEnv *, const char *, JNINativeMethod *, int);
            void (*pltHookRegister)(const char *, const char *, void *, void **);
            void (*pltHookExclude)(const char *, const char *);
            bool (*pltHookCommit)();

            int (*connectCompanion)(ZygiskModule *);
            void (*setOption)(ZygiskModule *, zygisk::Option);
        } v1;
    };
    ApiTable(ZygiskModule *m) : module(m), registerModule(&ZygiskModule::registerModule) {}
};

} // namespace
