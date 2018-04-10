#!/bin/bash

ssh_command()
{
  NODE=node-$1
  COMMAND=$2
  echo "ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no $NODE \"$COMMAND\""
}
 
############################## PRE PROCESSING ################################
#check and process arguments
REQUIRED_NUMBER_OF_ARGUMENTS=3
if [ $# -lt $REQUIRED_NUMBER_OF_ARGUMENTS ]
then
    echo "Usage: $0 <num_emulab_nodes> <master_node_id> <experiment suffix>"
    echo "./teardownk8s.sh 7 1 asterix.dcsq.emulab.net"
    exit 1
fi

NUM_EMULAB_NODES=$1
MASTER_NODE=$2
EXP_SUFFIX=$3
 
echo "Number of emulab nodes is $NUM_EMULAB_NODES"
echo "Master node is $MASTER_NODE"
echo "Experiment suffix is $EXP_SUFFIX"
echo ""

############################## PROCESSING ################################
#Removing worker nodes
echo "Removing master and worker nodes:"
for i in $(seq 1 $NUM_EMULAB_NODES)
do
        if [ "$i" = "$MASTER_NODE" ];
        then
            continue
        fi


        echo "Removing node-$i ..."
        DRAIN_COMMAND=$(ssh_command $MASTER_NODE "sudo kubectl drain node-$i.$EXP_SUFFIX --delete-local-data --force --ignore-daemonsets; sudo kubectl delete node node-$i.$EXP_SUFFIX")
        echo "$DRAIN_COMMAND"
        eval "$DRAIN_COMMAND"
        echo ""

        REMOVE_COMMAND=$( ssh_command $i "sudo kubeadm reset" )
        echo "$REMOVE_COMMAND"
        eval "$REMOVE_COMMAND"
        echo ""
done

echo "Removing node-$MASTER_NODE ..."
DRAIN_COMMAND=$(ssh_command $MASTER_NODE "sudo kubectl drain node-$MASTER_NODE.$EXP_SUFFIX --delete-local-data --force --ignore-daemonsets; sudo kubectl delete node node-$MASTER_NODE.$EXP_SUFFIX")
echo "$DRAIN_COMMAND"
eval "$DRAIN_COMMAND"
echo ""

REMOVE_COMMAND=$( ssh_command $MASTER_NODE "sudo kubeadm reset" )
echo "$REMOVE_COMMAND"
eval "$REMOVE_COMMAND"
echo ""
