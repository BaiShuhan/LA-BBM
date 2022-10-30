/*****************************************************************************************************************************
This is a project on 3Dsim, based on ssdsim under the framework of the completion of structures, the main function:
1.Support for 3D commands, for example:mutli plane\interleave\copyback\program suspend/Resume..etc
2.Multi - level parallel simulation
3.Clear hierarchical interface
4.4-layer structure

FileName： buffer.c
Author: Zuo Lu 		Version: 2.0	Date:2017/02/07
Description: 
buff layer: only contains data cache (minimum processing size for the sector, that is, unit = 512B), mapping table (page-level);

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
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "ssd.h"
#include "initialize.h"
#include "flash.h"
#include "buffer.h"
#include "interface.h"
#include "ftl.h"
#include "fcl.h"

extern int secno_num_per_page, secno_num_sub_page;

/**********************************************************************************************************************************************
*Buff strategy:Blocking buff strategy
*1--first check the buffer is full, if dissatisfied, check whether the current request to put down the data, if so, put the current request,
*if not, then block the buffer;
*
*2--If buffer is blocked, select the replacement of the two ends of the page. If the two full page, then issued together to lift the buffer
*block; if a partial page 1 full page or 2 partial page, then issued a pre-read request, waiting for the completion of full page and then issued
*And then release the buffer block.
***********************************************************************************************************************************************/
struct ssd_info *buffer_management(struct ssd_info *ssd)
{
	struct request *new_request;

#ifdef DEBUG
	printf("enter buffer_management,  current time:%I64u\n", ssd->current_time);
#endif

	ssd->dram->current_time = ssd->current_time;
	new_request = ssd->request_work;   //取队列上工作指针的请求
	
	if (new_request->operation == READ)
	{
		handle_write_buffer(ssd, new_request);
	}
	else if (new_request->operation == WRITE)
	{   
		handle_write_buffer(ssd, new_request);
	}
	//完全命中，则计算延时
	if (new_request->subs == NULL)
	{
		if (new_request->begin_time != ssd->current_time) {
			printf("Error:80\n");
			getchar();
		}
		new_request->begin_time = ssd->current_time;
		new_request->response_time = ssd->current_time + 1000;
	}

	new_request->cmplt_flag = 1;
	ssd->buffer_full_flag = 0;
	return ssd;
}

struct ssd_info *handle_write_buffer(struct ssd_info *ssd, struct request *req)
{
	unsigned int full_page, lsn, lpn, last_lpn, first_lpn;
	unsigned int mask;
	unsigned int state,offset1 = 0, offset2 = 0, flag = 0;

	//进行4kb对齐
	req->size = ((req->lsn + req->size - 1) / secno_num_sub_page - (req->lsn) / secno_num_sub_page + 1) * secno_num_sub_page;     //req大小  单位扇区
	req->lsn /= secno_num_sub_page;
	req->lsn *= secno_num_sub_page;                                                                                                //req 其实扇区号

	full_page = ~(0xffffffff << ssd->parameter->subpage_page);    //全页，表示lpn的子页都是满的
	lsn = req->lsn;//起始扇区号
	lpn = req->lsn / secno_num_per_page;//起始页号
	last_lpn = (req->lsn + req->size - 1) / secno_num_per_page;
	first_lpn = req->lsn / secno_num_per_page;   //计算lpn

	while (lpn <= last_lpn)       //lpn值在递增
	{
		mask = ~(0xffffffff << (ssd->parameter->subpage_page));   //掩码表示的是子页的掩码        //subpage_page      一页包含4个子页 
		state = mask;   //00001111

		if (lpn == first_lpn)
		{
			//offset表示state中0的个数，也就是第一个页中缺失的部分
			offset1 = ssd->parameter->subpage_page - (((lpn + 1)*secno_num_per_page - req->lsn)/secno_num_sub_page);
			state = state&(0xffffffff << offset1);
		}
		if (lpn == last_lpn)
		{
			offset2 = ssd->parameter->subpage_page - ((lpn + 1)*secno_num_per_page - (req->lsn + req->size)) / secno_num_sub_page;
			state = state&(~(0xffffffff << offset2));
		}


		if (req->operation == READ)                                                     //读写最小单位是page 
			ssd = check_w_buff(ssd, lpn, state, NULL, req);
		else if (req->operation == WRITE)
			ssd = insert2buffer(ssd, lpn, state, NULL, req);

		lpn++;
	}
	
