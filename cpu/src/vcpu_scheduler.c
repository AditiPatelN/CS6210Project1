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

int is_exit = 0; // DO NOT MODIFY THIS VARIABLE
// Added global variables for persistent code
typedef struct
{
	virDomainPtr domain_id;
	int vcpu_id;
	unsigned char cpumap;
	double utilization;
	int assigned_pcpu;
} vcpuMapping;
int num_pcpu = 4;		   // will dynamically assign in CPUScheduler()
int current_pcpu = 0;	   // all vCPUs are pinned to pCPU 0 in the beginning
unsigned long *vcpu_times; // Array to track vCPU times for each domain
static int mem_allocated = 0;
static int first_iteration = 1;
// static int is_modified = 0;
// have global variables that keeps track of the time- to get the cpu percentage
void CPUScheduler(virConnectPtr conn, int interval);

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

	// Get the total number of pCpus in the host
	signal(SIGINT, signal_callback_handler);

	while (!is_exit)
	// Run the CpuScheduler function that checks the CPU Usage and sets the pin at an interval of "interval" seconds
	{
		CPUScheduler(conn, interval);
		sleep(interval);
	}

	// Closing the connection
	virConnectClose(conn);
	// return 0;
}

/* COMPLETE THE IMPLEMENTATION */
void CPUScheduler(virConnectPtr conn, int interval)
{
	// domain pointer variable
	//  virDomainPtr domain;
	//  int stats;
	// connects to the hypervisor
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
	int maplen;
	unsigned char cpumaps;
	// check if there is uneven workload
	int is_uneven = 0;
	// get the number of pCPUs-global variable
	if (virNodeGetInfo(conn, &node_info) < 0)
	{
		fprintf(stderr, "Failed to get node information\n");
	}
	else
	{
		num_pcpu = node_info.cpus;
		maplen = VIR_CPU_MAPLEN(num_pcpu);
		printf("Number of pCPU on the host: %d\n", num_pcpu);
	}
	// get the number of active domain(VM)
	int num_domain = virConnectNumOfDomains(conn); // number of active domains/VMs
	if (num_domain > 0)
	{
		vcpuMapping vcpu_mappings[num_domain];
		int *activeDomains = (int *)malloc(sizeof(int) * num_domain);
		int numDomainList = virConnectListDomains(conn, activeDomains, num_domain);
		if (numDomainList < 0)
		{
			fprintf(stderr, "Failed to get the list of active domains\n");
		}
		else
		{
			printf("Active Virtual Machines: \n");
			// initially assign vcpu times to 0
			if (!mem_allocated)
			{

				for (int a = 0; a < num_domain; a++)
				{
					// vcpu_mappings[a].vcpu_time = 0;
					vcpu_mappings[a].utilization = 0;
				}
				vcpu_times = (unsigned long *)calloc(num_domain, sizeof(unsigned long));
				mem_allocated = 1;
			}

			for (int i = 0; i < numDomainList; i++)
			{

				domain = virDomainLookupByID(conn, activeDomains[i]);
				if (domain == NULL)
				{
					fprintf(stderr, "Failed to find domain by ID");
				}
				else
				{
					vcpu_mappings[i].domain_id = domain; // domain id
					vcpu_mappings[i].vcpu_id = 0;		 // all domains only have 1 vcpu
					if (virDomainGetInfo(domain, &domain_info) == 0)
					{
						unsigned long new_time = domain_info.cpuTime; // new time for domain
						printf("new vcpu time %lu\n", new_time);
						unsigned long time_diff = new_time - vcpu_times[i];
						printf("new vcpu time %lu\n", time_diff);
						double vcpu_utilization = ((double)time_diff / (interval * 1000000000ULL)) * 100; // should be percentage
						vcpu_mappings[i].utilization = vcpu_utilization;								  // storing utilization for each domain to take average
						printf("vCPU utilization for domain %s: %.2f%%\n", virDomainGetName(domain), vcpu_utilization);

						// retrieve affinity
						// unsigned char cpumaps;
						virVcpuInfoPtr vcpu_info = (virVcpuInfoPtr)malloc(sizeof(virVcpuInfo));
						if (virDomainGetVcpus(domain, vcpu_info, 1, &cpumaps, maplen) == maplen)
						{
							vcpu_mappings[i].cpumap = cpumaps;
						}
						else
						{
							fprintf(stderr, "Failed to get vCPU info for domain %d\n", i);
						}

						// int pcpu_assigned = current_pcpu; // pin to least busy pcpu

						printf("Maplen= %d\n", maplen);
						// naive RR algorithm
						// int index = cpumaps_size - pcpu_assigned - 1;
						// cpumaps = 1 << pcpu_assigned; // does this allow us to specify the pCPU to pin to?
						// int cpumaps_size = sizeof(cpumaps);
						printf("size of cpumaps: %lu\n", sizeof(unsigned char));

						// current_pcpu = (current_pcpu + 1) % num_pcpu;
						// pin the current vcpu to selected pcpu
						// if (virDomainPinVcpu(domain, 0, &cpumaps, maplen) != 0)
						// {
						// 	fprintf(stderr, "Failed to pin the vCPU: %d of domain %s to pCpu: %d\n", 0, virDomainGetName(domain), pcpu_assigned);
						// }
						// else
						// {
						// 	printf("Successfully pinned the vCPU: %d of domain %s to pCpu: %d\n", 0, virDomainGetName(domain), pcpu_assigned);
						// }
						vcpu_times[i] = new_time;
					}
					else
					{
						fprintf(stderr, "virDomainGetInfo != 0");
					}
				}
			}
			// Go through the vcpu_utilization here to determine uneven load
			// compute pcpu_utilization only when uneven load - or get priority queue
			// for (int i = 0; i < num_domain; i++)
			// {
			// 	int next_index = (i + 1) % num_domain;
			// 	int diff_utilization = abs(vcpu_mappings[i].utilization - vcpu_mappings[next_index].utilization);
			// 	printf("Inside is_uneven for loop, diff_utilization = %.d\n", diff_utilization);
			// 	if (diff_utilization > 25) // or 25?
			// 	{
			// 		printf("IS_UNEVEN is true");
			// 		is_uneven = 1;
			// 		break;
			// 	}
			// }
			// Calculate standard deviation
			double mean;
			double sum = 0.0;
			for (int i = 0; i < num_domain; i++)
			{
				sum += vcpu_mappings[i].utilization;
			}
			mean = sum / num_domain;
			printf("Mean = %f\n", mean);
			sum = 0.0;
			for (int i = 0; i < num_domain; i++)
			{
				sum += pow(vcpu_mappings[i].utilization - mean, 2);
			}
			double standard_deviation = sqrt(sum / num_domain);
			printf("Standard Deviation = %f\n", standard_deviation);
			if (standard_deviation > 12)
			{
				is_uneven = 1;
				printf("uneven workload");
			}
			if (first_iteration)
			{
				// if first iteration- run RR, else run RR only when uneven workload
				first_iteration = 0;
				printf("Using the round robin for the first iteration\n");
				// run the naive round robin
				for (int i = 0; i < num_domain; i++)
				{
					vcpu_mappings[i].assigned_pcpu = current_pcpu; // pin to least busy pcpu
					current_pcpu = (current_pcpu + 1) % num_pcpu;
					// after getting pcpu_assigned, pin them
					cpumaps = 1 << vcpu_mappings[i].assigned_pcpu; // does this allow us to specify the pCPU to pin to?
					//  pin the current vcpu to selected pcpu
					if (virDomainPinVcpu(vcpu_mappings[i].domain_id, 0, &cpumaps, maplen) != 0)
					{
						fprintf(stderr, "Failed to pin the vCPU: %d of domain %s to pCpu: %d\n", 0, virDomainGetName(vcpu_mappings[i].domain_id), vcpu_mappings[i].assigned_pcpu);
					}
					else
					{
						printf("Successfully pinned the vCPU: %d of domain %s to pCpu: %d\n", 0, virDomainGetName(vcpu_mappings[i].domain_id), vcpu_mappings[i].assigned_pcpu);
					}
				}
			}
			else if (is_uneven && !first_iteration)
			{

				// sort the vcpu_mapping array in high to low utilization
				// is_modified = 1;
				printf("Running modified algorithm\n");
				// run the difference algorithm
				for (int i = 0; i < num_domain - 1; i++)
				{
					for (int j = 0; j < num_domain - i - 1; j++)
					{
						if (vcpu_mappings[j].utilization < vcpu_mappings[j + 1].utilization)
						{
							vcpuMapping temp = vcpu_mappings[j];
							vcpu_mappings[j] = vcpu_mappings[j + 1];
							vcpu_mappings[j + 1] = temp;
						}
					}
				}
				printf("Using the round robin\n");
				// run the naive round robin
				for (int i = 0; i < num_domain; i++)
				{
					if (vcpu_mappings[i].assigned_pcpu == current_pcpu)
					{
						break;
					}
					else
					{

						vcpu_mappings[i].assigned_pcpu = current_pcpu; // pin to least busy pcpu
						current_pcpu = (current_pcpu + 1) % num_pcpu;
						// after getting pcpu_assigned, pin them
						cpumaps = 1 << vcpu_mappings[i].assigned_pcpu; // does this allow us to specify the pCPU to pin to?
						//  pin the current vcpu to selected pcpu
						if (virDomainPinVcpu(vcpu_mappings[i].domain_id, 0, &cpumaps, maplen) != 0)
						{
							fprintf(stderr, "Failed to pin the vCPU: %d of domain %s to pCpu: %d\n", 0, virDomainGetName(vcpu_mappings[i].domain_id), vcpu_mappings[i].assigned_pcpu);
						}
						else
						{
							printf("Successfully pinned the vCPU: %d of domain %s to pCpu: %d\n", 0, virDomainGetName(vcpu_mappings[i].domain_id), vcpu_mappings[i].assigned_pcpu);
						}
					}
				}
			}
			// else
			// {
			// first_iteration = 0;
			// printf("Using the round robin\n");
			// // run the naive round robin
			// for (int i = 0; i < num_domain; i++)
			// {
			// 	vcpu_mappings[i].assigned_pcpu = current_pcpu; // pin to least busy pcpu
			// 	current_pcpu = (current_pcpu + 1) % num_pcpu;
			// 	// after getting pcpu_assigned, pin them
			// 	cpumaps = 1 << vcpu_mappings[i].assigned_pcpu; // does this allow us to specify the pCPU to pin to?
			// 	//  pin the current vcpu to selected pcpu
			// 	if (virDomainPinVcpu(vcpu_mappings[i].domain_id, 0, &cpumaps, maplen) != 0)
			// 	{
			// 		fprintf(stderr, "Failed to pin the vCPU: %d of domain %s to pCpu: %d\n", 0, virDomainGetName(vcpu_mappings[i].domain_id), vcpu_mappings[i].assigned_pcpu);
			// 	}
			// 	else
			// 	{
			// 		printf("Successfully pinned the vCPU: %d of domain %s to pCpu: %d\n", 0, virDomainGetName(vcpu_mappings[i].domain_id), vcpu_mappings[i].assigned_pcpu);
			// 	}
			// }
		}
	}
}
