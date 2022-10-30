/*****************************************************************************************************************************
This is a project on 3Dsim, based on ssdsim under the framework of the completion of structures, the main function:
1.Support for 3D commands, for example:mutli plane\interleave\copyback\program suspend/Resume..etc
2.Multi - level parallel simulation
3.Clear hierarchical interface
4.4-layer structure

FileName： ftl.c
Author: Zuo Lu 		Version: 2.0	Date:2017/02/07
Description: 
ftl layer: can not interrupt the global gc operation, gc operation to migrate valid pages using ordinary read and write operations, remove support copyback operation;

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

#include "initialize.h"
#include "ssd.h"
#include "flash.h"
#include "buffer.h"
#include "interface.h"
#include "ftl.h"
#include "fcl.h"

extern int secno_num_per_page, secno_num_sub_page;
int gc_err_cyc = 0;
int greedy_gc_err_cyc = 0;
/******************************************************************************************下面是ftl层map操作******************************************************************************************/

/***************************************************************************************************
*function is given in the channel, chip, die, plane inside find an active_block and then find a page 
*inside the block, and then use find_ppn find ppn
****************************************************************************************************/
struct ssd_info *get_ppn(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, struct sub_request *sub)
{
	int old_ppn = -1;
	unsigned int ppn, lpn, full_page;
	unsigned int active_block;
	unsigned int block;
	unsigned int page, flag = 0;
	unsigned int old_state = 0, state = 0, copy_subpage = 0;
	struct local *location;
	struct direct_erase *direct_erase_node, *new_direct_erase;
	struct gc_operation *gc_node;
	unsigned int last_written_page_;
	int repeated_cnt = 0;
	unsigned int i = 0, j = 0, k = 0, l = 0, m = 0, n = 0;

#ifdef DEBUG
	printf("enter get_ppn,channel:%d, chip:%d, die:%d, plane:%d\n", channel, chip, die, plane);
#endif

	full_page = ~(0xffffffff << (ssd->parameter->subpage_page));
	lpn = sub->lpn;

	/*************************************************************************************
	*Use the function find_active_block() to find active blocks in channel, chip, die, plane
	*And modify the channel, chip, die, plane, active_block under the last_write_page and free_page_num
	**************************************************************************************/
	int channel_cyc = 0, chip_cyc = 0;

bsh_next_chip:
bsh_next_channel:
	if (find_active_block(ssd, channel, chip, die, plane) == FAILURE)
	{
		channel = (channel + 1) % ssd->parameter->channel_number;
		if (++channel_cyc > ssd->parameter->channel_number) {
			chip = (chip + 1) % ssd->parameter->chip_channel[0];
			if (++chip_cyc > ssd->parameter->chip_channel[0]) {
				printf("Error, \n\nssd is bad\n\n");
				print_exit_info_and_while(ssd);
			}
			else {
				goto bsh_next_chip;
			}
		}
		else {
			goto bsh_next_channel;
		}	
		printf("ERROR :there is no free page in channel:%d, chip:%d, die:%d, plane:%d\n", channel, chip, die, plane);
		while(1){}
	}

	active_block = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page++;
	