	return ssd;
}


struct ssd_info * check_w_buff(struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req)
{
	unsigned int sub_req_state = 0, sub_req_size = 0, sub_req_lpn = 0;
	struct buffer_group *buffer_node, key;
	struct sub_request *sub_req = NULL;

	key.group = lpn;
	buffer_node = (struct buffer_group*)avlTreeFind(ssd->dram->buffer, (TREE_NODE *)&key);		// buffer node 

	if (buffer_node == NULL)         //未命中，去flash上读
	{
		sub_req = NULL;
		sub_req_state = state;
		sub_req_size = size(state);
		sub_req_lpn = lpn;
		sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, READ);     //生成子请求，挂载在总请求上

		ssd->dram->buffer->read_miss_hit++;            //读命中失效次数+1
	}
	else
	{
		if ((state&buffer_node->stored) == state)   //完全命中
		{
			ssd->dram->buffer->read_hit++;
		}
		else    //部分命中（命中部分在buffer   ，其他去flash读）
		{
			sub_req = NULL;
			sub_req_state = (state | buffer_node->stored) ^ buffer_node->stored;       //^ 异或  相同为0 不同为1
			sub_req_size = size(sub_req_state);     //子请求需要读的子页的数目                通过查看sub_req_state中有几个1即需要读取几个子页
			sub_req_lpn = lpn;
			sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, READ);

			ssd->dram->buffer->read_miss_hit++;
		}
	}
	return ssd;
}

/*******************************************************************************
*The function is to write data to the buffer,Called by buffer_management()
********************************************************************************/
struct ssd_info * insert2buffer(struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req)
{
	int write_back_count, flag = 0;                                                             /*flag表示为写入新数据腾空间是否完成，0表示需要进一步腾，1表示已经腾空*/
	unsigned int sector_count, active_region_flag = 0, free_sector = 0;
	struct buffer_group *buffer_node = NULL, *pt, *new_node = NULL, key;
	struct sub_request *sub_req = NULL, *update = NULL;

	unsigned int sub_req_state = 0, sub_req_size = 0, sub_req_lpn = 0;
	unsigned int add_size;

#ifdef DEBUG
	printf("enter insert2buffer,  current time:%I64u, lpn:%d, state:%d,\n", ssd->current_time, lpn, state);
#endif

	sector_count = size(state);                                                                /*需要写到buffer的sector个数*/
	key.group = lpn;                //group 即一个int 
	buffer_node = (struct buffer_group*)avlTreeFind(ssd->dram->buffer, (TREE_NODE *)&key);    /*在平衡二叉树中寻找buffer node*/

