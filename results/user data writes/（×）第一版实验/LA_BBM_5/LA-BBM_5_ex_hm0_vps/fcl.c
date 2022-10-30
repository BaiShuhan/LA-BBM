/*****************************************************************************************************************************
This is a project on 3Dsim, based on ssdsim under the framework of the completion of structures, the main function:
1.Support for 3D commands, for example:mutli plane\interleave\copyback\program suspend/Resume..etc
2.Multi - level parallel simulation
3.Clear hierarchical interface
4.4-layer structure

FileName�� fcl.c
Author: Zuo Lu 		Version: 2.0	Date:2017/02/07
Description:
fcl layer: remove other high-level commands, leaving only mutli plane;

History:
<contributor>		<time>			<version>       <desc>													<e-mail>
Zuo Lu				2017/04/06	      1.0		    Creat 3Dsim											lzuo@hust.edu.cn
Zuo Lu				2017/05/12		  1.1			Support advanced commands:mutli plane					lzuo@hust.edu.cn
Zuo Lu				2017/06/12		  1.2			Support advanced commands:half page read				lzuo@hust.edu.cn
Zuo Lu				2017/06/16		  1.3			Support advanced commands:one shot program				lzuo@hust.edu.cn
Zuo Lu				2017/06/22		  1.4			Support advanced commands:one shot read					lzuo@hust.edu.cn
Zuo Lu				2017/07/07		  1.5			Support advanced commands:erase suspend/resume			lzuo@hust.edu.cn
Zuo Lu				2017/07/24		  1.6			Support static allocation strategy						lzuo@hust.edu.cn
Zuo Lu				2017/07/27		  1.7			Support hybrid allocation strategy						lzuo@hust.edu.cn
Zuo Lu				2017/08/17		  1.8			Support dynamic stripe allocation strategy				lzuo@hust.edu.cn
Zuo Lu				2017/10/11		  1.9			Support dynamic OSPA allocation strategy				lzuo@hust.edu.cn
Jin Li				2018/02/02		  1.91			Add the allocation_method								li1109@hust.edu.cn
Ke wang/Ke Peng		2018/02/05		  1.92			Add the warmflash opsration								296574397@qq.com/2392548402@qq.com
Hao Lv				2018/02/06		  1.93			Solve gc operation bug 									511711381@qq.com
Zuo Lu				2018/02/07        2.0			The release version 									lzuo@hust.edu.cn
***********************************************************************************************************************************************/

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include "flash.h"
#include "ssd.h"
#include "initialize.h"
#include "buffer.h"
#include "interface.h"
#include "ftl.h"
#include "fcl.h"

extern int secno_num_per_page, secno_num_sub_page;

//zy add
#define PAGES_PER_LAYER 12

//bsh
#define LAYERS_PER_BLOCK 48
#define X_BBM 0.5