	if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page >(ssd->parameter->page_block - 1))
	{
		printf("%s %d\n", __func__, __LINE__);
		printf("error! the last write page larger than max!!\n");
		while (1){}
	}
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;

	block = active_block;
	page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page; 


	if (ssd->dram->map->map_entry[lpn].state == 0)                                       /*this is the first logical page*/
	{
		if (ssd->dram->map->map_entry[lpn].pn != 0)
		{
			printf("Error in get_ppn()--1\n");
			//getchar();
		}
		ssd->dram->map->map_entry[lpn].pn = find_ppn(ssd, channel, chip, die, plane, block, page);
		ssd->dram->map->map_entry[lpn].state = sub->state;
	}
	else                                                                            /*This logical page has been updated, and the original page needs to be invalidated*/
	{
		ppn = ssd->dram->map->map_entry[lpn].pn;
		location = find_location(ssd, ppn);
		if (ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn != lpn)
		{

			printf("\nError in get_ppn()--2, %d, %d, %d\n",
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn,
				lpn,
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state );
			//getchar();
		}

		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state = 0;           
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state = 0;              
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn = 0;
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;

		/*******************************************************************************************
		*The block is invalid in the page, it can directly delete, in the creation of an erase node, 
		*hanging under the location of the plane below
		********************************************************************************************/
		if (ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num == ssd->parameter->page_block)
		{
			new_direct_erase = (struct direct_erase *)malloc(sizeof(struct direct_erase));
			alloc_assert(new_direct_erase, "new_direct_erase");
			memset(new_direct_erase, 0, sizeof(struct direct_erase));

			new_direct_erase->block = location->block;
			new_direct_erase->next_node = NULL;
			direct_erase_node = ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node;
			if (direct_erase_node == NULL)
			{
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node = new_direct_erase;
			}
			else
			{
				new_direct_erase->next_node = ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node;
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node = new_direct_erase;
			}
		}

		free(location);
		location = NULL;
		ssd->dram->map->map_entry[lpn].pn = find_ppn(ssd, channel, chip, die, plane, block, page);
		ssd->dram->map->map_entry[lpn].state = (ssd->dram->map->map_entry[lpn].state | sub->state);
	}


	sub->ppn = ssd->dram->map->map_entry[lpn].pn;                                      /*Modify the sub number request ppn, location and other variables*/
	sub->location->channel = channel;
	sub->location->chip = chip;
	sub->location->die = die;
	sub->location->plane = plane;
	sub->location->block = active_block;
	sub->location->page = page;

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_write_count++;
	ssd->program_count++;                                                         
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].lpn = lpn;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].valid_state = sub->state;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].free_state = (sub->state == 0 ? 1 : 0);
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].written_count++;
	ssd->write_flash_count++;

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].test_pro_count++;

	ssd->channel_head[channel].gc_soft = 0;
	ssd->channel_head[channel].gc_hard = 0;

	if (ssd->parameter->active_write == 0)
	{                                        /*If the number of free_page in plane is less than the threshold set by gc_hard_threshold, gc operation is generated*/
		if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page<(ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_soft_threshold))
		{
			ssd->channel_head[channel].gc_soft = 1;
			if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page < (ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_hard_threshold))
			{
				ssd->channel_head[channel].gc_hard = 1;
			}
			gc_node = (struct gc_operation *)malloc(sizeof(struct gc_operation));
			alloc_assert(gc_node, "gc_node");
			memset(gc_node, 0, sizeof(struct gc_operation));
			if (ssd->channel_head[channel].gc_soft == 1)
				gc_node->soft = 1;
			if (ssd->channel_head[channel].gc_hard == 1)
				gc_node->hard = 1;

			gc_node->next_node = NULL;
			gc_node->channel = channel;
			gc_node->chip = chip;
			gc_node->die = die;
			gc_node->plane = plane;
			gc_node->block = 0xffffffff;
			gc_node->page = 0;
			gc_node->state = GC_WAIT;
			gc_node->priority = GC_UNINTERRUPT;
			gc_node->next_node = ssd->channel_head[channel].gc_command;
			ssd->channel_head[channel].gc_command = gc_node;
			ssd->gc_request++;
		}
	}

	return ssd;
}


/*****************************************************************************
*The function is based on the parameters channel, chip, die, plane, block, page, 
*find the physical page number
******************************************************************************/
unsigned int find_ppn(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int block, unsigned int page)
{
	unsigned int ppn = 0;
	unsigned int i = 0;
	int page_plane = 0, page_die = 0, page_chip = 0;
	int page_channel[100];                 

#ifdef DEBUG
	printf("enter find_psn,channel:%d, chip:%d, die:%d, plane:%d, block:%d, page:%d\n", channel, chip, die, plane, block, page);
#endif

	/***************************************************************
	*Calculate the number of pages in plane, die, chip, and channel
	****************************************************************/
	page_plane = ssd->parameter->page_block*ssd->parameter->block_plane;
	page_die = page_plane*ssd->parameter->plane_die;
	page_chip = page_die*ssd->parameter->die_chip;
	while (i<ssd->parameter->channel_number)
	{
		page_channel[i] = ssd->parameter->chip_channel[i] * page_chip;
		i++;
	}

	/****************************************************************************
	*Calculate the physical page number ppn, ppn is the sum of the number of pages 
	*in channel, chip, die, plane, block, page
	*****************************************************************************/
	i = 0;
	while (i<channel)
	{
		ppn = ppn + page_channel[i];
		i++;
	}
	ppn = ppn + page_chip*chip + page_die*die + page_plane*plane + block*ssd->parameter->page_block + page;

	return ppn;
}


