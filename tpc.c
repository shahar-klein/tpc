
#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/mlx5/fs.h>
#include <linux/mlx5/device.h>
#include <linux/netdevice.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/random.h>
#include <linux/etherdevice.h>

#include "../core/en.h"

MODULE_LICENSE("GPL");

static struct kobject *tpc_obj;
//user input
enum someenum {
	GO = 0, /* 0 stop -  1 go*/ 
	IN_NIC,
	OUT_NIC,
	TEST_TYPE, /* 1=insert 2=update mode*/
	NUM_RULES,
	UPDATE_TIME, /* how long to mesure update */
	OUTPUT,
	TEST_SPEC,
	TEST_SPEC_NUM,
	TEST_SPEC_DELTA,
	MAX_ATTRS
};

struct sysfs_attr {
	struct kobj_attribute attr;
	char *name;
	char *default_val;
	char val[256];
};

static struct sysfs_attr attrs[MAX_ATTRS] = { 
	[GO] = { .name = "go", .default_val = "0" },
	[IN_NIC] = { .name = "in_nic", .default_val = "" },
	[OUT_NIC] = { .name = "out_nic", .default_val = "" },
	[TEST_TYPE] = { .name = "test_type", .default_val = "1" }, 
	[NUM_RULES] = { .name = "num_rules", .default_val = "100" },
	[UPDATE_TIME] = { .name = "test2_update_time", .default_val = "10000" }, 
	[OUTPUT] = { .name = "output", .default_val = "" },
	[TEST_SPEC] = { .name = "test2_spec", .default_val = "" },
	[TEST_SPEC_NUM] = { .name = "test2_spec_num_ops", .default_val = "0" },
	[TEST_SPEC_DELTA] = { .name = "test2_spec_delta", .default_val = "1000" },
};
static char *attr_val(int i) 
{
	if (attrs[i].val[0])
		return attrs[i].val;
	return attrs[i].default_val;

}

static int find_attr(struct kobj_attribute *attr) 
{
	int i = 0;

	for (i = 0; i < MAX_ATTRS; i++) {
		if (attr == &attrs[i].attr) return i;
	}

	return -1;
}



struct op {
	int op;
	int idx;
};

static struct op *ops = 0;
static int ops_size = 0;
static int ops_get_size = 0;
static int cur_op = 0;

static int read_test_spec(const char *buf, size_t count) {
	int op_idx = 0;
	int op = 0;
	char *p = 0;

	p = strchr(buf, '\n');
	if (!p) {
		return 0;
	}

	sscanf(buf, "%d, %d\n", &op,  &op_idx); 

	//printk(KERN_INFO "cur_op: %d, op: %d, op_idx %d\n", cur_op, op, op_idx);

	if (cur_op < ops_size) {
		ops[cur_op].op = op;
		ops[cur_op].idx = op_idx;
		cur_op++;
	}

	return (p-buf) + 1;
}

static ssize_t sysfs_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {

	int a = find_attr(attr);

	printk(KERN_INFO "%s", __func__);

	if (a < 0 || a > MAX_ATTRS)
		return 0;

	strcpy(buf, attr_val(a));
	return strlen(buf) +1;

}

static ssize_t sysfs_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int a = find_attr(attr);

	if (a < 0 || a > MAX_ATTRS || !count)
		return count;

	if (a == TEST_SPEC && ops) {
		return read_test_spec(buf, count);
	} else if (a == TEST_SPEC_NUM) {
		sscanf(buf, "%d", &ops_get_size);
		if (ops_get_size*2 < ops_size) // no need to alloc after the first run 
			return count;
		ops_size = ops_get_size*2;
		if (ops)
			vfree(ops);
		ops = vmalloc(sizeof(*ops) * ops_size);
		if (ops) {
			memset(ops, 0, sizeof(*ops) * ops_size);
			cur_op = 0;
		} else {
			printk(KERN_ERR "alloc ops failed");
		}

		return count;
	}

	memcpy(attrs[a].val, buf, count);
	attrs[a].val[count] = '\0';

	if (strlen(attrs[a].val)) {
		if (attrs[a].val[strlen(attrs[a].val)-1] == '\n') attrs[a].val[strlen(attrs[a].val)-1] = '\0';
		if (attrs[a].val[strlen(attrs[a].val)-1] == '\r') attrs[a].val[strlen(attrs[a].val)-1] = '\0';
	}

        return count;
}