	if (buffer_node == NULL)
	{
		free_sector = ssd->dram->buffer->max_buffer_sector - ssd->dram->buffer->buffer_sector_count;
		if (free_sector >= sector_count)
		{
			flag = 1;
		}
		if (flag == 0)
		{
			write_back_count = sector_count - free_sector;
			while (write_back_count>0)
			{
				sub_req = NULL;
				sub_req_state = ssd->dram->buffer->buffer_tail->stored;
				sub_req_size = size(ssd->dram->buffer->buffer_tail->stored);
				sub_req_lpn = ssd->dram->buffer->buffer_tail->group;
				
				/*
				if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION)
					insert2_command_buffer(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);
				else
					sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);
				*/
				//将请求分配到command_buffer
				distribute2_command_buffer(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);

				ssd->dram->buffer->write_miss_hit++;
				ssd->dram->buffer->buffer_sector_count = ssd->dram->buffer->buffer_sector_count - sub_req_size;
				//ssd->dram->buffer->buffer_sector_count = ssd->dram->buffer->buffer_sector_count - sub_req->size;
				
				pt = ssd->dram->buffer->buffer_tail;
				avlTreeDel(ssd->dram->buffer, (TREE_NODE *)pt);
				if (ssd->dram->buffer->buffer_head->LRU_link_next == NULL){
					ssd->dram->buffer->buffer_head = NULL;
					ssd->dram->buffer->buffer_tail = NULL;
				}
				else{
					ssd->dram->buffer->buffer_tail = ssd->dram->buffer->buffer_tail->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_next = NULL;
				}
				pt->LRU_link_next = NULL;
				pt->LRU_link_pre = NULL;
				AVL_TREENODE_FREE(ssd->dram->buffer, (TREE_NODE *)pt);
				pt = NULL;

				write_back_count = write_back_count - sub_req_size;                            /*因为产生了实时写回操作，需要将主动写回操作区域增加*/
				//write_back_count = write_back_count - sub_req->size;
			}
		}

		/******************************************************************************
		*生成一个buffer node，根据这个页的情况分别赋值个各个成员，添加到队首和二叉树中
		*******************************************************************************/
		new_node = NULL;
		new_node = (struct buffer_group *)malloc(sizeof(struct buffer_group));
		alloc_assert(new_node, "buffer_group_node");
		memset(new_node, 0, sizeof(struct buffer_group));

		new_node->group = lpn;
		new_node->stored = state;
		new_node->dirty_clean = state;
		new_node->LRU_link_pre = NULL;
		new_node->LRU_link_next = ssd->dram->buffer->buffer_head;
		if (ssd->dram->buffer->buffer_head != NULL){
			ssd->dram->buffer->buffer_head->LRU_link_pre = new_node;
		}
		else{
			ssd->dram->buffer->buffer_tail = new_node;
		}
		ssd->dram->buffer->buffer_head = new_node;
		new_node->LRU_link_pre = NULL;
		avlTreeAdd(ssd->dram->buffer, (TREE_NODE *)new_node);
		ssd->dram->buffer->buffer_sector_count += sector_count;
		ssd->dram->buffer->write_hit++;
	}
	else
	{
		ssd->dram->buffer->write_hit++;
		if ((state&buffer_node->stored) == state)   //完全命中
		{
			if (req != NULL)
			{
				if (ssd->dram->buffer->buffer_head != buffer_node)
				{
					if (ssd->dram->buffer->buffer_tail == buffer_node)
					{
						ssd->dram->buffer->buffer_tail = buffer_node->LRU_link_pre;
						buffer_node->LRU_link_pre->LRU_link_next = NULL;
					}
					else if (buffer_node != ssd->dram->buffer->buffer_head)
					{
						buffer_node->LRU_link_pre->LRU_link_next = buffer_node->LRU_link_next;
						buffer_node->LRU_link_next->LRU_link_pre = buffer_node->LRU_link_pre;
					}
					buffer_node->LRU_link_next = ssd->dram->buffer->buffer_head;
					ssd->dram->buffer->buffer_head->LRU_link_pre = buffer_node;
					buffer_node->LRU_link_pre = NULL;
					ssd->dram->buffer->buffer_head = buffer_node;
				}
				req->complete_lsn_count += size(state);                                       
			}
		}
		else
		{
			add_size = size((state | buffer_node->stored) ^ buffer_node->stored);					 //需要额外写入缓存的
			while((ssd->dram->buffer->buffer_sector_count + add_size) > ssd->dram->buffer->max_buffer_sector)
			{
				if (buffer_node == ssd->dram->buffer->buffer_tail)                  /*如果命中的节点是buffer中最后一个节点，交换最后两个节点*/
				{
					pt = ssd->dram->buffer->buffer_tail->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_pre = pt->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_pre->LRU_link_next = ssd->dram->buffer->buffer_tail;
					ssd->dram->buffer->buffer_tail->LRU_link_next = pt;
					pt->LRU_link_next = NULL;
					pt->LRU_link_pre = ssd->dram->buffer->buffer_tail;
					ssd->dram->buffer->buffer_tail = pt;

				}
				//写回尾节点
				sub_req = NULL;
				sub_req_state = ssd->dram->buffer->buffer_tail->stored;
				sub_req_size = size(ssd->dram->buffer->buffer_tail->stored);
				sub_req_lpn = ssd->dram->buffer->buffer_tail->group;
				/*
				if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION)
					insert2_command_buffer(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);
				else
					sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);*/
				
				//将请求分配到command_buffer
				distribute2_command_buffer(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);

				ssd->dram->buffer->write_miss_hit++;
				//删除尾节点
				pt = ssd->dram->buffer->buffer_tail;
				avlTreeDel(ssd->dram->buffer, (TREE_NODE *)pt);
				if (ssd->dram->buffer->buffer_head->LRU_link_next == NULL)
				{
					ssd->dram->buffer->buffer_head = NULL;
					ssd->dram->buffer->buffer_tail = NULL;
				}
				else{
					ssd->dram->buffer->buffer_tail = ssd->dram->buffer->buffer_tail->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_next = NULL;
				}
				pt->LRU_link_next = NULL;
				pt->LRU_link_pre = NULL;
				AVL_TREENODE_FREE(ssd->dram->buffer, (TREE_NODE *)pt);
				pt = NULL;

				ssd->dram->buffer->buffer_sector_count = ssd->dram->buffer->buffer_sector_count - sub_req_size;
				//ssd->dram->buffer->buffer_sector_count = ssd->dram->buffer->buffer_sector_count - sub_req->size;
			}
			/*如果该buffer节点不在buffer的队首，需要将这个节点提到队首*/
			if (ssd->dram->buffer->buffer_head != buffer_node)                   
			{
				if (ssd->dram->buffer->buffer_tail == buffer_node)
				{
					buffer_node->LRU_link_pre->LRU_link_next = NULL;
					ssd->dram->buffer->buffer_tail = buffer_node->LRU_link_pre;
				}
				else
				{
					buffer_node->LRU_link_pre->LRU_link_next = buffer_node->LRU_link_next;
					buffer_node->LRU_link_next->LRU_link_pre = buffer_node->LRU_link_pre;
				}
				buffer_node->LRU_link_next = ssd->dram->buffer->buffer_head;
				ssd->dram->buffer->buffer_head->LRU_link_pre = buffer_node;
				buffer_node->LRU_link_pre = NULL;
				ssd->dram->buffer->buffer_head = buffer_node;
			}
			buffer_node->stored = buffer_node->stored | state;
			buffer_node->dirty_clean = buffer_node->dirty_clean | state;
			ssd->dram->buffer->buffer_sector_count += add_size;
		}
	}
	return ssd;
}


