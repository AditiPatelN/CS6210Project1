#include<stdio.h>
#include<stdlib.h>
#include<libvirt/libvirt.h>
#include<math.h>
#include<string.h>
#include<unistd.h>
#include<limits.h>
#include<signal.h>
#define MIN(a,b) ((a)<(b)?a:b)
#define MAX(a,b) ((a)>(b)?a:b)

int is_exit = 0; // DO NOT MODIFY THIS VARIABLE
//Added global variables for persistent code
int num_pcpu = 4; // will dynamically assign in CPUScheduler()
int current_pcpu = 0;//all vCPUs are pinned to pCPU 0 in the beginning

void CPUScheduler(virConnectPtr conn,int interval);

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

	if(argc != 2)
	{
		printf("Incorrect number of arguments\n");
		return 0;
	}

	// Gets the interval passes as a command line argument and sets it as the STATS_PERIOD for collection of balloon memory statistics of the domains
	int interval = atoi(argv[1]);
	
	conn = virConnectOpen("qemu:///system");
	if(conn == NULL)
	{
		fprintf(stderr, "Failed to open connection\n");
		return 1;
	}

	// Get the total number of pCpus in the host
	signal(SIGINT, signal_callback_handler);

	while(!is_exit)
	// Run the CpuScheduler function that checks the CPU Usage and sets the pin at an interval of "interval" seconds
	{
		CPUScheduler(conn, interval);
		sleep(interval);
	}

	// Closing the connection
	virConnectClose(conn);
	//return 0;
}

