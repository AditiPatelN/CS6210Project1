#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#define MIN(a, b) ((a) < (b) ? a : b)
#define MAX(a, b) ((a) > (b) ? a : b)

int is_exit = 0;  // DO NOT MODIFY THE VARIABLE
double *vcpu_mem; // Array to track vCPU memory for each domain
static int mem_allocated = 0;

void MemoryScheduler(virConnectPtr conn, int interval);

/*
DO NOT CHANGE THE FOLLOWING FUNCTION
*/
void signal_callback_handler()
{
	printf("Caught Signal");
	is_exit = 1;
}

/*
DO NOT CHANGE THE FOLLOWING FUNCTION
*/
int main(int argc, char *argv[])
{
	virConnectPtr conn;

	if (argc != 2)
	{
		printf("Incorrect number of arguments\n");
		return 0;
	}

	// Gets the interval passes as a command line argument and sets it as the STATS_PERIOD for collection of balloon memory statistics of the domains
	int interval = atoi(argv[1]);

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL)
	{
		fprintf(stderr, "Failed to open connection\n");
		return 1;
	}

	signal(SIGINT, signal_callback_handler);

	while (!is_exit)
	{
		// Calls the MemoryScheduler function after every 'interval' seconds
		MemoryScheduler(conn, interval);
		sleep(interval);
	}

	// Close the connection
	virConnectClose(conn);
	return 0;
}

