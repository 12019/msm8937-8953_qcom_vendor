ifneq ($(TARGET_BLUR_KERNEL),true)
   ifneq ($(TARGET_SCVE_DISABLED),true)
       include $(call all-subdir-makefiles)
   endif
endif

