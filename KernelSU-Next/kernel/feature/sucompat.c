#include <linux/compiler_types.h>
#include <linux/preempt.h>
#include <linux/printk.h>
#include <linux/mm.h>
#include <linux/pgtable.h>
#include <linux/uaccess.h>
#include <asm/current.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/sched/task_stack.h>
#include <linux/ptrace.h>
#include <linux/namei.h>

#include "linux/jump_label.h"

#ifdef CONFIG_KSU_SUSFS
#include <linux/susfs_def.h>
#include <linux/minmax.h>
#include "selinux/selinux.h"
#include "objsec.h"
#include "runtime/ksud.h"
#endif

#include "policy/allowlist.h"
#include "policy/feature.h"
#include "klog.h" // IWYU pragma: keep
#include "runtime/ksud.h"
#include "feature/sucompat.h"
#include "policy/app_profile.h"
#include "hook/syscall_hook.h"

#include "tiny_sulog.h"

#define SU_PATH "/system/bin/su"
#define SH_PATH "/system/bin/sh"

#ifdef CONFIG_KSU_SUSFS
static const char sh_path[] = SH_PATH;
static const char su_path[] = SU_PATH;
static const char ksud_path[] = KSUD_PATH;

// Stubs for future adb_root and sulog integration
static inline int ksu_adb_root_handle_execve(const char *pathname, void ***envp) { return 0; }
static inline void *ksu_sulog_capture_sucompat(const char *filename, struct user_arg_ptr *argv, gfp_t gfp) { return NULL; }
static inline void ksu_sulog_emit_pending(void *event, int result, gfp_t gfp) { }
#endif

DEFINE_STATIC_KEY_TRUE(ksu_su_compat_enabled);

static int su_compat_feature_get(u64 *value)
{
	if (static_key_enabled(&ksu_su_compat_enabled))
		*value = 1;
	else
		*value = 0;
	return 0;
}

static int su_compat_feature_set(u64 value)
{
	bool enable = value != 0;
	if (enable)
		static_branch_enable(&ksu_su_compat_enabled);
	else
		static_branch_disable(&ksu_su_compat_enabled);
	pr_info("su_compat: set to %d\n", enable);
	return 0;
}

static const struct ksu_feature_handler su_compat_handler = {
	.feature_id = KSU_FEATURE_SU_COMPAT,
	.name = "su_compat",
	.get_handler = su_compat_feature_get,
	.set_handler = su_compat_feature_set,
};

static void __user *userspace_stack_buffer(const void *d, size_t len)
{
	// To avoid having to mmap a page in userspace, just write below the stack
	// pointer.
	char __user *p = (void __user *)current_user_stack_pointer() - len;

	return copy_to_user(p, d, len) ? NULL : p;
}

static char __user *sh_user_path(void)
{
	static const char sh_path_local[] = "/system/bin/sh";

	return userspace_stack_buffer(sh_path_local, sizeof(sh_path_local));
}

static char __user *ksud_user_path(void)
{
	static const char ksud_path_local[] = KSUD_PATH;

	return userspace_stack_buffer(ksud_path_local, sizeof(ksud_path_local));
}

#ifdef CONFIG_KSU_SUSFS
static const char __user *get_user_arg_ptr(struct user_arg_ptr argv, int nr)
{
	const char __user *native;

#ifdef CONFIG_COMPAT
	if (unlikely(argv.is_compat)) {
		compat_uptr_t compat;

		if (get_user(compat, argv.ptr.compat + nr))
			return ERR_PTR(-EFAULT);

		return compat_ptr(compat);
	}
#endif

	if (get_user(native, argv.ptr.native + nr))
		return ERR_PTR(-EFAULT);

	return native;
}

static bool ksu_str_ends_with(const char *str, size_t str_len, const char *suffix)
{
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len)
        return false;
    return memcmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

static bool ksu_is_zygote_or_adbd(const char *name, size_t len)
{
    if (len < 5)
        return false;

    // Fast-path: Check last character first to skip 99% of processes
    char last = name[len - 1];
    if (last == 'd')
        return ksu_str_ends_with(name, len, "/adbd");
    if (last == 's')
        return ksu_str_ends_with(name, len, "/app_process");
    if (last == '2')
        return ksu_str_ends_with(name, len, "/app_process32");
    if (last == '4')
        return ksu_str_ends_with(name, len, "/app_process64");

    return false;
}