/*********************************************************************************************
*The no_buffer_distribute () function is processed when ssd has no dram，
*This is no need to read and write requests in the buffer inside the search, directly use the 
*creat_sub_request () function to create sub-request, and then deal with.
*********************************************************************************************/
struct ssd_info *no_buffer_distribute(struct ssd_info *ssd)
{
	return NULL;
}

/***********************************************************************************
*According to the status of each page to calculate the number of each need to deal 
*with the number of sub-pages, that is, a sub-request to deal with the number of pages
************************************************************************************/
unsigned int size(unsigned int stored)
{
	unsigned int i, total = 0, mask = 0x80000000;

#ifdef DEBUG
	printf("enter size\n");
#endif
	for (i = 1; i <= 32; i++)
	{
		if (stored & mask) total++;     
		stored <<= 1;
	}
#ifdef DEBUG
	printf("leave size\n");
#endif
	return total;
}

struct ssd_info * distribute2_command_buffer(struct ssd_info * ssd, unsigned int lpn, int size_count, unsigned int state, struct request * req, unsigned int operation)
{

	unsigned int method_flag = 1;
	struct buffer_info * aim_command_buffer = NULL;
	struct allocation_info * allocation = NULL;

	//根据不同分配方式调用函数allocation_method分配，返回结果包括channel chip die plane aim_command_buffer 
	allocation = allocation_method(ssd, lpn, ALLOCATION_BUFFER);
    
	//将请求插入到对应的缓存中
	if (allocation->aim_command_buffer != NULL)
		insert2_command_buffer(ssd, allocation->aim_command_buffer, lpn, size_count, state,req, operation);

	//free掉地址空间
	free(allocation);
	return ssd;
}

struct allocation_info * allocation_method(struct ssd_info *ssd,unsigned int lpn,unsigned int use_flag)
{
	struct allocation_info * return_info = (struct allocation_info *)malloc(sizeof(struct allocation_info));
	unsigned int channel_num, chip_num, die_num, plane_num;
	unsigned int aim_die = 0;
	unsigned int type_flag = 0, i = 0;

	__int64 return_distance = 0;
	__int64 max_distance = 0;
	//初始化返回结构体
	return_info->plane = 0;
	return_info->channel = 0;
	return_info->chip = 0;
	return_info->die = 0;