/************************************************************************************
*function is based on the physical page number ppn find the physical page where the 
*channel, chip, die, plane, block,In the structure location and as a return value
*************************************************************************************/
struct local *find_location(struct ssd_info *ssd, unsigned int ppn)
{
	struct local *location = NULL;
	unsigned int i = 0;
	int pn, ppn_value = ppn;
	int page_plane = 0, page_die = 0, page_chip = 0, page_channel = 0;

	pn = ppn;

#ifdef DEBUG
	printf("enter find_location\n");
#endif

	location = (struct local *)malloc(sizeof(struct local));
	alloc_assert(location, "location");
	memset(location, 0, sizeof(struct local));

	page_plane = ssd->parameter->page_block*ssd->parameter->block_plane;
	page_die = page_plane*ssd->parameter->plane_die;
	page_chip = page_die*ssd->parameter->die_chip;
	page_channel = page_chip*ssd->parameter->chip_channel[0];

	location->channel = ppn / page_channel;
	location->chip = (ppn%page_channel) / page_chip;
	location->die = ((ppn%page_channel) % page_chip) / page_die;
	location->plane = (((ppn%page_channel) % page_chip) % page_die) / page_plane;
	location->block = ((((ppn%page_channel) % page_chip) % page_die) % page_plane) / ssd->parameter->page_block;
	location->page = (((((ppn%page_channel) % page_chip) % page_die) % page_plane) % ssd->parameter->page_block) % ssd->parameter->page_block;

	return location;
}

/*******************************************************************
*When executing a write request, get ppn for a normal write request
*********************************************************************/
Status get_ppn_for_normal_command(struct ssd_info * ssd, unsigned int channel, unsigned int chip, struct sub_request * sub)
{
	unsigned int die, plane;

	if (sub == NULL)
	{
		return ERROR;
	}
	if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION)
	{
		die = ssd->channel_head[channel].chip_head[chip].token;
		plane = ssd->channel_head[channel].chip_head[chip].die_head[die].token;
		get_ppn(ssd, channel, chip, die, plane, sub);
	
		//更新下次操作的令牌
		// update next op's token
		if (ssd->parameter->dynamic_allocation == STRIPE_DYNAMIC_ALLOCATION)	// changing order: channel -> chip -> die -> plane 
		{																		// to get higher parallelism
			
			//ssd->token = (ssd->token + 1) % ssd->parameter->channel_number; // skip to next channel
			//if (ssd->token == 0) {
				//ssd->channel_head[channel].token = (ssd->channel_head[channel].token + 1) % ssd->parameter->chip_channel[0]; // chip
				//if (ssd->channel_head[channel].token == 0) {
					ssd->channel_head[channel].chip_head[chip].token = (ssd->channel_head[channel].chip_head[chip].token + 1) % ssd->parameter->die_chip; // die
					if (ssd->channel_head[channel].chip_head[chip].token == 0) {
						ssd->channel_head[channel].chip_head[chip].die_head[die].token = (plane + 1) % ssd->parameter->plane_die; // plane
					}
				//}
			//}
		}
		else {
			printf("Error, dynamic alloc");
			while (1){}
		}
		
		compute_serve_time(ssd, channel, chip, die, &sub, 1, NORMAL);
	}
	else {
		printf("Error, alloc sch8\n");
		while(1){}
	}

	return SUCCESS;
}


/******************************************************************************************下面是ftl层gc操作******************************************************************************************/

/************************************************************************************************************
*Gc operation, for the invalid block, the use of mutli erase select two plane offset address of the same invalid block to erase,
*For the valid block, select the two planes within the invalid page of the largest block to erase, and migrate a valid page, 
*the purpose of this is to ensure that the use of mutli hit, that is, for the die, each erase the super block , In the mutli 
*plane write, only need to ensure that the page offset consistent, do not guarantee blcok offset address can be consistent.
************************************************************************************************************/


