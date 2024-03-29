include(CheckIncludeFile)

function(check_darwin_deps)
    check_include_file(mach/mach_init.h                     HAVE_MACH_MACH_INIT_H)
    check_include_file(mach/host_priv.h                     HAVE_MACH_HOST_PRIV_H)
    check_include_file(mach/mach_error.h                    HAVE_MACH_MACH_ERROR_H)
    check_include_file(mach/mach_host.h                     HAVE_MACH_MACH_HOST_H)
    check_include_file(mach/mach_port.h                     HAVE_MACH_MACH_PORT_H)
    check_include_file(mach/mach_types.h                    HAVE_MACH_MACH_TYPES_H)
    check_include_file(mach/message.h                       HAVE_MACH_MESSAGE_H)
    check_include_file(mach/processor_set.h                 HAVE_MACH_PROCESSOR_SET_H)
    check_include_file(mach/processor.h                     HAVE_MACH_PROCESSOR_H)
    check_include_file(mach/processor_info.h                HAVE_MACH_PROCESSOR_INFO_H)
    check_include_file(mach/task.h                          HAVE_MACH_TASK_H)
    check_include_file(mach/thread_act.h                    HAVE_MACH_THREAD_ACT_H)
    check_include_file(mach/vm_region.h                     HAVE_MACH_VM_REGION_H)
    check_include_file(mach/vm_map.h                        HAVE_MACH_VM_MAP_H)
    check_include_file(mach/vm_prot.h                       HAVE_MACH_VM_PROT_H)
    check_include_file(mach/vm_statistics.h                 HAVE_MACH_VM_STATISTICS_H)
    check_include_file(mach/kern_return.h                   HAVE_MACH_KERN_RETURN_H)
    check_include_file(CoreFoundation/CoreFoundation.h      HAVE_COREFOUNDATION_COREFOUNDATION_H)
    check_include_file(IOKit/IOKitLib.h                     HAVE_IOKIT_IOKITLIB_H)
    check_include_file(IOKit/IOTypes.h                      HAVE_IOKIT_IOTYPES_H)
    check_include_file(IOKit/ps/IOPSKeys.h                  HAVE_IOKIT_PS_IOPSKEYS_H)
    check_include_file(IOKit/IOBSD.h                        HAVE_IOKIT_IOBSD_H)
    check_include_file(IOKit/storage/IOBlockStorageDriver.h HAVE_IOKIT_STORAGE_IOBLOCKSTORAGEDRIVER_H)
    check_include_file(IOKit/ps/IOPowerSources.h            HAVE_IOKIT_PS_IOPOWERSOURCES_H)
endfunction()