static void init_specs(struct mlx5_flow_spec *specs, int num)
{
	unsigned char src_mac[ETH_ALEN] = { 0xe4, 0x1d, 0x2d, 0xfa, 0x80, 0x8e }; //e4:1d:2d:fa:80:8e
	unsigned char dst_mac[ETH_ALEN] = { 0xe4, 0x1d, 0x2d, 0xfa, 0x80, 0x8f }; //e4:1d:2d:fa:80:8f 
	uint32_t src_ip = ntohl(0x01010101); //1.1.1.1
	uint32_t dst_ip = ntohl(0x01010102); //1.1.1.2
	uint8_t ip_proto = IPPROTO_UDP;
	int total_count = 0;

	for (total_count = 0; total_count < num; total_count++) {
		struct mlx5_flow_spec *spec = &specs[total_count];
		void *headers_c = MLX5_ADDR_OF(fte_match_param, spec->match_criteria, outer_headers);
		void *headers_v = MLX5_ADDR_OF(fte_match_param, spec->match_value, outer_headers);

		memset(MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_c, dmac_47_16), 0xff, ETH_ALEN);
		memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v, dmac_47_16), dst_mac, ETH_ALEN);
		memset(MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_c, smac_47_16), 0xff, ETH_ALEN);
		memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v, smac_47_16), src_mac, ETH_ALEN);

		MLX5_SET(fte_match_set_lyr_2_4, headers_c, ethertype, ntohs(0xffff));
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ethertype, ETH_P_IP);

                memset(MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_c, src_ipv4_src_ipv6.ipv4_layout.ipv4), 0xff, sizeof(src_ip));
                memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v, src_ipv4_src_ipv6.ipv4_layout.ipv4), &src_ip, sizeof(src_ip));
                memset(MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_c, dst_ipv4_dst_ipv6.ipv4_layout.ipv4), 0xff, sizeof(dst_ip));
                memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v, dst_ipv4_dst_ipv6.ipv4_layout.ipv4), &dst_ip, sizeof(dst_ip));

                MLX5_SET(fte_match_set_lyr_2_4, headers_c, ip_protocol, 0xff);
                MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol, ip_proto);

		MLX5_SET(fte_match_set_lyr_2_4, headers_c, udp_sport, 0xffff);
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_sport, (total_count / 60000) + 1000);

		MLX5_SET(fte_match_set_lyr_2_4, headers_c, udp_dport, 0xffff);
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_dport, (total_count % 60000) + 1000);

		spec->match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	}
}

