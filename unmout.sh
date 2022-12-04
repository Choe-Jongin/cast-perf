#!/bin/bash

devs=($(df | awk '{print $1}'))

for (( i=0; i<${#devs[@]}; i++)) ; do
	dev=${devs[i]}
	if [[ "${dev}" != *mydev* ]] ; then
		continue
	fi

	dev=${dev:5}
	echo "unmount $dev"
	sudo umount /media/nvme/$dev
	sudo nvme lnvm remove -n $dev
done

echo "factory fotmat..."
sudo nvme lnvm factory -d nvme0n1