/*
 * return 0 -> No further checks should be required afterwards
 * return non-zero -> Further checks should be continued afterwards
 */
int ksu_handle_execveat_init(struct filename *filename, struct user_arg_ptr *argv_user, struct user_arg_ptr *envp_user) {
    if (current->pid != 1 && is_init(get_current_cred())) {
        int ret;
        if (unlikely(strcmp(filename->name, KSUD_PATH) == 0)) {
            char tmp_filename[SUSFS_MAX_LEN_PATHNAME] = {0};
            const char __user *argv_user_ptr = get_user_arg_ptr(*argv_user, 0);
            struct ksu_sulog_pending_event *pending_sucompat = NULL;

            pr_info("hook_manager: escape to root for init executing ksud: %d\n", current->pid);
            ret = escape_to_root_for_init();
            if (ret) {
                pr_err("escape_to_root_for_init() failed: %d\n", ret);
                return ret;
            }
            if (!argv_user_ptr || IS_ERR(argv_user_ptr)) {
                pr_err("!argv_user_ptr || IS_ERR(argv_user_ptr)\n");
                return -EFAULT;
            }
            strncpy(tmp_filename, filename->name, SUSFS_MAX_LEN_PATHNAME - 1);
            pending_sucompat = ksu_sulog_capture_sucompat(tmp_filename, argv_user, GFP_KERNEL);
            ksu_sulog_emit_pending(pending_sucompat, ret, GFP_KERNEL);
            return 0;
        } else {
            size_t len = strlen(filename->name);
            if (likely(!ksu_is_zygote_or_adbd(filename->name, len)) &&
                        !susfs_is_current_proc_umounted())
            {
                pr_info("susfs: mark no sucompat checks for pid: '%d', exec: '%s'\n", current->pid, filename->name);
                susfs_set_current_proc_umounted();
                return 0;
            }
        }

#ifdef CONFIG_COMPAT
        if (unlikely(envp_user->is_compat))
            ret = ksu_adb_root_handle_execve(filename->name, (void ***)&envp_user->ptr.compat);
        else
            ret = ksu_adb_root_handle_execve(filename->name, (void ***)&envp_user->ptr.native);
#else
        ret = ksu_adb_root_handle_execve(filename->name, (void ***)&envp_user->ptr.native);
#endif

        if (ret) {
            pr_err("adb root failed: %d\n", ret);
            return ret;
        }
        return ret;
    }

    return -EINVAL;
}

int ksu_handle_execveat_sucompat(int *fd, struct filename **filename_ptr,
                 void *argv_user, void *envp_user,
                 int *__never_use_flags)
{
    struct filename *filename;
    int ret;

    if (unlikely(!filename_ptr))
        return 0;

    filename = *filename_ptr;
    if (IS_ERR(filename))
        return 0;

    if (!ksu_handle_execveat_init(filename, (struct user_arg_ptr*)argv_user, (struct user_arg_ptr*)envp_user))
        return 0;

    if (likely(memcmp(filename->name, su_path, sizeof(su_path))))
        return 0;

    write_sulog('x');
    pr_info("ksu_handle_execveat_sucompat: su found\n");

    memcpy((void *)filename->name, ksud_path, sizeof(ksud_path));

    ret = escape_with_root_profile();
    if (ret)
        pr_err("escape_with_root_profile() failed: %d\n", ret);

    return 0;
}

int ksu_handle_execveat(int *fd, struct filename **filename_ptr, void *argv,
            void *envp, int *flags)
{
    if (ksu_handle_execveat_ksud(fd, filename_ptr, argv, envp, flags))
        return 0;

    return ksu_handle_execveat_sucompat(fd, filename_ptr, argv, envp,
                        flags);
}
#endif

