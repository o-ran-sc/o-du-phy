#!/bin/bash


###Check environment K8S_SRIOV_DP (0 or 1, default 0), K8S_CPU_MANAGER (0 or 1, default 0), XRAN_DIR before running this script

###Parameters:

###$1 - used to specify sriov net devices to run XRAN sample app.
###The input will be different depending on k8s sriov device plugin enabled or not.
###If k8s sriov net device plugin is enabled, container will request integral sriov VF devices ($1) from k8s,
###which is specified in the pod configuration file like - intel.com/intel_sriov_dpdk: "2".
###Then the input of this parameter would be number like 2, or 4.
###Else this parameter $1 will be sriov net devices PCIe address list like "18:02.0 18:02.1"

###$2 - Test case index. Like "mu3_100mhz"

###$3 - "du" or "ru"

###$4 - Instance/wls ID. Like "1", "2", or "3".

###$5 - Used to assign core affinity to different threads of XRAN sample app.
###If this parameter input is "0", k8s CPU Manager is enabled, and this script will get core mapping from the container taskset affinity,
###which is assigned by the CPU Manager.
###Else this parameter input should be taskset core mask assigned by user (in Hex format, but removing 0x in the head), like "0000f0"

cd $XRAN_DIR

#########Assigned PCIe VF for container
vf_num=$1
#########Test Case index
case_index=$2
########du or ru?
mode_du_ru=$3
########instance/wls ID
wls_id=$4
########taskset core mask
taskset_str=$5

###########Get SRIOV VF PCI addresses if K8S_SRIOV_DP enabled or not
array_pci_list=""
if [ $K8S_SRIOV_DP ]; then
    env_pci_list=$(env | grep -i PCIDEVICE | grep -i DPDK | awk -F= '{print $2}')

    let i=1
    while [ $i -le $vf_num ]
    do
        addr=$(echo $env_pci_list | awk -F, '{print $'$i'}')
        array_pci_list="$array_pci_list $addr"
        let i++
    done
else
   array_pci_list=$vf_number
fi

#####Specify which test case to run and the VF number
sed -i "s/\.\/build\/sample-app .*/\.\/build\/sample-app \.\/usecase\/$case_index\/config_file_o_${mode_du_ru}.dat $array_pci_list/" $XRAN_DIR/app/run_o_${mode_du_ru}.sh

####Update some parameters for test case
sed -i "s/debugStop=.*/debugStop=0/" app/usecase/${case_index}/config_file_o_${mode_du_ru}.dat
sed -i "s/CPenable=.*/CPenable=1/" app/usecase/${case_index}/config_file_o_${mode_du_ru}.dat
sed -i "s/instanceId=.*/instanceId=${wls_id}/" app/usecase/${case_index}/config_file_o_${mode_du_ru}.dat

####Get core ID from core mask
if [ $K8S_CPU_MANAGER ]; then
    taskset_str=$(echo $(taskset -p 1) | sed 's/.*affinity mask: \(.*\)/\1/')
fi
let core_map="0x$taskset_str"
let core_mask=1
let first_core_mask=$((core_map & core_mask))
let first_core_num=0


#####Get first core ID for system thread
let max_cpu=64
while [ $first_core_mask -eq 0 ]
do
    if [ $max_cpu -eq 0 ]; then
        echo "ERROR: provided core mask is wrong in your system. Please check."
    fi
    core_mask=$(($core_mask+$core_mask))
    first_core_num=$(($first_core_num+1))
    echo core_mask $core_mask
    echo first_core_num $first_core_num
    first_core_mask=$((core_map & core_mask))
    echo first_core_mask=$first_core_mask
    max_cpu=$(($max_cpu-1))
done

system_core_num=$first_core_num

core_map=$(($core_map-$core_mask))
if [ $core_map -eq 0 ]
then
    echo "ERROR: require at least two cores for O-DU/RU sample app"
    exit
fi

#####Get the second core ID for timing thread
first_core_mask=0
while [ $first_core_mask -eq 0 ]
do
    if [ $max_cpu -eq 0 ]; then
        echo "ERROR: provided core mask is wrong in your system. Please check."
    fi

    core_mask=$(($core_mask+$core_mask))
    first_core_num=$(($first_core_num+1))
    echo core_mask $core_mask
    echo first_core_num $first_core_num
    first_core_mask=$((core_map & core_mask))
    echo first_core_mask=$first_core_mask
    max_cpu=$(($max_cpu-1))
done

timing_core_num=$first_core_num
####Currently only use 2 cores to run O-DU/RU sample app
sed -i "s/ioCore.*/ioCore=${system_core_num}/" app/usecase/${case_index}/config_file_o_${mode_du_ru}.dat
sed -i "s/pktAuxCore.*/pktAuxCore=${system_core_num}/" app/usecase/${case_index}/config_file_o_${mode_du_ru}.dat
sed -i "s/pktProcCore.*/pktProcCore=${system_core_num}/" app/usecase/${case_index}/config_file_o_${mode_du_ru}.dat
sed -i "s/systemCore.*/systemCore=${system_core_num}/" app/usecase/${case_index}/config_file_o_${mode_du_ru}.dat
sed -i "s/timingCore.*/timingCore=${timing_core_num}/" app/usecase/${case_index}/config_file_o_${mode_du_ru}.dat

cd app
sh run_o_${mode_du_ru}.sh

