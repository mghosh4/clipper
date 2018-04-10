#!/bin/bash

ssh_command()
{
  NODE=node-$1
  COMMAND=$2
  echo "ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no $NODE \"$COMMAND\""
}
 
############################## PRE PROCESSING ################################
#check and process arguments
REQUIRED_NUMBER_OF_ARGUMENTS=2
if [ $# -lt $REQUIRED_NUMBER_OF_ARGUMENTS ]
then
    echo "Usage: $0 <num_emulab_nodes> <master_node_id>"
    exit 1
fi

NUM_EMULAB_NODES=$1
MASTER_NODE=$2
 
echo "Number of emulab nodes is $NUM_EMULAB_NODES"
echo "Master Node is $MASTER_NODE"
echo ""

############################## PROCESSING ################################
#Initializing kubeadm
echo "Initializing kubeadm at master"
echo ""
INIT_COMMAND=$( ssh_command $MASTER_NODE "sudo kubeadm init" )
echo $INIT_COMMAND
echo ""

INIT_COMMAND_OUTPUT=$( eval "$INIT_COMMAND" )
echo "${INIT_COMMAND_OUTPUT}"
echo ""

#Setting up your user with kubernetes
echo "Setting up your username with kubernetes"
echo ""
SETUP_USER_COMMAND=$( ssh_command $MASTER_NODE "mkdir -p \$HOME/.kube; sudo cp /etc/kubernetes/admin.conf \$HOME/.kube/config; sudo chown \$(id -u):\$(id -g) \$HOME/.kube/config" )
echo "$SETUP_USER_COMMAND"
echo ""
eval "$SETUP_USER_COMMAND"
echo""

#Installing pod network
echo "Installing pod network"
echo ""
POD_NETWORK_COMMAND=$( ssh_command $MASTER_NODE "sudo sysctl net.bridge.bridge-nf-call-iptables=1; export kubever=$(kubectl version | base64 | tr -d '\n'); kubectl apply -f 'https://cloud.weave.works/k8s/net?k8s-version=\$kubever'" )
echo "$POD_NETWORK_COMMAND"
echo ""
eval "$POD_NETWORK_COMMAND"
echo ""
 
#Setting up worker nodes
echo "Setting up worker nodes nodes:"
for i in $(seq 1 $NUM_EMULAB_NODES)
do

        if [ "$i" = "$MASTER_NODE" ];
        then
            continue
        fi

        echo "Setting up node-$i ..."
        JOIN_COMMAND=`echo "${INIT_COMMAND_OUTPUT}" | tail -1`
        echo "$JOIN_COMMAND"

        JOIN_RUN_COMMAND=$( ssh_command $i "sudo $JOIN_COMMAND" )
        echo $JOIN_RUN_COMMAND
        eval "$JOIN_RUN_COMMAND"
        echo ""
done

############################## VERIFYING  ################################
echo "Testing whether all nodes joined"
echo ""
GET_NODE_COMMAND=$( ssh_command $MASTER_NODE "kubectl get nodes" )
sleep 5
eval "$GET_NODE_COMMAND"
