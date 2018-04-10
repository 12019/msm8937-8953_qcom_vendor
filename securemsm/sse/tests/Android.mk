ifeq ($(call is-board-platform-in-list, msm8937 msm8953 msm8996 msmcobalt),true)
  include $(call all-subdir-makefiles)
endif