unsigned int gc(struct ssd_info *ssd, unsigned int channel, unsigned int flag)
{
	unsigned int i;

	//printf("gc flag=%d\n",flag);
	//Active gc
	if (flag == 1)                                                                       /*The whole ssd is the case of IDEL*/
	{
		for (i = 0; i<ssd->parameter->channel_number; i++)
		{
			if ((ssd->channel_head[i].current_state == CHANNEL_IDLE) || (ssd->channel_head[i].next_state == CHANNEL_IDLE&&ssd->channel_head[i].next_state_predict_time <= ssd->current_time))
			{
				if (ssd->channel_head[i].gc_command != NULL)
				{
					if (gc_for_channel(ssd, i, 1) == SUCCESS)
					{
						ssd->gc_count++;
					}
				}
			}
		}
		return SUCCESS;
	}
	//Passive gc
	else
	{
		//当读写子请求都完成的情况下，才去执行gc操作，否则先去执行读写请求
		//if ((ssd->channel_head[channel].subs_r_head != NULL) || (ssd->channel_head[channel].subs_w_head != NULL) || (ssd->subs_w_head != NULL))    
		//{
		//return 0;
		//}
		if (gc_for_channel(ssd, channel, 0) == SUCCESS)
		{
			ssd->gc_count++;
			return SUCCESS;
		}
		else
			return FAILURE;
	}
	return FAILURE;
}


/************************************************************
*this function is to handle every gc operation of the channel
************************************************************/
Status gc_for_channel(struct ssd_info *ssd, unsigned int channel, unsigned int flag)
{
	int flag_direct_erase = 1, flag_gc = 1, flag_suspend = 1;
	unsigned int chip, die, plane, flag_priority = 0;
	unsigned int hard, soft;
	struct gc_operation *gc_node = NULL;

	/*******************************************************************************************
	*Find each gc_node, get the current state of the chip where gc_node is located, the next state,
	*the expected time of the next state .If the current state is idle, or the next state is idle
	*and the next state is expected to be less than the current time, and is not interrupted gc
	*Then the flag_priority order is 1, otherwise 0.
	********************************************************************************************/
	gc_node = ssd->channel_head[channel].gc_command;

	// process ONE node each time go_for_channel is exec when channel and chip are both idle
	while (gc_node != NULL)
	{	
		if ((ssd->channel_head[channel].chip_head[gc_node->chip].current_state == CHIP_IDLE) ||
			((ssd->channel_head[channel].chip_head[gc_node->chip].next_state == CHIP_IDLE) && (ssd->channel_head[channel].chip_head[gc_node->chip].next_state_predict_time <= ssd->current_time)))
		{	
				break;																	/*Processing the nearest free node on the current channel gc request chain*/
		}
		
		gc_node = gc_node->next_node;
	}

	if (gc_node == NULL)
	{
		return FAILURE;
	}

	chip = gc_node->chip;
	die = gc_node->die;
	
	flag_direct_erase = gc_direct_erase(ssd, channel, chip, die);
	if (flag_direct_erase != SUCCESS)
	{
		flag_gc = greedy_gc(ssd, channel, chip, die);							 /*When a complete gc operation is completed, return 1, the corresponding channel gc operation request node to delete*/
		if (flag_gc == SUCCESS)
		{
			delete_gc_node(ssd, channel, gc_node);
		}
		else
		{
			return FAILURE;
		}

	}
	else
	{
		delete_gc_node(ssd, channel, gc_node);
	}
	return SUCCESS;
}



