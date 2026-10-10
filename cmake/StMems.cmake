include_guard(GLOBAL)
include(FetchContent)

# ST's platform-independent register drivers ship as a single .c/.h pair each.
FetchContent_Declare(lsm6dsox_pid
    URL https://github.com/STMicroelectronics/lsm6dsox-pid/archive/82e615e799f2683f2e6562c4df7d6193313b15df.tar.gz
    URL_HASH SHA256=5f42f11570af0045675ea3def53eca3672e791b202be95c33633906c52558acd
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(lis3mdl_pid
    URL https://github.com/STMicroelectronics/lis3mdl-pid/archive/f002ac7436ee3ccd3bfd821c2046a9a1cc802634.tar.gz
    URL_HASH SHA256=39ba4ea8dd7136ed40fb8638ad769459b6267f5a9b14201b23b4b6d83429c298
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(lsm6dsox_pid lis3mdl_pid)

add_library(st_mems STATIC
    "${lsm6dsox_pid_SOURCE_DIR}/lsm6dsox_reg.c"
    "${lis3mdl_pid_SOURCE_DIR}/lis3mdl_reg.c")
target_include_directories(st_mems PUBLIC "${lsm6dsox_pid_SOURCE_DIR}" "${lis3mdl_pid_SOURCE_DIR}")
