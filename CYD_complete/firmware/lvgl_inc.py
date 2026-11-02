# Makes <project>/include (where lv_conf.h lives) visible to the LVGL library build.
Import("env")
import os
inc = os.path.join(env.subst("$PROJECT_DIR"), "include")
env.Append(CPPPATH=[inc])
env.Append(CCFLAGS=["-I" + inc])