/*******************************************************************************************************************
*GC operation in a number of plane selected two offset address of the same block to erase, and in the invalid block 
*on the table where the invalid block node, erase success, calculate the mutli plane erase operation of the implementation 
*time, channel chip status Change time
*********************************************************************************************************************/
int gc_direct_erase(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die)
{
	unsigned int i,j, plane, block;		
	unsigned int * erase_block;
	struct direct_erase * direct_erase_node = NULL;

	erase_block = (unsigned int*)malloc(ssd->parameter->plane_die * sizeof(erase_block));
	for ( i = 0; i < ssd->parameter->plane_die; i++)
	{
		direct_erase_node = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[i].erase_node;
		if (direct_erase_node == NULL)
		{
			free(erase_block);
			erase_block = NULL;
			return FAILURE;
		}

		//Perform mutli plane erase operation,and delete gc_node
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[i].erase_node = direct_erase_node->next_node;
		erase_block[i] = direct_erase_node->block;

		free(direct_erase_node);
		ssd->direct_erase_count++;
		direct_erase_node = NULL;
	}

	//首先进行channel的跳转，仅是传输命令的时间
	ssd->channel_head[channel].current_state = CHANNEL_TRANSFER;
	ssd->channel_head[channel].current_time = ssd->current_time;
	ssd->channel_head[channel].next_state = CHANNEL_IDLE;
	ssd->channel_head[channel].next_state_predict_time = ssd->current_time + 7 * ssd->parameter->plane_die * ssd->parameter->time_characteristics.tWC;   //14表示的是传输命令的时间，为mutli plane


	
	for (j = 0; j < ssd->parameter->plane_die; j++)
	{
		plane = j;
		block = erase_block[j];
		erase_operation(ssd, channel, chip, die, plane, block);
	}

	ssd->mplane_erase_count++;
	ssd->channel_head[channel].chip_head[chip].current_state = CHIP_ERASE_BUSY;
	ssd->channel_head[channel].chip_head[chip].current_time = ssd->current_time;
	ssd->channel_head[channel].chip_head[chip].next_state = CHIP_IDLE;
	ssd->channel_head[channel].chip_head[chip].next_state_predict_time = ssd->channel_head[channel].next_state_predict_time + ssd->parameter->time_characteristics.tBERS;
	free(erase_block);
	erase_block = NULL;
	return SUCCESS;
}