static int test_main(void)
{
	unsigned long ts_start, ts_end;
	struct mlx5_flow_spec *specs = 0;
	struct mlx5e_priv *priv, *priv_out;
	struct mlx5_flow_namespace *ns;
        struct mlx5_flow_table *ft;
	struct mlx5_flow_act flow_act = { .action = MLX5_FLOW_CONTEXT_ACTION_FWD_DEST };
	struct mlx5_flow_destination flow_dest;
	struct mlx5_flow_handle **rules;
	int rcnt = 0;
	struct mlx5_flow_spec *spec;
	void *headers_c;
	void *headers_v;
	unsigned int rule_count = 100;
	struct net_device *dev;
	struct net_device *dev_out;
	int i;
	unsigned long test_time = 10000;
	unsigned long delta_time = 1000;
	unsigned long tot_ops = 0;
	unsigned long delta_ops = 0;
	unsigned long timeout_to_end;
	unsigned long timeout_to_delta;
	int rv;


	printk(KERN_INFO "%s\n", __func__);

	rv = kstrtouint(attr_val(NUM_RULES), 0 , &rule_count);

	printk(KERN_INFO "running with %d rules\n", rule_count );

	specs = vmalloc(rule_count * sizeof(*specs));
	if (!specs) {
		printk(KERN_ERR "kcalloc specs\n");
		return -1;
	}
	memset(specs, 0, rule_count * sizeof(*specs)); 

	rules = vmalloc(rule_count * sizeof(*rules));
	if (!rules) {
		vfree(specs);
		printk(KERN_ERR "kcalloc rules\n");
		return -1;
	}
	memset(rules, 0, rule_count * sizeof(*rules)); 
	
	/* init all nececery data */
	printk(KERN_INFO "1\n");

	read_lock(&dev_base_lock);
	dev = first_net_device(&init_net);
	while (dev) {
		printk(KERN_INFO "found [%s]\n", dev->name);
		dev = next_net_device(dev);
	}
	read_unlock(&dev_base_lock);

	dev = dev_get_by_name(&init_net, attr_val(IN_NIC));
	if (dev == NULL) {
		printk(KERN_INFO "dev not found: %s\n", attr_val(IN_NIC));
		return -1;
	}
	dev_out = dev_get_by_name(&init_net, attr_val(OUT_NIC));
	if (dev_out == NULL) {
		printk(KERN_INFO "out dev not found: %s\n", attr_val(OUT_NIC));
		dev_put(dev);
		return -1;
	}

	printk(KERN_INFO "2\n");

	priv = netdev_priv(dev); 
	priv_out = netdev_priv(dev_out); 
	ns = mlx5_get_flow_namespace(priv->mdev,
				     MLX5_FLOW_NAMESPACE_KERNEL);
	if (!ns) {
		printk(KERN_ERR "Failed mlx5_get_flow_namespace.\n");
		goto cleanup_dev;
	}

	ft = mlx5_create_auto_grouped_flow_table(ns, 0, 16*1024*192, 2, 0, 0);
	if (IS_ERR(ft)) {
		printk(KERN_ERR "Failed create flow table.\n");
		goto cleanup_dev;
	}
	printk(KERN_INFO "init specs:\n");
	// init rules table
	init_specs(specs, rule_count);
	printk(KERN_INFO "init specs done\n");
	flow_dest.type = MLX5_FLOW_DESTINATION_TYPE_TIR;
	flow_dest.tir_num = priv_out->indir_tir[0].tirn;

	// take time
	ts_start = jiffies;

	// add rules
	for (rcnt=0 ; rcnt < rule_count; rcnt++) {
		spec = &specs[rcnt];
		if ( rcnt % 1000 == 0)
			printk(KERN_INFO "rcnt: %d\n", rcnt);

		rules[rcnt] = mlx5_add_flow_rules(ft, spec, &flow_act, &flow_dest, 1);
		if (IS_ERR(rules[rcnt])) {
			printk(KERN_ERR "Failed adding rule.\n");
			goto cleanup_rules;
		}
	}
	
	// take time
	ts_end = jiffies;
	// report
	printk(KERN_INFO "complete insert %d rules. ramp up time: %u\n", rcnt, jiffies_to_usecs(ts_end - ts_start));

	printk(KERN_INFO "Sleeping 20\n");
	msleep(20000);
	printk(KERN_INFO "Done Sleeping 20\n");

	if (attr_val(TEST_TYPE)[0] == '1') 
		goto cleanup_rules;


	rv = kstrtoul(attr_val(UPDATE_TIME), 0 , &test_time);
	rv = kstrtoul(attr_val(TEST_SPEC_DELTA), 0 , &delta_time);
	

	timeout_to_end = jiffies + msecs_to_jiffies(test_time);
	timeout_to_delta = jiffies + msecs_to_jiffies(delta_time);


	printk(KERN_INFO "running phase 2\n");
	printk(KERN_INFO "running time %lu msecs. delta %lu msecs\n", test_time, delta_time);

	if (!ops) {
		printk(KERN_INFO "can't run phase 2\n");
	}

	for (;;) {
		if (jiffies > timeout_to_end) {
			printk(KERN_INFO "in %lu mseconds, completed %lu ops", test_time, tot_ops);
			goto cleanup_rules;
		}
		if (!cur_op)
			goto cleanup_rules;
		for (i = 0; i < cur_op;  i++) {
			struct op *op = &ops[i];

			if (jiffies > timeout_to_end) {
				printk(KERN_INFO "in %lu mseconds, completed %lu ops", test_time, tot_ops);
				goto cleanup_rules;
			}

			if (jiffies > timeout_to_delta) {
				unsigned long print_t = jiffies;

				printk(KERN_INFO "in %lu mseconds, completed %lu ops", delta_time, delta_ops);

				timeout_to_delta = jiffies + msecs_to_jiffies(delta_time);
				delta_ops = 0;

				timeout_to_end += print_t - jiffies;
			}

			if (op->op) { //delete
				mlx5_del_flow_rules(rules[op->idx]);
				rules[op->idx] = 0;
			} else {
				spec = &specs[op->idx];
				rules[op->idx] = mlx5_add_flow_rules(ft, spec, &flow_act, &flow_dest, 1);
				if (IS_ERR(rules[rcnt])) {
					printk(KERN_ERR "Failed adding rule.\n");
					goto cleanup_rules;
				}
			}

			tot_ops++;
			delta_ops++;
		}	
	}
		
cleanup_rules:
#if 0
/* This block will dump the HW */
	{
		char * envp[] = {"HOME=/", "TERM=linux","PATH=/sbin:/usr/sbin:/bin:/usr/bin",NULL};
		char * argv[] = {"/bin/bash","-c","/usr/bin/mlxdump -d /dev/mst/mt4121_pciconf0 fsdump --type FT --no_zer=TRUE > /tmp/1_rule",NULL};
		printk(KERN_INFO "Dumping.....\n");
		call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
		printk(KERN_INFO "Dumping.....Done\n");
	}
#endif

	printk(KERN_INFO "cleanup %d rules\n", rcnt);
	while (rcnt)
		if (rules[--rcnt])
			mlx5_del_flow_rules(rules[rcnt]);
cleanup_ft: 
	printk(KERN_INFO "destroy table\n");
	mlx5_destroy_flow_table(ft);
cleanup_dev:
	printk(KERN_INFO "cleanup devs\n");
	dev_put(dev);
	dev_put(dev_out);

	vfree(specs);
	vfree(rules);

	printk(KERN_INFO "test done.\n");
	
	return 0;
}