/**************************************************************************
*This function is also only deal with read requests, processing chip current state is CHIP_WAIT,
*Or the next state is CHIP_DATA_TRANSFER and the next state of the expected time is less than the current time of the chip
***************************************************************************/
Status services_2_r_data_trans(struct ssd_info * ssd, unsigned int channel)
{
	unsigned int chip = 0;
	unsigned int sub_r_count, i = 0;
	unsigned int aim_die;

	struct sub_request ** sub_r_request = NULL;
	sub_r_request = (struct sub_request **)malloc((ssd->parameter->plane_die * PAGE_INDEX) * sizeof(struct sub_request *));
	alloc_assert(sub_r_request, "sub_r_request");
	for (i = 0; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
		sub_r_request[i] = NULL;

	for (chip = 0; chip < ssd->channel_head[channel].chip; chip++)
	{
		if ((ssd->channel_head[channel].chip_head[chip].current_state == CHIP_DATA_TRANSFER) ||
			((ssd->channel_head[channel].chip_head[chip].next_state == CHIP_DATA_TRANSFER) &&
			(ssd->channel_head[channel].chip_head[chip].next_state_predict_time <= ssd->current_time)))
		{
			for (aim_die = 0; aim_die < ssd->parameter->die_chip; aim_die++)
			{
				//�߼������֧�֣���ʹ����ͨ��
				sub_r_count = find_read_sub_request(ssd, channel, chip, aim_die, sub_r_request, SR_R_DATA_TRANSFER, NORMAL);
				if (sub_r_count == 1)
				{
					go_one_step(ssd, sub_r_request, sub_r_count, SR_R_DATA_TRANSFER, NORMAL);
					ssd->channel_head[channel].channel_busy_flag = 1;
					break;
				}
				else 
					ssd->channel_head[channel].channel_busy_flag = 0;
			}
		}

		if (ssd->channel_head[channel].channel_busy_flag == 1)
			break;
	}
	for (i = 0; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
		sub_r_request[i] = NULL;
	free(sub_r_request);
	sub_r_request = NULL;
	return SUCCESS;
}

unsigned int *find_pages_in_same_layer(unsigned int page)
{
	static unsigned int fail_list[PAGES_PER_LAYER];
	unsigned int layer;
	int i;

	layer = page / PAGES_PER_LAYER;
	for (i = 0; i < PAGES_PER_LAYER; i++)
	{
		fail_list[i] = layer * PAGES_PER_LAYER + i;
	}

	return fail_list;
}


Status services_2_r_read(struct ssd_info * ssd)
{
	unsigned int i, j, subs_count = 0, aim_die;
	unsigned int mask;

	//zy add start
	unsigned int lpn, ppn, erase_count;
	struct local * loc = NULL;
	struct local * location = NULL;
	unsigned int *fail_list = NULL;
	unsigned int page_idx;
	unsigned int sub_req_state = 0, sub_req_lpn = 0;
	int sub_req_size = 0;
	struct sub_request *sub_req = NULL;
	unsigned int transfer_size = 0;
	int flag_direct_erase = 1, flag_gc = 1;

	//zy end

	//bsh
	unsigned int tmp = 0;

	struct sub_request ** subs = NULL;
	subs = (struct sub_request **)malloc((ssd->parameter->plane_die*PAGE_INDEX) * sizeof(struct sub_request *));
	alloc_assert(subs, "subs");
	for (i = 0; i < (ssd->parameter->plane_die*PAGE_INDEX); i++)
		subs[i] = NULL;

	mask = ~(0xffffffff << (ssd->parameter->subpage_page));
	for (i = 0; i < ssd->parameter->channel_number; i++)
	{
		for (j = 0; j < ssd->parameter->chip_channel[i]; j++)
		{
			if ((ssd->channel_head[i].chip_head[j].current_state == CHIP_READ_BUSY) ||
				((ssd->channel_head[i].chip_head[j].next_state == CHIP_READ_BUSY) &&
				(ssd->channel_head[i].chip_head[j].next_state_predict_time <= ssd->current_time)))
			{
				for (aim_die = 0; aim_die < ssd->parameter->die_chip; aim_die++)
				{
					

					//�������и߼������֧�֣���������ͨ�Ķ���half page read
					subs_count = find_read_sub_request(ssd, i, j, aim_die, subs, SR_R_READ, NORMAL);
					if (subs_count == 1)
					{
						
						go_one_step(ssd, subs, subs_count, SR_R_READ, NORMAL);
						if (ssd->warm_flash_cmplt == 0) continue;
						//zy add start
						lpn = subs[0]->lpn;
						ppn = ssd->dram->map->map_entry[lpn].pn;
						loc = find_location(ssd, ppn);
						erase_count = ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].erase_count;
#if 0 // tradition bbm (traditional -- 1;TA-BBM -- 0)
						if (erase_count > lifetime_zy[loc->page])
						{
							//healthy statue start
							printf("read time = %.1lf, read_cnt = %.1lf, read_avg = %.1lf\n", (double)ssd->read_avg, (double)ssd->read_request_count, (double)ssd->read_avg / ssd->read_request_count);
							printf("write time = %.1lf, write_cnt = %.1lf, write_avg = %.1lf\n", (double)ssd->write_avg, (double)ssd->write_request_count, (double)ssd->write_avg / ssd->write_request_count);
							printf("Test complete\n");
							exit(0);
							//end (若不输出健康盘数据则应注释此段)
							printf("block %d is failed\n", loc->block);					
							for (j = 0; j < ssd->parameter->page_block; j++)
							{
								page_idx = j;
								if (ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].page_head[page_idx].fail == 0)  // cur page is healthy, which need to be set fail
								{
									if (ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].page_head[page_idx].valid_state > 0) // cur page is valid, and valid data need to be move
									{
										location = (struct local *)malloc(sizeof(struct local));
										alloc_assert(location, "location");
										memset(location, 0, sizeof(struct local));
										location->channel = loc->channel;
										location->chip = loc->chip;
										location->die = loc->die;
										location->plane = loc->plane;
										location->block = loc->block;
										location->page = page_idx;

										move_page(ssd, location, 0, &transfer_size);

										free(location);
										location = NULL;

										ssd->channel_head[loc->channel].next_state_predict_time = ssd->current_time +
											1 * (7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tR + 7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tPROG) +
											transfer_size * SECTOR*(ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tRC);

										if (ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].free_page <= (ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_soft_threshold))
										{
											gc(ssd, 0, 1);
										}

									}
									ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].page_head[page_idx].fail = 1;
									ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].fail_pages++;
									ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].fail_pages++;
								}
							}
							free(loc);
							loc = NULL;
						}
#else
						if (erase_count > lifetime_zy[loc->page])
						{
							//tmp++;   //bsh
							printf("page ppn %d is failed, page_idx = %d\n", ppn, loc->page);
							fail_list = find_pages_in_same_layer(loc->page);
							for (j = 0; j < PAGES_PER_LAYER; j++)
							{
								page_idx = fail_list[j];
								printf("%d ", page_idx);
								if (ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].page_head[page_idx].fail == 0)  // cur page is healthy, which need to be set fail
								{
									if (ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].page_head[page_idx].valid_state > 0) // cur page is valid, and valid data need to be move
									{
										location = (struct local *)malloc(sizeof(struct local));
										alloc_assert(location, "location");
										memset(location, 0, sizeof(struct local));
										location->channel = loc->channel;
										location->chip = loc->chip;
										location->die = loc->die;
										location->plane = loc->plane;
										location->block = loc->block;
										location->page = page_idx;

										move_page(ssd, location, 0, &transfer_size);

										free(location);
										location = NULL;

										ssd->channel_head[loc->channel].next_state_predict_time = ssd->current_time +
											1 * (7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tR + 7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tPROG) +
											transfer_size*SECTOR*(ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tRC);

										if (ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].free_page <= (ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_soft_threshold))
										{
											gc(ssd, 0, 1);					
										}

									}
									ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].page_head[page_idx].fail = 1;
									ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].fail_pages++;
									ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].fail_pages++;
								}
							}
							printf("\n");
							tmp = calc_total_fail_layer_cur_block(ssd, loc->channel, loc->chip, loc->die, loc->plane, loc->block);
							//bsh start
							if (tmp > (LAYERS_PER_BLOCK * X_BBM))
							{
								printf("BLOCK %d is failed\n", loc->block);
								for (j = 0; j < (PAGES_PER_LAYER * LAYERS_PER_BLOCK); j++)
								{
									if (ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].page_head[j].fail == 0)
									{
										// check whether to move valid pages or not
										if (ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].page_head[j].valid_state > 0)
										{
											location = (struct local *)malloc(sizeof(struct local));
											alloc_assert(location, "location");
											memset(location, 0, sizeof(struct local));
											location->channel = loc->channel;
											location->chip = loc->chip;
											location->die = loc->die;
											location->plane = loc->plane;
											location->block = loc->block;
											location->page = j;

											move_page(ssd, location, 0, &transfer_size);

											free(location);
											location = NULL;

											ssd->channel_head[loc->channel].next_state_predict_time = ssd->current_time +
												1 * (7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tR + 7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tPROG) +
												transfer_size*SECTOR*(ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tRC);

											if (ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].free_page <= (ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_soft_threshold))
											{
												gc(ssd, 0, 1);
											}

										}
										ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].page_head[j].fail = 1;
										
										ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].fail_pages++;
										ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].fail_pages++;
									}
								}
							}
							//bsh end
						}
						//zy end

						free(loc);
						loc = NULL;
						break;