	channel_num = ssd->parameter->channel_number;
	chip_num = ssd->parameter->chip_channel[0];
	die_num = ssd->parameter->die_chip;
	plane_num = ssd->parameter->plane_die;
	if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION)
	{
		if (use_flag == ALLOCATION_BUFFER)
		{
			if (ssd->parameter->dynamic_allocation == STRIPE_DYNAMIC_ALLOCATION)           //优先级
			{
				//按照替换的顺序，轮询分配到每个die_buffer上面
				aim_die = ssd->die_token;
				return_info->aim_command_buffer = ssd->dram->static_die_buffer[aim_die];

				//ssd->die_token = (ssd->die_token + 1) % DIE_NUMBER;

				ssd->plane_count++;
				if (ssd->plane_count % ssd->parameter->plane_die == 0)
				{
					ssd->die_token = (ssd->die_token + 1) % DIE_NUMBER;
					ssd->plane_count = 0;
				}
			}
		}
		else if (use_flag == ALLOCATION_MOUNT){
			return_info->channel = -1;
			return_info->chip = -1;
			return_info->die = -1;
			return_info->plane = -1;
			return_info->mount_flag = SSD_MOUNT;
		}
	}
	return return_info;
}



struct ssd_info * insert2_command_buffer(struct ssd_info * ssd, struct buffer_info * command_buffer, unsigned int lpn, int size_count, unsigned int state, struct request * req, unsigned int operation)
{
	unsigned int i = 0;
	unsigned int sub_req_state = 0, sub_req_size = 0, sub_req_lpn = 0;
	struct buffer_group *command_buffer_node = NULL, *pt, *new_node = NULL, key;
	struct sub_request *sub_req = NULL;
	int tmp;

	//遍历缓存的节点，判断是否有命中
	key.group = lpn;
	command_buffer_node = (struct buffer_group*)avlTreeFind(command_buffer, (TREE_NODE *)&key);

	if (command_buffer_node == NULL)
	{
		//如果是更新写操作，直接写到flash上
		/*
		if ((state&ssd->dram->map->map_entry[lpn].state) != ssd->dram->map->map_entry[lpn].state)
		{
		creat_sub_request(ssd, lpn, size_count, state, req, operation);
		return ssd;
		}
		*/

		//生成 一个buff_node,根据这个页的情况分别赋值给各个成员，并且添加到队首
		new_node = NULL;
		new_node = (struct buffer_group *)malloc(sizeof(struct buffer_group));
		alloc_assert(new_node, "buffer_group_node");
		memset(new_node, 0, sizeof(struct buffer_group));

		new_node->group = lpn;
		new_node->stored = state;
		new_node->dirty_clean = state;
		new_node->LRU_link_pre = NULL;
		new_node->LRU_link_next = command_buffer->buffer_head;
		if (command_buffer->buffer_head != NULL)
		{
			command_buffer->buffer_head->LRU_link_pre = new_node;
		}
		else
		{
			command_buffer->buffer_tail = new_node;
		}
		command_buffer->buffer_head = new_node;
		new_node->LRU_link_pre = NULL;
		avlTreeAdd(command_buffer, (TREE_NODE *)new_node);
		command_buffer->command_buff_page++;

		//如果缓存已满，此时发生flush操作，将缓存的内存一次性flush到闪存上
		if (command_buffer->command_buff_page >= command_buffer->max_command_buff_page)
		{
			if (ssd->warm_flash_cmplt == 0)
				tmp = command_buffer->command_buff_page;
			else
				tmp = command_buffer->max_command_buff_page;
			//printf("begin to flush command_buffer\n");
			for (i = 0; i < tmp; i++)
			{
				sub_req = NULL;
				sub_req_state = command_buffer->buffer_tail->stored;
				sub_req_size = size(command_buffer->buffer_tail->stored);
				sub_req_lpn = command_buffer->buffer_tail->group;
				sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state,req, operation);

				//删除buff中的节点
				pt = command_buffer->buffer_tail;
				avlTreeDel(command_buffer, (TREE_NODE *)pt);
				if (command_buffer->buffer_head->LRU_link_next == NULL){
					command_buffer->buffer_head = NULL;
					command_buffer->buffer_tail = NULL;
				}
				else{
					command_buffer->buffer_tail = command_buffer->buffer_tail->LRU_link_pre;
					command_buffer->buffer_tail->LRU_link_next = NULL;
				}
				pt->LRU_link_next = NULL;
				pt->LRU_link_pre = NULL;
				AVL_TREENODE_FREE(command_buffer, (TREE_NODE *)pt);
				pt = NULL;

				command_buffer->command_buff_page--;
			}
			if (command_buffer->command_buff_page != 0)
			{
				printf("command buff flush failed\n");
				getchar();
			}
		}
	}
	else  //命中的情况，合并扇区数
	{
		if (command_buffer->buffer_head != command_buffer_node)
		{
			if (command_buffer->buffer_tail == command_buffer_node)      //如果是最后一个节点  则交换最后两个
			{
				command_buffer_node->LRU_link_pre->LRU_link_next = NULL;
				command_buffer->buffer_tail = command_buffer_node->LRU_link_pre;
			}
			else
			{
				command_buffer_node->LRU_link_pre->LRU_link_next = command_buffer_node->LRU_link_next;     //如果是中间节点，则操作前一个和后一个节点的指针
				command_buffer_node->LRU_link_next->LRU_link_pre = command_buffer_node->LRU_link_pre;
			}
			command_buffer_node->LRU_link_next = command_buffer->buffer_head;              //提到队首
			command_buffer->buffer_head->LRU_link_pre = command_buffer_node;
			command_buffer_node->LRU_link_pre = NULL;
			command_buffer->buffer_head = command_buffer_node;
		}

		command_buffer_node->stored = command_buffer_node->stored | state;
		command_buffer_node->dirty_clean = command_buffer_node->dirty_clean | state;
	}

	return ssd;
}


