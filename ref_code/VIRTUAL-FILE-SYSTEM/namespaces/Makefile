DEVICE = ./blk-dev

create-dev-fs:
	dd if=/dev/zero of=$(DEVICE) bs=4096 count=100
	mkfs.ext4 $(DEVICE)

mount:
	mkdir ./temp
	mount -t ext4 $(DEVICE) ./temp
	touch ./temp/example-file
	echo "francesco" > ./temp/example-file

unmount:
	umount ./temp
	rm -rf ./temp
	rm $(DEVICE)

unshare:
	unshare --mount -- /bin/bash

