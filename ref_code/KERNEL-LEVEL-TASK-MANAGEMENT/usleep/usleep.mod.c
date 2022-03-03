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
	{ 0x1ece0962, __VMLINUX_SYMBOL_STR(pv_cpu_ops) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xdcb0349b, __VMLINUX_SYMBOL_STR(sys_close) },
	{ 0xced6bcd8, __VMLINUX_SYMBOL_STR(finish_wait) },
	{ 0x69e43cfd, __VMLINUX_SYMBOL_STR(prepare_to_wait_event) },
	{ 0x1000e51, __VMLINUX_SYMBOL_STR(schedule) },
	{ 0xfe487975, __VMLINUX_SYMBOL_STR(init_wait_entry) },
	{ 0xdc21da9e, __VMLINUX_SYMBOL_STR(hrtimer_cancel) },
	{ 0xee01fa4b, __VMLINUX_SYMBOL_STR(hrtimer_start_range_ns) },
	{ 0xf61c61a7, __VMLINUX_SYMBOL_STR(hrtimer_init) },
	{ 0xbdd5da2e, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0xb0cc2d88, __VMLINUX_SYMBOL_STR(wake_up_process) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "2BDCBB378C7070ACB03720F");

MODULE_INFO(suserelease, "openSUSE Leap 15.0");
