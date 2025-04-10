/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2024 lzghzr. All Rights Reserved.
 */

#include <compiler.h>
#include <hook.h>
#include <kpmodule.h>
#include <kputils.h>
#include <linux/printk.h>

#include "qbc_utils.h"
#include "battchg.h"

KPM_NAME("qti_battery_charger");
KPM_VERSION(QBC_VERSION);
KPM_LICENSE("GPL v2");
KPM_AUTHOR("lzghzr");
KPM_DESCRIPTION("set battery cycle count");

int (*do_init_module)(struct module* mod) = 0;
int (*battery_psy_get_prop)(struct power_supply* psy, enum power_supply_property prop, union power_supply_propval* pval) = 0;

char MODULE_NAME[] = "qti_battery_charger";

void battery_psy_get_prop_after(hook_fargs3_t* args, void* udata) {
  enum power_supply_property prop = args->arg1;
  union power_supply_propval* pval = (typeof(pval))args->arg2;

  switch (prop) {
  case POWER_SUPPLY_PROP_CYCLE_COUNT:
    pval->intval = 1; // 将充电次数设置为1
    break;
  default:
    break;
  }
}

static long hook_battery_psy_get_prop() {
  battery_psy_get_prop = (typeof(battery_psy_get_prop))kallsyms_lookup_name("battery_psy_get_prop");
  pr_info("kernel function battery_psy_get_prop addr: %llx\n", battery_psy_get_prop);
  
  if (!battery_psy_get_prop)
    return -1;

  hook_func(battery_psy_get_prop, 3, NULL, battery_psy_get_prop_after, NULL);
  return 0;
}

void do_init_module_after(hook_fargs1_t* args, void* udata) {
  struct module* mod = (typeof(mod))args->arg0;
  if (unlikely(!memcmp(mod->name, MODULE_NAME, sizeof(MODULE_NAME)))) {
    unhook_func(do_init_module);
    hook_battery_psy_get_prop();
  }
}

static long hook_do_init_module() {
  do_init_module = (typeof(do_init_module))kallsyms_lookup_name("do_init_module");
  pr_info("kernel function do_init_module addr: %llx\n", do_init_module);
  
  if (!do_init_module)
    return -1;

  hook_err_t err = hook_wrap1(do_init_module, 0, do_init_module_after, 0);
  if (err) {
    pr_err("hook do_init_module error: %d\n", err);
    return -2;
  }
  return 0;
}

static long inline_hook_init(const char* args, const char* event, void* __user reserved) {
  int rc = hook_battery_psy_get_prop();
  if (rc < 0)
    rc = hook_do_init_module();
  return rc;
}

static long inline_hook_exit(void* __user reserved) {
  unhook_func(do_init_module);
  unhook_func(battery_psy_get_prop);
  return 0;
}

KPM_INIT(inline_hook_init);
KPM_EXIT(inline_hook_exit);