/**************************************************************
this function is to create sub_request based on lpn, size, state
****************************************************************/
struct sub_request * creat_sub_request(struct ssd_info * ssd, unsigned int lpn, int size, unsigned int state, struct request * req, unsigned int operation)
{
	struct sub_request* sub = NULL, *sub_r = NULL;
	struct channel_info * p_ch = NULL;
	struct local * loc = NULL;
	unsigned int flag = 0;

	sub = (struct sub_request*)malloc(sizeof(struct sub_request));                        /*申请一个子请求的结构*/
	alloc_assert(sub, "sub_request");
	memset(sub, 0, sizeof(struct sub_request));

	if (sub == NULL)
	{
		return NULL;
	}
	sub->location = NULL;
	sub->next_node = NULL;
	sub->next_subs = NULL;
	sub->update = NULL;

	//将子请求挂载在总请求上，为请求的执行完成做准备
	if (req != NULL)
	{
		sub->next_subs = req->subs;
		req->subs = sub;
		sub->total_request = req;
	}

	/*************************************************************************************
	*在读操作的情况下，有一点非常重要就是要预先判断读子请求队列中是否有与这个子请求相同的，
	*有的话，新子请求就不必再执行了，将新的子请求直接赋为完成
	**************************************************************************************/
	if (operation == READ)
	{
		loc = find_location(ssd, ssd->dram->map->map_entry[lpn].pn);
		sub->location = loc;
		sub->begin_time = ssd->current_time;
		sub->current_state = SR_WAIT;
		sub->current_time = 0x7fffffffffffffff;
		sub->next_state = SR_R_C_A_TRANSFER;
		sub->next_state_predict_time = 0x7fffffffffffffff;
		sub->lpn = lpn;
		sub->size = size;                                                               /*需要计算出该子请求的请求大小*/
		sub->update_read_flag = 0;
		sub->suspend_req_flag = NORMAL_TYPE;

		p_ch = &ssd->channel_head[loc->channel];
		sub->ppn = ssd->dram->map->map_entry[lpn].pn;
		sub->operation = READ;
		sub->state = state;
		sub_r = ssd->channel_head[loc->channel].subs_r_head;

		flag = 0;
		while (sub_r != NULL)                                    
		{
			if (sub_r->ppn == sub->ppn)                             //判断有没有访问相同ppn的子请求    ，依次比较ppn
			{
				flag = 1;
				break;
			}
			sub_r = sub_r->next_node;
		}
		if (flag == 0)                                                //子请求队列中没有访问相同ppn的请求则将新建的sub插入到子请求队列中
		{
			if (ssd->channel_head[loc->channel].subs_r_tail != NULL)
			{
				ssd->channel_head[loc->channel].subs_r_tail->next_node = sub;          //sub挂在子请求队列最后
				ssd->channel_head[loc->channel].subs_r_tail = sub;
			}
			else
			{
				ssd->channel_head[loc->channel].subs_r_head = sub;
				ssd->channel_head[loc->channel].subs_r_tail = sub;
			}
		}
		else                                                               
		{
			sub->current_state = SR_R_DATA_TRANSFER;
			sub->current_time = ssd->current_time;
			sub->next_state = SR_COMPLETE;                                         //置为完成状态
			sub->next_state_predict_time = ssd->current_time + 1000;
			sub->complete_time = ssd->current_time + 1000; 
		}
	}
	/*************************************************************************************
	*写请求的情况下，就需要利用到函数allocate_location(ssd ,sub)来处理静态分配和动态分配了
	**************************************************************************************/
	else if (operation == WRITE)
	{
		sub->ppn = 0;
		sub->operation = WRITE;
		sub->location = (struct local *)malloc(sizeof(struct local));
		alloc_assert(sub->location, "sub->location");
		memset(sub->location, 0, sizeof(struct local));

		sub->current_state = SR_WAIT;
		sub->current_time = ssd->current_time;
		sub->lpn = lpn;
		sub->size = size;
		sub->state = state;
		sub->begin_time = ssd->current_time;

		if (allocate_location(ssd, sub) == ERROR)
		{
			free(sub->location);
			sub->location = NULL;
			free(sub);
			sub = NULL;
			printf("allocate_location error \n");
			getchar();
			return NULL;
		}

	}
	else
	{
		free(sub->location);
		sub->location = NULL;
		free(sub);
		sub = NULL;
		printf("\nERROR ! Unexpected command.\n");
		return NULL;
	}