#if 0
int timeline_went_wrong(struct ssd_info * ssd, int channel, int chip) {
	// timeline can't be pushed ahead.
	if (ssd->subs_w_head == NULL && ssd->request_queue_length >= ssd->parameter->queue_length
		&& !((ssd->channel_head[channel].chip_head[chip].current_state == CHIP_READ_BUSY) ||
		((ssd->channel_head[channel].chip_head[chip].next_state == CHIP_READ_BUSY) &&
			(ssd->channel_head[channel].chip_head[chip].next_state_predict_time <= ssd->current_time)))
		)
		return 1;
	else
		return 0;
}
#endif
/*******************************************************************************************************************************************
*The target plane can not be directly deleted by the block, need to find the target erase block after the implementation of the erase operation, 
*the successful deletion of a block, returns 1, does not delete a block returns -1
********************************************************************************************************************************************/
int greedy_gc(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die)
{
	unsigned int i = 0, j = 0, p = 0 ,invalid_page = 0;
	unsigned int active_block1, active_block2, transfer_size = 0, free_page, avg_page_move = 0;                           /*Record the maximum number of blocks that are invalid*/
	struct local *  location = NULL;
	unsigned int plane , move_plane;
	int block1, block2;
	
	unsigned int active_block;
	unsigned int block;
	unsigned int page_move_count = 0;
	struct direct_erase * direct_erase_node_tmp = NULL;
	struct direct_erase * pre_erase_node_tmp = NULL;
	unsigned int * erase_block;
	unsigned int aim_page;


	erase_block = (unsigned int*)malloc( ssd->parameter->plane_die * sizeof(erase_block));
	//gets active blocks within all plane
	for ( p = 0; p < ssd->parameter->plane_die; p++)
	{
		//find the largest number of invalid pages in plane
		invalid_page = 0;
		block = -1;
		direct_erase_node_tmp = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[p].erase_node;
		for (i = 0; i<ssd->parameter->block_plane; i++)		 /*Find the maximum number of invalid_page blocks, and the largest invalid_page_num*/
		{
			if ((ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[p].blk_head[i].invalid_page_num>invalid_page)) /*Can not find the current active block*/
			{
				invalid_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[p].blk_head[i].invalid_page_num;
				block = i;
			}
		}
		if (block == -1) {
			//printf("All pages're valid\n");
			free(erase_block);
			erase_block = NULL;
			return SUCCESS;
		}
		//Check whether all is invalid page, if all is, then the current block is invalid block, need to remove this node from the erase chain
		if (invalid_page == ssd->parameter->page_block)
		{
			while (direct_erase_node_tmp != NULL)
			{
				if (block == direct_erase_node_tmp->block) // if most-invalid-pages block is in erase chain, it need to get out of erase chain
				{
					if (pre_erase_node_tmp == NULL)
						ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[p].erase_node = direct_erase_node_tmp->next_node;
					else
						pre_erase_node_tmp->next_node = direct_erase_node_tmp->next_node;

					free(direct_erase_node_tmp);
					direct_erase_node_tmp = NULL;
					break;
				}
				else
				{
					pre_erase_node_tmp = direct_erase_node_tmp;
					direct_erase_node_tmp = direct_erase_node_tmp->next_node;
				}
			}
			pre_erase_node_tmp = NULL;
			direct_erase_node_tmp = NULL;
		}

		//caculate sum of  vaild page_move count
		page_move_count += ssd->parameter->page_block - ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[p].blk_head[block].invalid_page_num;
		erase_block[p] = block;
	}

	//caculate the average of the sum vaild page_block,and distribute equally to all plane of die
	avg_page_move = page_move_count / (ssd->parameter->plane_die);

	//Perform a migration of valid data pages
	free_page = 0;
	page_move_count = 0;
	move_plane = 0;
	for (j = 0; j < ssd->parameter->plane_die; j++)
	{
		plane = j;
		block = erase_block[j];
		for (i = 0; i < ssd->parameter->page_block; i++)		                                                     /*Check each page one by one, if there is a valid data page need to move to other places to store*/
		{
			if ((ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].free_state == 1) && (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].fail == 0))
			{
				free_page++;
			}
			if (free_page != 0)
			{
				//printf("\ntoo much free page. \t %d\t .%d\t%d\t%d\t%d\t\n", free_page, channel, chip, die, plane); /*There are free pages, proved to be active blocks, blocks are not finished, can not be erased*/
				//getchar();
			}

			 /*The page is a valid page that requires a copyback operation*/
			if ((ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state > 0) && (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].fail == 0))
			{
				location = (struct local *)malloc(sizeof(struct local));
				alloc_assert(location, "location");
				memset(location, 0, sizeof(struct local));
				location->channel = channel;
				location->chip = chip;
				location->die = die;
				location->plane = plane;
				location->block = block;
				location->page = i;
				page_move_count++;

				move_page(ssd, location, move_plane, &transfer_size);  // valid pages locate in `location`, dest: move_plane           /*Real move_page operation*/
				move_plane = (move_plane + 1) % ssd->parameter->plane_die;

				free(location);
				location = NULL;
			}
		}
	}

	//迁移有效页的时间推动
	// push the timeline of channels related to valid pages ahead.
	ssd->channel_head[channel].current_state = CHANNEL_GC;
	ssd->channel_head[channel].current_time = ssd->current_time;
	ssd->channel_head[channel].next_state = CHANNEL_IDLE;
	ssd->channel_head[channel].next_state_predict_time = ssd->current_time +
	page_move_count*(7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tR + 7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tPROG) +
	transfer_size*SECTOR*(ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tRC);

	//有效页迁移完成，开始执行擦除操作,擦除无效block, i.e., 源block
	// after valid-pages completion, invalid blocks(i.e., source blocks) start to be erased.	
	for (j = 0; j < ssd->parameter->plane_die; j++)
	{
		plane = j;
		block = erase_block[j];
		erase_operation(ssd, channel, chip, die, plane, block);					
	}
	ssd->mplane_erase_count++;
	ssd->channel_head[channel].chip_head[chip].current_state = CHIP_ERASE_BUSY;
	ssd->channel_head[channel].chip_head[chip].current_time = ssd->current_time;
	ssd->channel_head[channel].chip_head[chip].next_state = CHIP_IDLE;
	ssd->channel_head[channel].chip_head[chip].next_state_predict_time = ssd->channel_head[channel].next_state_predict_time + ssd->parameter->time_characteristics.tBERS;
	
	free(erase_block);
	erase_block = NULL;
	return SUCCESS;
}


/*****************************************************************************************
*This function is for the gc operation to find a new ppn, because in the gc operation need 
*to find a new physical block to store the original physical block data
******************************************************************************************/
struct local get_ppn_for_gc(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane)
{
	unsigned int ppn;
	unsigned int active_block, block, page;
	

#ifdef DEBUG
	printf("enter get_ppn_for_gc,channel:%d, chip:%d, die:%d, plane:%d\n", channel, chip, die, plane);
#endif