int ksu_handle_faccessat(int *dfd, const char __user **filename_user,
		int *mode, int *__unused_flags)
{
#ifdef CONFIG_KSU_SUSFS
    char path[sizeof(su_path) + 1] = {0};
    if (strncpy_from_user_nofault(path, *filename_user, sizeof(path)) < 0)
        return 0;

    if (unlikely(!memcmp(path, su_path, sizeof(su_path)))) {
        write_sulog('a');
        pr_info("ksu_handle_faccessat: su->sh!\n");
        *filename_user = sh_user_path();
    }
#else
	const char su[] = SU_PATH;

	if (!ksu_is_allow_uid_for_current(current_uid().val)) {
		return 0;
	}

	char path[sizeof(su) + 1];
	memset(path, 0, sizeof(path));
	strncpy_from_user_nofault(path, *filename_user, sizeof(path));

	if (unlikely(!memcmp(path, su, sizeof(su)))) {
		write_sulog('a');
		pr_info("faccessat su->sh!\n");
		*filename_user = sh_user_path();
	}
#endif

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0) && defined(CONFIG_KSU_SUSFS)
int ksu_handle_stat(int *dfd, struct filename **filename, int *flags) {
    if (unlikely(IS_ERR(*filename) || (*filename)->name == NULL))
        return 0;

    if (likely(memcmp((*filename)->name, su_path, sizeof(su_path))))
        return 0;

    write_sulog('s');
    pr_info("ksu_handle_stat: su->sh!\n");
    memcpy((void *)((*filename)->name), sh_path, sizeof(sh_path));
    return 0;
}
#else
int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags)
{
#ifdef CONFIG_KSU_SUSFS
    char path[sizeof(su_path) + 1] = {0};
    if (strncpy_from_user_nofault(path, *filename_user, sizeof(path)) < 0)
        return 0;

    if (unlikely(!memcmp(path, su_path, sizeof(su_path)))) {
        write_sulog('s');
        pr_info("ksu_handle_stat: su->sh!\n");
        *filename_user = sh_user_path();
    }
#else
	const char su[] = SU_PATH;

	if (!ksu_is_allow_uid_for_current(current_uid().val)) {
		return 0;
	}

	if (unlikely(!filename_user)) {
		return 0;
	}

	char path[sizeof(su) + 1];
	memset(path, 0, sizeof(path));
	strncpy_from_user_nofault(path, *filename_user, sizeof(path));

	if (unlikely(!memcmp(path, su, sizeof(su)))) {
		write_sulog('s');
		pr_info("newfstatat su->sh!\n");
		*filename_user = sh_user_path();
	}
#endif

	return 0;
}
#endif

#if defined(CONFIG_KPROBES) && !defined(CONFIG_KSU_SUSFS)
long ksu_handle_execve_sucompat(const char __user **filename_user, int orig_nr, const struct pt_regs *regs)
{
	const char su[] = SU_PATH;
	const char __user *fn;
	char path[sizeof(su) + 1];
	long ret;
	unsigned long addr;

	if (unlikely(!filename_user))
		goto do_orig_execve;

	if (!ksu_is_allow_uid_for_current(current_uid().val))
		goto do_orig_execve;

	addr = untagged_addr((unsigned long)*filename_user);
	fn = (const char __user *)addr;
	memset(path, 0, sizeof(path));
	ret = strncpy_from_user(path, fn, sizeof(path));

	if (ret < 0) {
		pr_warn("Access filename when execve failed: %ld", ret);
		goto do_orig_execve;
	}

	if (likely(memcmp(path, su, sizeof(su))))
		goto do_orig_execve;

	write_sulog('x');

	pr_info("sys_execve su found\n");
	*filename_user = ksud_user_path();

	ret = escape_with_root_profile();
	if (ret) {
		pr_err("escape_with_root_profile failed: %ld\n", ret);
		goto do_orig_execve;
	}

	ret = ksu_syscall_table[orig_nr](regs);
	if (ret < 0) {
		pr_err("failed to execve ksud as su: %ld, fallback to sh\n", ret);
		*filename_user = sh_user_path();
	} else {
		return ret;
	}

do_orig_execve:
	return ksu_syscall_table[orig_nr](regs);
}
#endif

// sucompat: permitted process can execute 'su' to gain root access.
void __init ksu_sucompat_init()
{
	if (ksu_register_feature_handler(&su_compat_handler)) {
		pr_err("Failed to register su_compat feature handler\n");
	}
}

void __exit ksu_sucompat_exit()
{
	ksu_unregister_feature_handler(KSU_FEATURE_SU_COMPAT);
}
