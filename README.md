# tpc
tpc is a 2 phase module to test and benchmark insertion and update rate of flows into the NIC
In phase one rules are inserted to reach steady state. 
In the second phase we roll around the steady state adding and deleting rules.
The module is controlled via sysfs. /sys/kernel/tpc/*

Is is possible to run only phase 1 without continuing to phase 2.

To run the test:

1.	Build and load the module
2.	Set number of rules in steady state via num_rules
3.	Set the nic In which the rules will be written to via in_nic 
4.	Set the nic in which the action qp will be taken from via out_nic (use the  in_nic value if you are not sure) 
5.	Set the time the test will role rules around the steady state via test2_update_time(in millisecond)(default 10 seconds)
6.	Set the delta time for the module to report updates per delta via test2_spec_delta(in millisecond)(default 1000 millisecond)
7.	Set the rules chunk size that will roll around the steady state via test2_spec_num_ops
8.	Pass an array of  pairs of (op, idx) to the module. This array will be cycled by the module via test2_spec. The easiest way to do this is to run a helper script and redirect the output to test2_spec. The helper script can get a seed and so give deterministic arrays of ops. Example: 
python ./tpc_update_array.py -R 4000 -C 400 -S 5  > /sys/kernel/tpc/test2_spec
9.	Set the test type to run phase 1 or phase 1+2 via test_type. 1 – only phase one 2 – phase 1+2
10.	Run the test via go – set to 1. 