	return sub;
}

/***************************************
*Write request allocation mount
***************************************/
Status allocate_location(struct ssd_info * ssd, struct sub_request *sub_req)
{
	struct sub_request * update = NULL, *sub_r = NULL;
	struct local *location = NULL;
	unsigned int flag;
	struct allocation_info * allocation1 = NULL;

	//判断是否会产生更新写操作，更新写操作要先读后写
	if (ssd->dram->map->map_entry[sub_req->lpn].state != 0)
	{
		if ((sub_req->state&ssd->dram->map->map_entry[sub_req->lpn].state) != ssd->dram->map->map_entry[sub_req->lpn].state)
		{
			//ssd->read_count++;
			ssd->update_read_count++;
			ssd->update_write_count++;

			update = (struct sub_request *)malloc(sizeof(struct sub_request));
			alloc_assert(update, "update");
			memset(update, 0, sizeof(struct sub_request));

			if (update == NULL)
			{
				return ERROR;
			}
			update->location = NULL;
			update->next_node = NULL;
			update->next_subs = NULL;
			update->update = NULL;
			location = find_location(ssd, ssd->dram->map->map_entry[sub_req->lpn].pn);
			update->location = location;
			update->begin_time = ssd->current_time;
			update->current_state = SR_WAIT;
			update->current_time = 0x7fffffffffffffff;
			update->next_state = SR_R_C_A_TRANSFER;
			update->next_state_predict_time = 0x7fffffffffffffff;
			update->lpn = sub_req->lpn;
			update->state = ((ssd->dram->map->map_entry[sub_req->lpn].state^sub_req->state) & 0x7fffffff);
			update->size = size(update->state);
			update->ppn = ssd->dram->map->map_entry[sub_req->lpn].pn;
			update->operation = READ;
			update->update_read_flag = 1;
			update->suspend_req_flag = NORMAL_TYPE;

			sub_r = ssd->channel_head[location->channel].subs_r_head;
			flag = 0;
			while (sub_r != NULL)
			{
				if (sub_r->ppn == update->ppn)
				{
					flag = 1;
					break;
				}
				sub_r = sub_r->next_node;
			}

			if (flag == 0)
			{
				if (ssd->channel_head[location->channel].subs_r_tail != NULL)
				{
					ssd->channel_head[location->channel].subs_r_tail->next_node = update;
					ssd->channel_head[location->channel].subs_r_tail = update;
				}
				else
				{
					ssd->channel_head[location->channel].subs_r_tail = update;
					ssd->channel_head[location->channel].subs_r_head = update;
				}
			}
			else
			{
				update->current_state = SR_R_DATA_TRANSFER;
				update->current_time = ssd->current_time;
				update->next_state = SR_COMPLETE;
				update->next_state_predict_time = ssd->current_time + 1000;
				update->complete_time = ssd->current_time + 1000;
			}
		}
	}

	//按照不同的分配策略，进行分配
	allocation1 = allocation_method(ssd, sub_req->lpn, ALLOCATION_MOUNT);
	
	sub_req->location->channel = allocation1->channel;
	sub_req->location->chip = allocation1->chip;
	sub_req->location->die = allocation1->die;
	sub_req->location->plane = allocation1->plane;

	//根据mount_flag, 选择挂载在channel上还是ssd上
	if (allocation1->mount_flag == SSD_MOUNT)
	{
		if (ssd->subs_w_tail != NULL)
		{
			ssd->subs_w_tail->next_node = sub_req;
			ssd->subs_w_tail = sub_req;
		}
		else
		{
			ssd->subs_w_tail = sub_req;
			ssd->subs_w_head = sub_req;
		}
	}
	else if (allocation1->mount_flag == CHANNEL_MOUNT)
	{
		if (ssd->channel_head[sub_req->location->channel].subs_w_tail != NULL)
		{
			ssd->channel_head[sub_req->location->channel].subs_w_tail->next_node = sub_req;
			ssd->channel_head[sub_req->location->channel].subs_w_tail = sub_req;
		}
		else
		{
			ssd->channel_head[sub_req->location->channel].subs_w_tail = sub_req;
			ssd->channel_head[sub_req->location->channel].subs_w_head = sub_req;
		}
	}

	//更新读的请求挂载在该请求上
	if (update != NULL)
	{
		sub_req->update_read_flag = 1;
		sub_req->update = update;
		sub_req->state = (sub_req->state | update->state);
		sub_req->size = size(sub_req->state);
	}
	else
	{
		sub_req->update_read_flag = 0;
	}

	//free掉地址空间
	free(allocation1);
	allocation1 = NULL;
	return SUCCESS;
}

