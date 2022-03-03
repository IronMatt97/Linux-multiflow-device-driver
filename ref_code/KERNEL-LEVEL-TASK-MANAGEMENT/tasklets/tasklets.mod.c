#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x881e648f, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x4a18199a, __VMLINUX_SYMBOL_STR(param_ops_ulong) },
	{ 0xfaef0ed, __VMLINUX_SYMBOL_STR(__tasklet_schedule) },
	{ 0x9545af6d, __VMLINUX_SYMBOL_STR(tasklet_init) },
	{ 0x92333ba5, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xd3bcd8a6, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x3e0e1252, __VMLINUX_SYMBOL_STR(try_module_get) },
	{ 0x7926ea18, __VMLINUX_SYMBOL_STR(module_put) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0xbdd5da2e, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0x7a2af7b4, __VMLINUX_SYMBOL_STR(cpu_number) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "26E27961982CB7E133C64CA");

MODULE_INFO(suserelease, "openSUSE Leap 15.0");
