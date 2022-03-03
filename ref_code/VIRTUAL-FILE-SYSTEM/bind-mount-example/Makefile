
MOUNT_POINT = bind-mount-point

mount:
	cd /; mkdir $(MOUNT_POINT); mount --bind /home $(MOUNT_POINT)

unmount:
	cd / ; umount $(MOUNT_POINT) ; rmdir $(MOUNT_POINT)