#endif
					}
				}
			}
		}
	}

	for (i = 0; i < (ssd->parameter->plane_die*PAGE_INDEX); i++)
		subs[i] = NULL;
	free(subs);
	subs = NULL;

	return SUCCESS;
}

/**************************************************************************************
* Summary:		calculate total number of layers which are set fail in current block
* Parameters:	ssd, channel, chip, die, plane, block
* Return:		total number of layers failed
**************************************************************************************/
int calc_total_fail_layer_cur_block(struct ssd_info *ssd, int channel, int chip, int die, int plane, int block) {
	int layer = 0;
	int ret = 0;
	int page_offset = 0;
	int i = 0;
	int is_all_fail = 1;
	struct blk_info tblk = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block];
	for (layer = 0; layer < LAYERS_PER_BLOCK; layer++) {
		is_all_fail = 1;
		for (i = 0; i < PAGES_PER_LAYER; i++) {
			if (tblk.page_head[layer*PAGES_PER_LAYER + i].fail == 0) {
				is_all_fail = 0;
				break;
			}	
		}
		if (is_all_fail == 1) ret++;
	}
	return ret;
}
/**************************************************************************************
*Function function is given in the channel, chip, die above looking for reading requests
*The request for this child ppn corresponds to the ppn of the corresponding plane's register
*****************************************************************************************/
unsigned int find_read_sub_request(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, struct sub_request ** subs, unsigned int state, unsigned int command)
{
	struct sub_request * sub = NULL;
	unsigned int add_reg, i, j = 0;

	for (i = 0; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
		subs[i] = NULL;
	
	j = 0;
	sub = ssd->channel_head[channel].subs_r_head;
	while (sub != NULL)
	{
		if (sub->location->chip == chip && sub->location->die == die)
		{
			if (sub->next_state == state && sub->next_state_predict_time <= ssd->current_time)
			{
				if (command == NORMAL)
				{
					if ((sub->oneshot_mutliplane_flag != 1) && (sub->oneshot_flag != 1) && (sub->mutliplane_flag != 1))
					{
						subs[j] = sub;
						j++;
						
					}
					if (j == 1)
						break; 
				}
				else {
					printf("Error command!\n");
					while(1){}
				}
			}
		}
		sub = sub->next_node;
	}
 

	if (j > (ssd->parameter->plane_die * PAGE_INDEX))
	{
		printf("error,beyong plane_die* PAGE_INDEX\n");
		getchar();
	}

	return j;
}

/*********************************************************************************************
* function that specifically serves a read request
*1��Only when the current state of the sub request is SR_R_C_A_TRANSFER
*2��The current state of the read request is SR_COMPLETE or the next state is SR_COMPLETE and 
*the next state arrives less than the current time
**********************************************************************************************/
Status services_2_r_complete(struct ssd_info * ssd)
{
	unsigned int i = 0;
	struct sub_request * sub = NULL, *p = NULL;
	
	for (i = 0; i<ssd->parameter->channel_number; i++)                                       /*This loop does not require the channel time, when the read request is completed, it will be removed from the channel queue*/
	{
		sub = ssd->channel_head[i].subs_r_head;
		p = NULL;
		while (sub != NULL)
		{
			if ((sub->current_state == SR_COMPLETE) || ((sub->next_state == SR_COMPLETE) && (sub->next_state_predict_time <= ssd->current_time)))
			{
				if (sub != ssd->channel_head[i].subs_r_head)                         
				{
					if (sub == ssd->channel_head[i].subs_r_tail)
					{
						ssd->channel_head[i].subs_r_tail = p;
						p->next_node = NULL;
					}
					else
					{
						p->next_node = sub->next_node;
						sub = p->next_node;
					}
				}
				else
				{
					if (ssd->channel_head[i].subs_r_head != ssd->channel_head[i].subs_r_tail)
					{
						ssd->channel_head[i].subs_r_head = sub->next_node;
						sub = sub->next_node;
						p = NULL;
					}
					else
					{
						ssd->channel_head[i].subs_r_head = NULL;
						ssd->channel_head[i].subs_r_tail = NULL;
						break;
					}
				}
			}
			else
			{
				p = sub;
				sub = sub->next_node;
			}
			
		}
	}

	return SUCCESS;
}


/*****************************************************************************************
*This function is also a service that only reads the child request, and is in a wait state
******************************************************************************************/
Status services_2_r_wait(struct ssd_info * ssd, unsigned int channel)
{
	struct sub_request ** sub_place = NULL;
	unsigned int sub_r_req_count, i ,chip;

	sub_place = (struct sub_request **)malloc(ssd->parameter->plane_die * PAGE_INDEX * sizeof(struct sub_request *));
	alloc_assert(sub_place, "sub_place");
	sub_r_req_count = 0;
	for (chip = 0; chip < ssd->parameter->chip_channel[channel]; chip++)
	{	
		//�����ܣ���ʾ���еĸ߼���������У���ȥִ����ͨ�Ķ�����,����ͨ�Ķ�����δ�ҵ����򷵻أ���chip����Ч������ִ��
		sub_r_req_count = find_r_wait_sub_request(ssd, channel, chip, sub_place, NORMAL);
		if (sub_r_req_count == 1)
		{
			for (i = sub_r_req_count; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
				sub_place[i] = NULL;

			go_one_step(ssd, sub_place, sub_r_req_count, SR_R_C_A_TRANSFER, NORMAL);
			ssd->channel_head[channel].channel_busy_flag = 1;
		}
		else if (sub_r_req_count == 0)
		{
			ssd->channel_head[channel].channel_busy_flag = 0;
		}
	}

	for (i = 0; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
		sub_place[i] = NULL;
	free(sub_place);
	sub_place = NULL;

	return SUCCESS;
}



//Ѱ����one shot read����������
unsigned int find_r_wait_sub_request(struct ssd_info * ssd, unsigned int channel, unsigned int chip, struct sub_request ** sub_place, unsigned int command)
{
	unsigned int i,j,flag;
	unsigned int aim_die, aim_plane, aim_block, aim_group;
	unsigned int sub_count, plane_flag;
	struct sub_request * sub_plane_request = NULL;

	//��ʼ��
	for (i = 0; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
		sub_place[i] = NULL;

	flag = 0;
	i = 0;
	sub_count = 0;
	sub_plane_request = ssd->channel_head[channel].subs_r_head;
	while (sub_plane_request != NULL)
	{
		if (sub_plane_request->current_state == SR_WAIT && sub_plane_request->location->chip == chip)
		{
			/*************************************************************************************/

			if (command == NORMAL)
			{
				if (((ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].current_state == CHIP_IDLE) ||
					((ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].next_state == CHIP_IDLE) &&
					(ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].next_state_predict_time <= ssd->current_time))))
				{
					sub_place[0] = sub_plane_request;
					sub_count = 1;
					break;
				}
			}
			else {
				printf("Error, command\n");
				while(1){}
			}
		}
		sub_plane_request = sub_plane_request->next_node;
	}

	if (sub_count > ssd->parameter->plane_die * PAGE_INDEX)
	{
		printf("error,beyong plane_die * PAGE_INDEX\n");
		getchar();
	}
	return sub_count;
}




/***********************************************************************************************************
*1.The state transition of the child request, and the calculation of the time, are handled by this function
*2.The state of the execution of the normal command, and the calculation of the time, are handled by this function
****************************************************************************************************************/
Status go_one_step(struct ssd_info * ssd, struct sub_request ** subs, unsigned int subs_count, unsigned int aim_state, unsigned int command)
{
	unsigned int i = 0, j = 0, k = 0, m = 0;
	long long read_time = 0;
	long long time = 0;
	unsigned int plane_number = 0;

	struct sub_request * sub = NULL;
	struct local * location = NULL;

	for (i = 0; i < subs_count; i++)
	{
		if (subs[i] == NULL)
		{
			printf("ERROR! no subs state jump\n");
			getchar();
			return ERROR;
		}
	}

	/***************************************************************************************************
	*When dealing with ordinary commands, the target state of the read request is divided into the following
	*cases: SR_R_READ, SR_R_C_A_TRANSFER, SR_R_DATA_TRANSFER
	*
	*The target status of the write request is only SR_W_TRANSFER
	****************************************************************************************************/
	if (command == NORMAL)
	{
		sub = subs[0];
		location = subs[0]->location;

		switch (aim_state)
		{
		case SR_R_C_A_TRANSFER:
		{
			/*******************************************************************************************************
			*When the target state is the command address transfer, the next state of sub is SR_R_READ
			*This state and channel, chip, so to modify the channel, chip status were CHANNEL_C_A_TRANSFER, CHIP_C_A_TRANSFER
			*The next status is CHANNEL_IDLE, CHIP_READ_BUSY
			*******************************************************************************************************/
			sub->current_time = ssd->current_time;
			sub->current_state = SR_R_C_A_TRANSFER;
			sub->next_state = SR_R_READ;
			sub->next_state_predict_time = ssd->current_time + 7 * ssd->parameter->time_characteristics.tWC;
			sub->begin_time = ssd->current_time;

			ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].add_reg_ppn = sub->ppn;

			ssd->channel_head[location->channel].current_state = CHANNEL_C_A_TRANSFER;
			ssd->channel_head[location->channel].current_time = ssd->current_time;
			ssd->channel_head[location->channel].next_state = CHANNEL_IDLE;
			ssd->channel_head[location->channel].next_state_predict_time = ssd->current_time + 7 * ssd->parameter->time_characteristics.tWC;

			ssd->channel_head[location->channel].chip_head[location->chip].current_state = CHIP_C_A_TRANSFER;
			ssd->channel_head[location->channel].chip_head[location->chip].current_time = ssd->current_time;
			ssd->channel_head[location->channel].chip_head[location->chip].next_state = CHIP_READ_BUSY;
			ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time = ssd->current_time + 7 * ssd->parameter->time_characteristics.tWC;

			break;

		}
		case SR_R_READ:
		{
			/*****************************************************************************************************
			*This target state is flash in the state of reading data, sub the next state should be transmitted data SR_R_DATA_TRANSFER.
			*Then has nothing to do with the channel, only with the chip so to modify the chip status CHIP_READ_BUSY, the next state is CHIP_DATA_TRANSFER
			******************************************************************************************************/
			sub->current_time = ssd->current_time;
			sub->current_state = SR_R_READ;
			sub->next_state = SR_R_DATA_TRANSFER;
			sub->next_state_predict_time = ssd->current_time + ssd->parameter->time_characteristics.tR;

			ssd->read_count++;
			ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_read_count++;

			ssd->channel_head[location->channel].chip_head[location->chip].current_state = CHIP_READ_BUSY;
			ssd->channel_head[location->channel].chip_head[location->chip].current_time = ssd->current_time;
			ssd->channel_head[location->channel].chip_head[location->chip].next_state = CHIP_DATA_TRANSFER;
			ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time = ssd->current_time + ssd->parameter->time_characteristics.tR;
			
		
			break;
		}

		case SR_R_DATA_TRANSFER:
		{
			/**************************************************************************************************************
			*When the target state is data transfer, the next state of sub is the completion state. SR_COMPLETE
			*The state of the deal with the channel, chip, so channel, chip current state into CHANNEL_DATA_TRANSFER, CHIP_DATA_TRANSFER
			*The next state is CHANNEL_IDLE, CHIP_IDLE.
			***************************************************************************************************************/
			sub->current_time = ssd->current_time;
			sub->current_state = SR_R_DATA_TRANSFER;
			sub->next_state = SR_COMPLETE;
			sub->next_state_predict_time = ssd->current_time + (sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tRC;
			sub->complete_time = sub->next_state_predict_time;

			ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].add_reg_ppn = -1;

			if (sub->update_read_flag == 1)
				sub->update_read_flag = 0;

			ssd->channel_head[location->channel].current_state = CHANNEL_DATA_TRANSFER;
			ssd->channel_head[location->channel].current_time = ssd->current_time;
			ssd->channel_head[location->channel].next_state = CHANNEL_IDLE;
			ssd->channel_head[location->channel].next_state_predict_time = sub->next_state_predict_time;

			ssd->channel_head[location->channel].chip_head[location->chip].current_state = CHIP_DATA_TRANSFER;
			ssd->channel_head[location->channel].chip_head[location->chip].current_time = ssd->current_time;
			ssd->channel_head[location->channel].chip_head[location->chip].next_state = CHIP_IDLE;
			ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time = sub->next_state_predict_time;

			
			break;
		}
		case SR_W_TRANSFER:
		{
			/******************************************************************************************************
			*This is the time to deal with write requests, state changes, and time calculations
			*Write requests are from the top of the plane to transfer data, so that you can put a few states as a state
			*to deal with, as SR_W_TRANSFER this state to deal with, sub next state is complete state
			*At this time channel, chip current state into CHANNEL_TRANSFER, CHIP_WRITE_BUSY
			*The next state changes to CHANNEL_IDLE, CHIP_IDLE
			*******************************************************************************************************/
			sub->current_time = ssd->current_time;
			sub->current_state = SR_W_TRANSFER;
			sub->next_state = SR_COMPLETE;
			sub->next_state_predict_time = ssd->current_time + 7 * ssd->parameter->time_characteristics.tWC + 
			(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tPROG;
			
			sub->complete_time = sub->next_state_predict_time;
			time = sub->complete_time;

			ssd->channel_head[location->channel].current_state = CHANNEL_TRANSFER;
			ssd->channel_head[location->channel].current_time = ssd->current_time;
			ssd->channel_head[location->channel].next_state = CHANNEL_IDLE;
			ssd->channel_head[location->channel].next_state_predict_time = ssd->current_time + 7 * ssd->parameter->time_characteristics.tWC + (sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;

			ssd->channel_head[location->channel].chip_head[location->chip].current_state = CHIP_WRITE_BUSY;
			ssd->channel_head[location->channel].chip_head[location->chip].current_time = ssd->current_time;
			ssd->channel_head[location->channel].chip_head[location->chip].next_state = CHIP_IDLE;
			ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time = time;

			break;
		}
		default:  return ERROR;
		}
	}
	
	else
	{
		printf("\nERROR: Unexpected command !\n");
		return ERROR;
	}

	return SUCCESS;
}

/****************************************
Write the request function of the request
*****************************************/
Status services_2_write(struct ssd_info * ssd, unsigned int channel)
{
	int j = 0,i = 0;
	unsigned int chip_token = 0;
	struct sub_request *sub = NULL;

	/************************************************************************************************************************
	*Because it is dynamic allocation, all write requests hanging in ssd-> subs_w_head, that is, do not know which allocation before writing on the channel
	*************************************************************************************************************************/
	if (ssd->subs_w_head != NULL || ssd->channel_head[channel].subs_w_head != NULL)
	{
		//�ж�tlcģʽ�£�һ��process���������ϵ�������û��С��3�����󣬲���ִ��one��shot
		if (ssd->channel_head[channel].subs_w_head != NULL)
		{
			sub = ssd->channel_head[channel].subs_w_head;
			while (sub != NULL)
			{
				i++;	
				sub = sub->next_node;
			}
			if (ssd->parameter->flash_mode == TLC_MODE)
			{
				if (i < PAGE_INDEX)
					printf("\n less than 3 subs \n");
			}
		}
		
		//����flagȥ�жϵ��ױ���״̬ת��ִ�����ַ�ʽ
		if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION)
		{
			for (j = 0; j<ssd->channel_head[channel].chip; j++)							  //Traverse all the chips
			{
				if (ssd->subs_w_head == NULL)											  //The loop is stopped when the request is processed
				{
					break;
				}

				chip_token = ssd->channel_head[channel].token;                           
				if (ssd->channel_head[channel].channel_busy_flag == 0)
				{
					if ((ssd->channel_head[channel].chip_head[chip_token].current_state == CHIP_IDLE) || ((ssd->channel_head[channel].chip_head[chip_token].next_state == CHIP_IDLE) && (ssd->channel_head[channel].chip_head[chip_token].next_state_predict_time <= ssd->current_time)))
					{
						if (dynamic_advanced_process(ssd, channel, chip_token) == NULL)
							ssd->channel_head[channel].channel_busy_flag = 0;
						else   
							ssd->channel_head[channel].channel_busy_flag = 1;
					}
					ssd->channel_head[channel].token = (ssd->channel_head[channel].token + 1) % ssd->parameter->chip_channel[channel];  //The current chip is busy and jumps to the next chip execution
				}
			}
		}
		else {
			printf("Error, alloc sch");
		}
		
	}
	else
	{
		ssd->channel_head[channel].channel_busy_flag = 0;
	}
	return SUCCESS;
}

/****************************************************************************************************************************
*When ssd supports advanced commands, the function of this function is to deal with high-level command write request
*According to the number of requests, decide which type of advanced command to choose (this function only deal with write requests, 
*read requests have been assigned to each channel, so the implementation of the election between the corresponding command)
*****************************************************************************************************************************/
struct ssd_info *dynamic_advanced_process(struct ssd_info *ssd, unsigned int channel, unsigned int chip)
{
	unsigned int subs_count = 0;
	unsigned int update_count = 0;    /*record which plane has sub request in static allocation*/
	struct sub_request *sub = NULL, *p = NULL;
	struct sub_request ** subs = NULL;
	unsigned int max_sub_num = 0, aim_subs_count;
	unsigned int die_token = 0, plane_token = 0;

	unsigned int mask = 0x00000001;
	unsigned int i = 0, j = 0, k = 0;
	unsigned int aim_die;

	max_sub_num = (ssd->parameter->die_chip)*(ssd->parameter->plane_die)*PAGE_INDEX;
	subs = (struct sub_request **)malloc(max_sub_num*sizeof(struct sub_request *));
	alloc_assert(subs, "sub_request");
	update_count = 0;
	for (i = 0; i < max_sub_num; i++)
		subs[i] = NULL;  
	

	//�����Ƕ�̬���仹�Ǿ�̬���䣬ѡ����ͬ�Ĺ��ص�
	if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION)
		sub = ssd->subs_w_head;

	//1.�����������ϵ�ǰ���е�����
	subs_count = 0;
	while ((sub != NULL) && (subs_count < max_sub_num))
	{
		if (sub->current_state == SR_WAIT)
		{
			if ((sub->update == NULL) || ((sub->update != NULL) && ((sub->update->current_state == SR_COMPLETE) || ((sub->update->next_state == SR_COMPLETE) && (sub->update->next_state_predict_time <= ssd->current_time)))))    //û����Ҫ��ǰ������ҳ
			{
				
				if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION)						//��̬���䣬����֪���߼�ҳ�ֵ�����һ���̶���ҳ�������ǲ�ȷ����
				{
					//if (sub->location->chip == chip)
					//{
					//	subs[subs_count] = sub;
					//	subs_count++;
					//}
					subs[subs_count] = sub;
					subs_count++;
				}
				else {
					printf("Error, sch3");
					while(1){}
				}
				
			}
		}
		if (sub->update_read_flag == 1)
			update_count++;

		sub = sub->next_node;
	}

	//2.�Ҳ������е��������򷵻�null,��ʾ��ǰû������ִ��
	if (subs_count == 0)
	{
		for (i = 0; i < max_sub_num; i++)
			subs[i] = NULL;
		free(subs);
		subs = NULL;
		return NULL;
	}

	//3.�������������������������������������еĹ���	
	if (update_count > ssd->update_sub_request)
		ssd->update_sub_request = update_count;

	//�������¶������ȣ���trace�ļ���ȡ����
	if (update_count > ssd->parameter->update_reqeust_max)
	{
		printf("update sub request is full!\n");
		ssd->buffer_full_flag = 1;  //blcok the buffer
	}
	else
		ssd->buffer_full_flag = 0;
		
	//4.���ݲ�ͬģʽ��ѡ����Ӧ�ĸ߼���������
	if (ssd->parameter->flash_mode == SLC_MODE)
	{
		for (i = 1; i<subs_count; i++)
		{
			subs[i] = NULL;
		}
		subs_count = 1;
		get_ppn_for_normal_command(ssd, channel, chip, subs[0]);
		//printf("lz:normal program\n");
		//getchar();
		
	}
	else {
		printf("Error, slc mode1\n");
		while(1){}
	}
	

	//5.�������ɣ��ͷ����������ռ䣬������ssd�ṹ�壬��ʾ������ִ��
	for (i = 0; i < max_sub_num; i++)
	{
		subs[i] = NULL;
	}
	free(subs);
	subs = NULL;
	return ssd;
}


