#!/bin/bash

modprobe pblk

#ssd settings
SIZE=64
CH=16
CHIPS_PER_CH=2
CHIPS_TOTAL=$(($CH*$CHIPS_PER_CH))
SIZE_PER_CH=$(($SIZE/$CH))

#partition settings
DEV_NAME="mydev"
DEV_MOUNT_PATH="/media/nvme/"
DEV_DATA_PATH="/pblk-cast_perf/"

ARGS=("$@")

begin=0
function create_dev {
    ch=$1
    name=$2
    end=$((${begin}+${ch}))
    lun_b=$((${begin}*2))
    lun_e=$((${end}*2-1))
    echo "[device name] ${name}"
    echo "[info] ch:$ch, chips:$(($lun_e-$lun_b+1)), size:$((ch*$SIZE_PER_CH))GB, lun:($lun_b ~ $lun_e)"

    #directory check
    if [ ! -d $DEV_MOUNT_PATH$name ]; then
        mkdir $DEV_MOUNT_PATH$name
    fi    
    
    #data file check
    if [ -d "$DEV_DATA_PATH$name.data" ]; then
        rm "$DEV_DATA_PATH$name.data"
    fi

    sudo nvme lnvm create -d nvme0n1 --lun-begin=${lun_b} --lun-end=${lun_e} -n ${name} -t pblk -f
    sudo mkfs -t ext4 /dev/${name}
    sudo mount /dev/${name} $DEV_MOUNT_PATH$name

    begin=$(($end))
}

#main

#no argment
if [[ $# == 0 ]] ; then
    ARGS=($CH)
fi

#check # of channels
sum=0
for ch in "${ARGS[@]}"; do
    if [[ $ch -eq 0 ]] ; then
    echo "0 is invalid argment"
    exit 0
    fi
    sum=$(($sum+$ch))
done

if [[ $sum -gt $CH ]] ; then
    echo "You have requested too many channels($sum)."
    echo "(max $CH)"
    exit 0
fi
echo "total channels ${sum}"

#
echo        "+--------------------------------------------------------------"
for (( i=0; i<${#ARGS[@]}; i++)) ; do
    create_dev ${ARGS[i]} ${DEV_NAME}$i
    echo    "+--------------------------------------------------------------"
done