/*
COMPLETE THE IMPLEMENTATION
*/
void MemoryScheduler(virConnectPtr conn, int interval)
{
	conn = virConnectOpen("qemu:///system");
	if (conn == NULL)
	{
		fprintf(stderr, "Failed to connect to the hypervisor");
		// return 1;
	}
	// Declare variables for node and domain
	virNodeInfo node_info;
	virDomainInfo domain_info;
	virDomainPtr domain;
	double threshold = 0.85; // 85% is out threshold
	double physical_available_mem;
	// get the number of pCPUs-global variable
	if (virNodeGetInfo(conn, &node_info) < 0)
	{
		fprintf(stderr, "Failed to get node information\n");
	}
	else
	{
		physical_available_mem = (node_info.memory / 1024); // MB
		// double physical_max = (node_info.maxMem / 1024);
		printf("Physical total available memory = : %f\n", physical_available_mem);
		// printf("Physical max available memory = : %f\n", physical_max);
	}
	// get the number of active domain(VM)
	int num_domain = virConnectNumOfDomains(conn); // number of active domains/VMs
	if (num_domain > 0)
	{
		int *activeDomains = (int *)malloc(sizeof(int) * num_domain);
		int numDomainList = virConnectListDomains(conn, activeDomains, num_domain);
		if (numDomainList < 0)
		{
			fprintf(stderr, "Failed to get the list of active domains\n");
		}
		else
		{
			// initially assign memory allocated to 0
			if (!mem_allocated)
			{
				vcpu_mem = (double *)calloc(num_domain, sizeof(double));
				mem_allocated = 1;
			}
			printf("Active Virtual Machines: \n");
			// memory_coordinator
			for (int i = 0; i < num_domain; i++)
			{
				domain = virDomainLookupByID(conn, activeDomains[i]);
				if (domain == NULL)
				{
					fprintf(stderr, "Failed to find domain with ID %d\n", activeDomains[i]);
				}
				else
				{
					if (virDomainGetInfo(domain, &domain_info) < 0)
					{
						fprintf(stderr, "Failed to goet doman info\n");
					}
					else
					{
						double domain_max_mem = (double)(virDomainGetMaxMemory(domain)) / 1024; // MB
						double unused_mem;
						double balloon_usable; // How much the balloon can be inflated without pushing the guest system to swap, corresponds to 'Available' in /proc/meminfo
						double node_total;	   // total memory usage
						double node_free;
						printf("Max memory for domain %s = %f\n", virDomainGetName(domain), domain_max_mem);
						double threshold_memory = (unsigned long)(threshold * domain_max_mem);
						printf("Threshold memory for domain %s = %f\n", virDomainGetName(domain), threshold_memory);
						// Memory stats for Node
						virNodeMemoryStatPtr node_stats = calloc(VIR_NODE_MEMORY_STATS_ALL, sizeof(*node_stats));
						int nparam = VIR_NODE_MEMORY_STATS_ALL;
						int node_mem_stats = virNodeGetMemoryStats(conn, 0, node_stats, &nparams, 0);
						if (node_mem_stats < 0)
						{
							fprintf(stderr, "Failed to get node stats\n");
							free(node_stats);
						}
						else
						{
							// assign node stats
							for (int a = 0; a < VIR_NODE_MEMORY_STATS_ALL; a++)
							{
								if (node_stats[a].tag == VIR_NODE_MEMORY_STATS_TOTAL)
								{
									node_total = node_stats[a].val;
									printf("node total usage for domain %s = %f\n", virDomainGetName(domain), node_total);
								}
								else if (node_stats[a].tag == VIR_NODE_MEMORY_STATS_FREE)
								{
									node_free = node_stats[a].val;
									printf("node free for domain %s = %f\n", virDomainGetName(domain), node_free);
								}
							}
						}
						// memory stats for Domain
						virDomainMemoryStatPtr domain_stats = calloc(VIR_DOMAIN_MEMORY_STAT_NR, sizeof(*domain_stats));
						int mem_stats = virDomainMemoryStats(domain, domain_stats, VIR_DOMAIN_MEMORY_STAT_NR, 0);
						if (mem_stats < 0)
						{
							fprintf(stderr, "Failed to get memory stats\n");
						}
						else
						{
							double mem_used = (domain_info.memory) / 1024;
							printf("MEMORY USED from domain_info = %f\n", mem_used);
							// int rss_index = VIR_DOMAIN_MEMORY_STAT_RSS;
							// if (rss_index >= 0 && rss_index < VIR_DOMAIN_MEMORY_STAT_NR)
							// {
							// 	printf("inside the if statement for mem_use\n");

							// 	mem_used = (stats[rss_index].val) / 1024; // new mem over interval- MB
							// 	printf("memory used from VirDomainMemoryStat for domain %s = %f\n", virDomainGetName(domain), mem_used);
							// }
							for (int j = 0; j < mem_stats; j++)
							{
								if (domain_stats[j].tag == VIR_DOMAIN_MEMORY_STAT_UNUSED)
								{
									unused_mem = (domain_stats[j].val) / 1024; // in MB
								}
								else if (domain_stats[j].tag == VIR_DOMAIN_MEMORY_STAT_USABLE)
								{
									balloon_usable = (domain_stats[j].val) / 1024;
								}
							}

							double mem_diff = mem_used - vcpu_mem[i];
							printf("memory difference for domain %s = %f\n", virDomainGetName(domain), mem_diff);
							double mem_utilization = ((mem_diff) / (interval * 1000)) * 100; // convert the diff to MB - interval in ms
							printf("memory utilization for domain %s = %.2f%%\n", virDomainGetName(domain), mem_utilization);
							vcpu_mem[i] = mem_used;
							if (unused_mem == 0)
							{
								printf("Inside the unused for loop");
								unused_mem = domain_max_mem - mem_used;
							}
							// unused_mem = domain_max_mem - mem_used;
							printf("ballon memory for domain %s = %f\n", virDomainGetName(domain), balloon_usable);
							printf("unused memory for domain %s = %f\n", virDomainGetName(domain), unused_mem);
							double unused_threshold = (0.4 * mem_used); // 40% * max_used
							printf("unused threshold percentage for domain %s = %f\n", virDomainGetName(domain), unused_threshold);
							// Allocation when unused <= 100MB; make sure allocated memory to VM > 2048MB
							// Decallocation when unused > 40% maxMemory or if used == maxMemory
							// double new_mem_allocation;
							// ACTUAL mem = mem_used
							// keep allocating mem till actual memory == 2048MB - 150 MB per iteration
							// once it hits 2GB and large unused memory is observed - start gradually taking away
							// when the difference between used and unused get more than 1.5 GB big - gradually decrease
							if (unused_mem <= 200 && mem_used != domain_max_mem)
							{ // or 100
								printf("Allocating memory\n");
								double new_mem_allocation = mem_used + 75; // give memory in gradually
								if (virDomainSetMemory(domain, new_mem_allocation) == -1)
								{
									fprintf(stderr, "domainSetMemory allocation failed%f\n", new_mem_allocation);
								}
								else
								{
									printf("Added memory for domain %s to %f MB\n", virDomainGetName(domain), new_mem_allocation);
								}
							}
							else if ((mem_used == domain_max_mem) || (unused_mem > 250))
							{
								printf("Deallocating memory\n");
								double new_mem_allocation = domain_max_mem - 75; // gradually decreasing
								if (virDomainSetMemory(domain, new_mem_allocation) == -1)
								{
									fprintf(stderr, "domainSetMem deallocation failed %f\n", new_mem_allocation)
								}
								else
								{
									printf("Reduced memory for domain %s to %f MB\n", virDomainGetName(domain), new_mem_allocation);
								}
							}
							else
							{
								printf("No change in memory\n");
							}

							// virDomainSetMemory(domain, new_mem_allocation);

							// double unused_percentage = (double)(unused_mem / domain_max_mem) * 100;
							// printf("Unused percentage for domain %s = %.2f%%\n", virDomainGetName(domain), unused_percentage);
							// if (unused_mem > unused_threshold)
							// {
							// 	unsigned long reduction_memory = (unsigned long)(0.6 * domain_max_mem); // take away 60% memory back
							// 	unsigned long new_mem_allocation = domain_max_mem - reduction_memory;
							// 	virDomainSetMemory(domain, new_mem_allocation);
							// 	printf("Reduced memory for domain %s to %lu bytes\n", virDomainGetName(domain), new_mem_allocation);
							// }
							// // else if ((mem_utilization < threshold_memory) || (mem_used == domain_max_mem)) // mem_used or utilization
							// else if (mem_used == domain_max_mem) // mem_used or utilization

							// {
							// 	// Calculate the memory needed to reach threshold (20% above current memory usage or to the threshold, whichever is smaller)
							// 	unsigned long reduction_memory = (unsigned long)(0.2 * mem_used);
							// 	unsigned long new_memory_allocation = domain_max_mem - reduction_memory;
							// 	printf("NEW MEMORY ALLOCATION for domain %s = %lu\n", virDomainGetName(domain), new_memory_allocation);
							// 	unsigned long min_memory = 256 * 1024; // min memory is 512MB or 256
							// 	if (new_memory_allocation < mem_used)
							// 	{
							// 		printf("new memory < mem_used\n");
							// 		new_memory_allocation = mem_used + buffer;
							// 	}
							// 	else if (new_memory_allocation < min_memory)
							// 	{
							// 		printf("new memory < min_memory\n");
							// 		new_memory_allocation = min_memory;
							// 	}
							// 	else if (new_memory_allocation > domain_max_mem)
							// 	{
							// 		printf("new memory > domain_max_mem\n");
							// 		new_memory_allocation = domain_max_mem;
							// 	}
							// 	else
							// 	{
							// 		printf("didn't pass any if statements\n");
							// 		new_memory_allocation = new_memory_allocation;
							// 	}
							// 	// check if there is enough physical memory available
							// 	// if (new_memory_allocation <= physical_available_mem)
							// 	// { // LOOK INTO THIS
							// 	printf("NEW MEMORY ALLOCATION for domain %s after if statement = %lu\n", virDomainGetName(domain), new_memory_allocation);
							// 	virDomainSetMemory(domain, new_memory_allocation);
							// 	printf("Reduced memory for domain %s to %lu bytes\n", virDomainGetName(domain), new_memory_allocation);
							// 	// }
							// 	// else
							// 	// {
							// 	// 	printf("Not enough physical memory to reduce for domain %s\n", virDomainGetName(domain));
							// 	// }
							// }
							// else if (unused_mem < (0.20 * domain_max_mem))
							// {
							// 	// you need more memory for the VM to deflate the balloon - assign max memory
							// 	unsigned long new_mem_alloc = mem_used + (0.10 * domain_max_mem);
							// 	virDomainSetMemory(domain, new_mem_alloc);
							// 	printf("Increased memory to  %s to %lu bytes\n", virDomainGetName(domain), new_mem_alloc);
							// }
						}
					}
				}
			}
		}
	}
}