struct ssd_info *flush_all(struct ssd_info *ssd)
{
	struct buffer_group *pt;
	struct sub_request *sub_req = NULL;
	unsigned int sub_req_state = 0, sub_req_size = 0, sub_req_lpn = 0;

	struct request *req = (struct request*)malloc(sizeof(struct request));
	alloc_assert(req, "request");
	memset(req, 0, sizeof(struct request));

	int i, j;

	if (ssd->request_queue == NULL)          //The queue is empty
	{
		ssd->request_queue = req;
		ssd->request_tail = req;
		ssd->request_work = req;
		ssd->request_queue_length++;
	}
	else
	{
		(ssd->request_tail)->next_node = req;
		ssd->request_tail = req;
		if (ssd->request_work == NULL)
			ssd->request_work = req;
		ssd->request_queue_length++;
	}

	int max_command_buff_page_tmp1, max_command_buff_page_tmp2;
	if (ssd->trace_over_flag == 1 && ssd->warm_flash_cmplt == 0)
	{
		max_command_buff_page_tmp1 = ssd->dram->command_buffer->max_command_buff_page;
		max_command_buff_page_tmp2 = ssd->dram->static_die_buffer[0]->max_command_buff_page;
		ssd->dram->command_buffer->max_command_buff_page = 1;
		for (i = 0; i < 4; i++)
			ssd->dram->static_die_buffer[i]->max_command_buff_page = 1;
		while (ssd->dram->buffer->buffer_sector_count > 0)
		{
			//printf("%u\n", ssd->dram->buffer->buffer_sector_count);
			sub_req = NULL;
			sub_req_state = ssd->dram->buffer->buffer_tail->stored;
			sub_req_size = size(ssd->dram->buffer->buffer_tail->stored);
			sub_req_lpn = ssd->dram->buffer->buffer_tail->group;

			distribute2_command_buffer(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);

			ssd->dram->buffer->buffer_sector_count = ssd->dram->buffer->buffer_sector_count - sub_req_size;

			pt = ssd->dram->buffer->buffer_tail;
			avlTreeDel(ssd->dram->buffer, (TREE_NODE *)pt);
			if (ssd->dram->buffer->buffer_head->LRU_link_next == NULL) {
				ssd->dram->buffer->buffer_head = NULL;
				ssd->dram->buffer->buffer_tail = NULL;
			}
			else {
				ssd->dram->buffer->buffer_tail = ssd->dram->buffer->buffer_tail->LRU_link_pre;
				ssd->dram->buffer->buffer_tail->LRU_link_next = NULL;
			}
			pt->LRU_link_next = NULL;
			pt->LRU_link_pre = NULL;
			AVL_TREENODE_FREE(ssd->dram->buffer, (TREE_NODE *)pt);
			pt = NULL;
		}
		ssd->dram->command_buffer->max_command_buff_page = max_command_buff_page_tmp1;
		for (i = 0; i < 4; i++)
			ssd->dram->static_die_buffer[i]->max_command_buff_page = max_command_buff_page_tmp2;
	}
	return ssd;
}