/****************************************************************************
*this function is to calculate the processing time and the state transition 
*of the processing when processing the write request for the advanced command
*****************************************************************************/
struct ssd_info *compute_serve_time(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, struct sub_request **subs, unsigned int subs_count, unsigned int command)
{
	unsigned int i = 0;
	struct sub_request * last_sub = NULL;
	int prog_time = 0;

	//����д�����У�д��ַ������д���ݵĴ����Ǵ��еģ�����һ������
	for (i = 0; i < subs_count; i++)
	{
		subs[i]->current_state = SR_W_TRANSFER;
		if (last_sub == NULL)
		{
			subs[i]->current_time = ssd->current_time;
		}
		else
		{
			subs[i]->current_time = last_sub->complete_time + ssd->parameter->time_characteristics.tDBSY;
		}

		subs[i]->update_read_flag = 0;
		subs[i]->next_state = SR_COMPLETE;
		subs[i]->next_state_predict_time = subs[i]->current_time + 7 * ssd->parameter->time_characteristics.tWC + (subs[i]->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
		subs[i]->complete_time = subs[i]->next_state_predict_time;
		last_sub = subs[i];

		delete_from_channel(ssd, channel, subs[i]);
	}

	ssd->channel_head[channel].current_state = CHANNEL_TRANSFER;
	ssd->channel_head[channel].current_time = ssd->current_time;
	ssd->channel_head[channel].next_state = CHANNEL_IDLE;
	ssd->channel_head[channel].next_state_predict_time = last_sub->complete_time;

	//��ͬ�ĸ߼�������д���ʵ�ʱ��������������������������ʱ����chip��ʱ������
	if (command == NORMAL)
	{
		prog_time = ssd->parameter->time_characteristics.tPROG;
	}
	else
	{
		printf("Error! commond error\n");
		getchar();
	}

	ssd->channel_head[channel].chip_head[chip].current_state = CHIP_WRITE_BUSY;
	ssd->channel_head[channel].chip_head[chip].current_time = ssd->current_time;
	ssd->channel_head[channel].chip_head[chip].next_state = CHIP_IDLE;
	ssd->channel_head[channel].chip_head[chip].next_state_predict_time = ssd->channel_head[channel].next_state_predict_time + prog_time;

	//����ǰ��������д���ʵ�ʱ��
	for (i = 0; i < subs_count; i++)
	{
		subs[i]->next_state_predict_time = subs[i]->next_state_predict_time + prog_time;
		subs[i]->complete_time = subs[i]->next_state_predict_time;
	}

	return ssd;

}


/*****************************************************************************************
*Function is to remove the request from ssd-> subs_w_head or ssd-> channel_head [channel] .subs_w_head
******************************************************************************************/
struct ssd_info *delete_from_channel(struct ssd_info *ssd, unsigned int channel, struct sub_request * sub_req)
{
	struct sub_request *sub = NULL, *p;

	if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION || ssd->parameter->allocation_scheme == HYBRID_ALLOCATION)
		sub = ssd->subs_w_head;
	else if (ssd->parameter->allocation_scheme == STATIC_ALLOCATION)
		sub = ssd->channel_head[channel].subs_w_head; 

	p = sub;
	while (sub != NULL)
	{
		if (sub == sub_req)
		{
			if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION)
			{
				if (sub == ssd->subs_w_head)
				{
					if (ssd->subs_w_head != ssd->subs_w_tail)
					{
						ssd->subs_w_head = sub->next_node;
						sub = ssd->subs_w_head;
						continue;
					}
					else
					{
						ssd->subs_w_head = NULL;
						ssd->subs_w_tail = NULL;
						p = NULL;
						break;
					}
				}
				else
				{
					if (sub->next_node != NULL)
					{
						p->next_node = sub->next_node;
						sub = p->next_node;
						continue;
					}
					else
					{
						ssd->subs_w_tail = p;
						ssd->subs_w_tail->next_node = NULL;
						break;
					}
				}
			}
			else {
				printf("Error, alloc sch\n");
				while(1){}
			}
		}
		p = sub;
		sub = sub->next_node;
	}

	return ssd;
}

