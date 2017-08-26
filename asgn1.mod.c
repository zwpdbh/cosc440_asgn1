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

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x4cd667c1, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x573a869c, __VMLINUX_SYMBOL_STR(device_destroy) },
	{ 0x7485e15e, __VMLINUX_SYMBOL_STR(unregister_chrdev_region) },
	{ 0x21178273, __VMLINUX_SYMBOL_STR(cdev_del) },
	{ 0x5a041bb8, __VMLINUX_SYMBOL_STR(class_destroy) },
	{ 0x17b37d38, __VMLINUX_SYMBOL_STR(device_create) },
	{ 0xf39cacaa, __VMLINUX_SYMBOL_STR(__class_create) },
	{ 0x32a397eb, __VMLINUX_SYMBOL_STR(cdev_add) },
	{ 0x934e3b0d, __VMLINUX_SYMBOL_STR(cdev_init) },
	{ 0x29537c9e, __VMLINUX_SYMBOL_STR(alloc_chrdev_region) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x6b321efe, __VMLINUX_SYMBOL_STR(__free_pages) },
	{ 0x28cc25db, __VMLINUX_SYMBOL_STR(arm_copy_from_user) },
	{ 0xfa2a45e, __VMLINUX_SYMBOL_STR(__memzero) },
	{ 0xe5e85202, __VMLINUX_SYMBOL_STR(contig_page_data) },
	{ 0xdfc98939, __VMLINUX_SYMBOL_STR(__alloc_pages_nodemask) },
	{ 0xce23173e, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x440473b8, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0xf4fa543b, __VMLINUX_SYMBOL_STR(arm_copy_to_user) },
	{ 0x49bbd46f, __VMLINUX_SYMBOL_STR(mem_map) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x2e5810c6, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr1) },
	{ 0xb1ad28e0, __VMLINUX_SYMBOL_STR(__gnu_mcount_nc) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "763B590B77B2B4411B2045F");
