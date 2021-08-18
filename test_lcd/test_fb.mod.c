#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x10d06f3b, "module_layout" },
	{ 0xccf15c6e, "kmalloc_caches" },
	{ 0xf9a482f9, "msleep" },
	{ 0x90567055, "dma_release_from_dev_coherent" },
	{ 0x5736a32a, "of_find_node_opts_by_path" },
	{ 0xb0d464b8, "framebuffer_release" },
	{ 0xa1381993, "arm_dma_ops" },
	{ 0xe027f59b, "gpio_to_desc" },
	{ 0xe85c13b, "__spi_register_driver" },
	{ 0xed257b08, "spi_setup" },
	{ 0xb1ecc9c7, "kthread_create_on_node" },
	{ 0x7ea32650, "dma_alloc_from_dev_coherent" },
	{ 0x5f754e5a, "memset" },
	{ 0x7c32d0f0, "printk" },
	{ 0xed4b9a0c, "kthread_stop" },
	{ 0xd633690a, "driver_unregister" },
	{ 0xff4baf76, "register_framebuffer" },
	{ 0x16305289, "warn_slowpath_null" },
	{ 0xe392d11e, "gpiod_direction_output_raw" },
	{ 0xdf6bc1e7, "spi_sync" },
	{ 0x2bac60c1, "kmem_cache_alloc" },
	{ 0xdb7305a1, "__stack_chk_fail" },
	{ 0x470efb25, "of_get_named_gpio_flags" },
	{ 0x92aeccf5, "wake_up_process" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0x4cfe3a07, "framebuffer_alloc" },
	{ 0xfe990052, "gpio_free" },
	{ 0x37a0cba, "kfree" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0x2226969a, "unregister_framebuffer" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "190131C2129A125C7BD6EAC");