	int channel_cyc = 0, chip_cyc = 0;
bsh_next_channel:
bsh_next_chip:
	if (find_active_block(ssd, channel, chip, die, plane) != SUCCESS)
	{
		channel = (channel + 1) % ssd->parameter->channel_number;
		if (++channel_cyc > ssd->parameter->channel_number) {
			chip = (chip + 1) % ssd->parameter->chip_channel[0];
			if (++chip_cyc > ssd->parameter->chip_channel[0]) {
				printf("Error, ssd is bad - 2\n\n");
				print_exit_info_and_while(ssd);
			}
			else {
				goto bsh_next_chip;
			}
		}
		else {
			goto bsh_next_channel;
		}
		printf("\n\n Error int get_ppn_for_gc().\n");
	}
	active_block = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page++;
	while (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page].fail == 1)
	{
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page++;
	}

	if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page>(ssd->parameter->page_block - 1)) //  write op fail
	{
		
		printf("error! the last write page larger than max!!\n");
		printf("Code position: %s %d\n", __func__, __LINE__);
		while (1){}
	}
	//free_page isn't real-used until write op is success, i.e., free_page_num-- 
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;

	block = active_block;
	page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page;

	struct local new_loc;
	new_loc.channel = channel;
	new_loc.chip = chip;
	new_loc.die = die;
	new_loc.plane = plane;
	new_loc.block = block;
	new_loc.page = page;

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_write_count++;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].written_count++;
	ssd->write_flash_count++;
	ssd->program_count++;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].test_gc_count++;

	return new_loc;

}
/*************************************************************
*function is when dealing with a gc operation, the need to gc 
*chain gc_node deleted
**************************************************************/
int delete_gc_node(struct ssd_info *ssd, unsigned int channel, struct gc_operation *gc_node)
{
	struct gc_operation *gc_pre = NULL;
	if (gc_node == NULL)
	{
		return ERROR;
	}

	if (gc_node == ssd->channel_head[channel].gc_command)
	{
		ssd->channel_head[channel].gc_command = gc_node->next_node;
	}
	else
	{
		gc_pre = ssd->channel_head[channel].gc_command;
		while (gc_pre->next_node != NULL)
		{
			if (gc_pre->next_node == gc_node)
			{
				gc_pre->next_node = gc_node->next_node;
				break;
			}
			gc_pre = gc_pre->next_node;
		}
	}
	free(gc_node);
	gc_node = NULL;
	ssd->gc_request--;
	return SUCCESS;
}


/**************************************************************************************
*Function function is to find active fast, there should be only one active block for 
*each plane, only the active block in order to operate
***************************************************************************************/
Status  find_active_block(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane)
{
	unsigned int active_block = 0;
	unsigned int free_page_num = 0;
	unsigned int count = 0;
	//	int i, j, k, p, t;

	active_block = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
	free_page_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;
	//last_write_page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;
	while ((free_page_num == 0) && (count<ssd->parameter->block_plane))
	{
		active_block = (active_block + 1) % ssd->parameter->block_plane;
		free_page_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;
		count++;
	}

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block = active_block;

	if (count<ssd->parameter->block_plane)
	{
		return SUCCESS;
	}
	else
	{
		return FAILURE;
	}
}


void print_exit_info_and_while(struct ssd_info * ssd) {
	errno_t err;
	if ((err = fopen_s(&(ssd->zyfile), ssd->zyfilename, "w")) != 0)
	{
		printf("file can't open\n");
		return NULL;
	}

	fprintf(ssd->zyfile, "PE of each block\n---------------------------------------------------------------------\n");
	unsigned int i, p, j, k, m;
	for (i = 0; i < ssd->parameter->channel_number; i++)
	{
		for (p = 0; p < ssd->parameter->chip_channel[i]; p++)
		{
			for (j = 0; j < ssd->parameter->die_chip; j++)
			{
				for (k = 0; k < ssd->parameter->plane_die; k++)
				{
					for (m = 0; m < ssd->parameter->block_plane; m++)
					{
						fprintf(ssd->zyfile, "%d ", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].erase_count);
					}
					fprintf(ssd->zyfile, "\n");
				}
			}
		}
	}

	statistic_output(ssd);

	fflush(ssd->zyfile);
	fflush(ssd->outputfile);
	fflush(ssd->statisticfile);
	fclose(ssd->zyfile);
	fclose(ssd->outputfile);
	fclose(ssd->statisticfile);
	print_free_page_nums(ssd);
	print_current_times(ssd);
	printf("Program run in the end.\n");
	//while (1){}
	exit(0);
}