/* COMPLETE THE IMPLEMENTATION */
void CPUScheduler(virConnectPtr conn, int interval)
{
	//domain pointer variable
	// virDomainPtr domain;
	// int stats;
	//connects to the hypervisor
	conn = virConnectOpen("qemu:///system");
	if (conn == NULL){
		fprintf(stderr, "Failed to connect to the hypervisor");
		//return 1;
	}
	//Declare variables for node and domain
	virNodeInfo node_info;
	virDomainInfo domain_info;
	virDomainPtr domain;
	//get the number of pCPUs-global variable
	if(virNodeGetInfo(conn, &node_info) < 0){
		fprintf(stderr, "Failed to get node information\n");
	} else {
		num_pcpu = node_info.cpus;
		printf("Number of pCPU on the host: %d\n", num_pcpu); 
	}
	//get the number of active domain(VM)
	int num_domain = virConnectNumOfDomains(conn); //number of active domains/VMs
	if(num_domain > 0){
		int * activeDomains = (int *)malloc(sizeof(int) * num_domain);
		int numDomainList = virConnectListDomains(conn, activeDomains, num_domain);
		if(numDomainList < 0) {
			fprintf(stderr, "Failed to get the list of active domains\n");
		} else {
			printf("Active Virtual Machines: \n");
			for(int i = 0; i < numDomainList; i++){
				domain = virDomainLookupByID(conn, activeDomains[i]);
				if(domain == NULL){
					fprintf(stderr, "Failed to find domain by ID");
				} else {
					
					if(virDomainGetInfo(domain, &domain_info) == 0){
						//cpuinfo is same as start time for percentage
						virVcpuInfoPtr cpu_info = (virVcpuInfoPtr)malloc(sizeof(virVcpuInfoPtr) * domain_info.nrVirtCpu);
						int maplen = VIR_CPU_MAPLEN(num_pcpu);
						printf("Maplen= %d\n", maplen);
						unsigned char * cpumaps = (unsigned char *)malloc(maplen);
						int cpumaps_size = sizeof(cpumaps);
						printf("size of cpumaps: %lu\n", sizeof(unsigned char *));
						//number of vcpus per domain
						int num_vcpu = virDomainGetVcpus(domain, cpu_info, domain_info.nrVirtCpu, cpumaps, maplen);
						printf("CPUMAPS before pinning: ");
						for(size_t a = 0; a < cpumaps_size; a++){
							printf("%02x ", cpumaps[a]);
						}
						printf("\n");
						if(num_vcpu < 0){
							fprintf(stderr, "Failed to find any vcpus for this domain. \n");
						} else {
								//naive RR algorithm
								for (int j = 0; j < num_vcpu; j++){
									int pcpu_assigned = current_pcpu;
									int index = cpumaps_size - pcpu_assigned - 1;
									// zero out the cpumap before setting a specific pCPU bit to 1
									for(int d = 0; d < sizeof(cpumaps); d++){
										cpumaps[d] = 0;
									}
									cpumaps[index] = 1; // does this allow us to specify the pCPU to pin to?
									printf("CPUMAPS during pinning: ");
									for(size_t b = 0; b < cpumaps_size; b++){
										printf("%02x ", cpumaps[b]);
									}
									printf("\n");
									current_pcpu = (current_pcpu + 1)% num_pcpu;
									//pin the current vcpu to selected pcpu
									if(virDomainPinVcpu(domain, j, cpumaps + index, maplen)!=0){
										fprintf(stderr, "Failed to pin the vCPU: %d of domain %s to pCpu: %d\n", j, virDomainGetName(domain), pcpu_assigned);
									} else {
										printf("Successfully pinned the vCPU: %d of domain %s to pCpu: %d\n", j, virDomainGetName(domain), pcpu_assigned);
										printf("CPUMAPS after pinning: ");
										for(int m = 0; m < sizeof(cpumaps); m++){
											printf("%02x ", cpumaps[m]);
										}
										printf("\n");
										if(virDomainGetVcpus(domain, cpu_info, domain_info.nrVirtCpu, cpumaps, maplen) < 0){
											fprintf(stderr, "Failed to get affnity\n");
										} else {
											printf("Afinity for CPU: ");
											for(int c = 0; c < maplen; c++){
												printf("%02x ", cpumaps[c]);
											}
											printf("\n");
										}
										//printf("CPUMAPS after sucessfully pinning: %s\n", cpumaps);

									}
								}
						}
					} else{
						fprintf(stderr, "virDomainGetInfo != 0");
					}
				}
			}
		}

	}
	printf("Time Interval: %d\n", interval);
	// if(num_vcpu > 0){
	// 	int * activeDomains = (int *)malloc(sizeof(int) * num_vcpu);
	// 	//list of all the active domains
	// 	int numDomainList = virConnectListDomains(conn, activeDomains, num_vcpu);
	// 	if(numDomainList < 0){
	// 		fprintf(stderr, "Failed to retrieve the list of active domains");
	// 	} else {
	// 		printf("Active virtual machines: \n");
	// 		for (int i = 0; i < numDomainList; i++){
	// 			printf("Domain ID: %d\n", activeDomains[i]);
	// 			domain = virDomainLookupByID(conn, activeDomains[i]);
	// 			if(domain == NULL){
	// 				fprintf(stderr, "Failed to find domain");
	// 				virConnectClose(conn);
	// 			}
	// 			//get max number of VCPU
	// 			int maxVCPUs = virDomainGetMaxVcpus(domain);
	// 			if(maxVCPUs > 0 ){
	// 				virVcpuInfoPtr vcpuInfoStart = (virVcpuInfoPtr)malloc(sizeof(virVcpuInfo)*maxVCPUs);
					
	// 				int maplen = maxVCPUs;
	// 				unsigned char * cpumaps = (unsigned char *)malloc(sizeof(char)* maxVCPUs); //cannot be null?
					
	// 				int numVcpus = virDomainGetVcpus(domain, vcpuInfoStart, maxVCPUs, cpumaps, maplen);
	// 				if(numVcpus < 0){
	// 					fprintf(stderr, "Failed to retrieve vCPU information during start");
	// 				}
	// 				sleep(interval);
	// 				virVcpuInfoPtr vcpuInfoEnd = (virVcpuInfoPtr)malloc(sizeof(virVcpuInfo)*maxVCPUs);
	// 				//printf("Value of starting maplen(before call) %d\n", maplen);
	// 				numVcpus = virDomainGetVcpus(domain, vcpuInfoEnd, maxVCPUs, cpumaps, maplen);
	// 				//printf("Value of starting maplen(after call) %d\n", maplen);
	// 				if(numVcpus < 0){
	// 					fprintf(stderr, "Failed to retrieve vCPU information during end");
	// 				}
	// 				printf("vCPU Usage for domain '%s':\n", virDomainGetName(domain));
	// 				for(int j = 0; j < numVcpus; j++){
	// 					unsigned long long cpuTimeStart = vcpuInfoStart[j].cpuTime;
	// 					unsigned long long cpuTimeEnd = vcpuInfoEnd[j].cpuTime;
	// 					unsigned long long cpuTimeDiff = cpuTimeEnd - cpuTimeStart;
	// 					double vcpuUsage = ((double)cpuTimeDiff/(interval * 1000000000ULL))*100.0;
	// 					printf("vCPU %d: Usage=%.2f%%\n ", vcpuInfoStart[j].number, vcpuUsage);
						
	// 					printf("Affinity (pCPUs): ");
	// 					for(int k= 0; k < maplen; k++){
	// 						printf("%04X", cpumaps[k]);
	// 					}
	// 					printf("\n");
						
						

	// 				}
	// 			}
	// 			//get vCPU statistics for a specific domain id
				
				
				
	// 			}
	// 		}
	// 	}
		
	
	// //Get the host CPU info
	// virNodeInfo nodeInfo;
	// stats = virNodeGetInfo(conn, &nodeInfo);
	// if(stats < 0){
	// 	fprintf(stderr, "Failed to get host CPU stats");
	// }
	// else {
	// 	printf("\nHost CPU information:\n");
	// 	printf("Model %s\n", nodeInfo.model);
	// 	printf("Memory Size: %lu MB\n", nodeInfo.memory);
	// 	printf("Number of CPUs: %d\n", nodeInfo.cpus);
	// }
	// //closes the connection
	// virConnectClose(conn);
	//return 0;

}