static int test_GO(void *data)
{
	while (!kthread_should_stop()) {	
		if (attr_val(GO)[0] == '1') {
			test_main();
			strcpy(attrs[GO].val, "0");
		}

		schedule();
	}

	return 0;
}

static int init_sysfs(void)
{
	int error = 0; 
	int i = 0;

	tpc_obj = kobject_create_and_add("tpc", kernel_kobj);
	if(!tpc_obj)
		return -ENOMEM;

	for (i = 0; i < MAX_ATTRS; i++) {
		struct kobj_attribute temp = {
			.attr = { .name =  attrs[i].name, 
				  .mode = VERIFY_OCTAL_PERMISSIONS(0660) },		
			.show	= sysfs_show,
			.store	= sysfs_store,						
		};

		attrs[i].attr = temp;

		error = sysfs_create_file(tpc_obj, &attrs[i].attr.attr);
		if (error) {
			pr_debug("failed to create the foo file in /sys/kernel/tpc/%s\n",  attrs[i].name);
			return error;
		}

	}

	return 0;
}

static struct task_struct *test_task;

int init_module(void)
{
	if (init_sysfs())
		return -1;
	printk(KERN_INFO "starting thread\n");

	test_task = kthread_run(test_GO, NULL, "tpc test");

	return 0;
}

void cleanup_module(void)
{
	kthread_stop(test_task);
	kobject_del(tpc_obj);
	printk(KERN_INFO "bye bye\n");
}
