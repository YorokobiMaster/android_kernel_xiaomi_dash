=========================================
Android first-stage orange bootconfig view
=========================================

``CONFIG_ANDROID_DSU_AVB_BYPASS`` prepends
``androidboot.verifiedbootstate = "orange"`` to ``/proc/bootconfig`` reads
by PID 1 until its first exec of ``/system/bin/init``. Other readers see
the original bootconfig. Despite its historical name, this helper does
not require a DSU boot.

The orange view lets first-stage Android AVB handling accept verification
errors. On dash, it was tested with verification-disabled on-disk vbmeta
and the original AVB fstab. The helper does not rewrite vbmeta reads or
storage, change SELinux enforcement, or affect bootloader checks that
run before the kernel.
