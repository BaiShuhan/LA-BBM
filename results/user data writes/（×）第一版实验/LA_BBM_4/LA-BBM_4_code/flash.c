/*****************************************************************************************************************************
This is a project on 3Dsim, based on ssdsim under the framework of the completion of structures, the main function:
1.Support for 3D commands, for example:mutli plane\interleave\copyback\program suspend/Resume..etc
2.Multi - level parallel simulation
3.Clear hierarchical interface
4.4-layer structure

FileName�� flash.c
Author: Zuo Lu 		Version: 2.0	Date:2017/02/07
Description:
flash layer: the original ssdsim this layer is not a specific description, it was their own package to achieve, not completed.

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

#define TRESHOLD_WL 200   //bsh
#define INVALID_PPN -1    //bsh
#define INVALID_LPN -1    //bsh



#include "initialize.h"
#include "ssd.h"
#include "flash.h"
#include "buffer.h"
#include "interface.h"
#include "ftl.h"
#include "fcl.h"

extern int secno_num_per_page;


/******************************************************************************************
*function is to erase the operation, the channel, chip, die, plane under the block erase
*******************************************************************************************/

Status erase_operation(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int block)
{
	unsigned int i = 0,j,k,n,m;
    unsigned int failed_page = 0;
	unsigned int wl_block;     //bsh
	struct sub_req* sub_w = NULL;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].plane_erase_count++;

    for (i = 0; i < ssd->parameter->page_block; i++)
    {
        if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].fail == 1)
        {
            failed_page++;
        }
    }
	//printf("\nchecking erase operation......\n");
#if 0
	if (check_block_free_page_issue(ssd, channel, chip, die, plane, block, failed_page) == FAILURE ||
		check_plane_free_page_issue(ssd, channel, chip, die, plane) == FAILURE) {
		printf("In, erase_operation, Error, the two vals calculated by different methods are not equal.\n");	while (1) {}
	}
#endif
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_page_num = ssd->parameter->page_block - failed_page;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_page_num = 0;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page = -1;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].erase_count++;

	for (i = 0; i<ssd->parameter->page_block; i++)
	{
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].free_state = 1;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state = 0;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].lpn = -1;
	}
	ssd->erase_count++;
	int plane_free_pages_ = 0;
	for (int i = 0; i < ssd->parameter->block_plane; i++) {
		plane_free_pages_ += ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].free_page_num;
	}
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page = plane_free_pages_;
	

	//bsh start
	/*
	ĥ����⹦�ܷ���
	��������:��֮���ĥ��������ĳ����ֵ
	�������ܣ�����ĥ������ĥ����С�������������
	*/

	//�������ò�����������ĥ�������ÿ������������ɺ��ж� 

	//judge whether static wear leveling need to be carried out


#if 0
	wl_block = IS_Wear_Leveling(ssd, channel, chip, die, plane, block);

	if (wl_block != -1) //need to wear leveling
	{
		sub_w = ssd->channel_head[channel].subs_w_head;
		wear_leveling(ssd, channel, chip, die, plane, wl_block, block);
	}
	//bsh end
#endif
	return SUCCESS;

}



//bsh start
int IS_Wear_Leveling(struct ssd_info *ssd, int channel, int chip, int die, int plane, int block)
{
	int i, m, j, k, p;
	int erase_cnt, block_no;
	int block_num;
	int block_wear;

	int temp_wear, invalid_data1, invalid_data2, min_wear = 99999;

	block_num = ssd->parameter->channel_number*ssd->parameter->chip_channel[0]*ssd->parameter->die_chip*ssd->parameter->plane_die*ssd->parameter->block_plane;
	block_wear = ssd->erase_count / block_num;

	// �ҳ�ĥ��̶�ԽС����ЧҳԽ���block
	for (i = 0; i<ssd->parameter->channel_number; i++)
	{
		for (m = 0; m < ssd->parameter->chip_channel[i]; m++)
		{
			for (j = 0; j < ssd->parameter->die_chip; j++)
			{
				for (k = 0; k < ssd->parameter->plane_die; k++)
				{
					for (p = 0; p < ssd->parameter->block_plane; p++)
					{
						// if (check_conflict_between_subw_and_wl(ssd, i, m, j, k, p) == 1) continue;
						if (ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].last_write_page == -1 && p != ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].active_block) //not free and active block
																																																		  		
						{
							temp_wear = ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].erase_count;
							if (temp_wear < block_wear)
							{
								min_wear = temp_wear;
								block_no = p;
							}
							if (temp_wear == min_wear)
							{
								invalid_data1 = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_page_num;
								invalid_data2 = ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].invalid_page_num;
								if (invalid_data1 > invalid_data2)
								{
									min_wear = temp_wear;
									block_no = p;
								}
							}
						}
					}
				}
			}
		}
	}

	erase_cnt = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].erase_count;

	if ((erase_cnt - min_wear) >= TRESHOLD_WL)
	{
		return block_no;
	}
	return -1;
}

/*
//ʵ�ֵ���plane�ڵ�ĥ����⣬�����Ҫ�ģ����Ըĳ�ȫ�ַ�Χ�ڵ� ֻ��Ҫ��ȫ�ַ�Χ����һ��ĥ����С������鼴��
    int i;
    int erase_cnt, block_no;
    int sum_wear;
    int plane_wear;

    int temp_wear, tmp_ec, invalid_data1, invalid_data2, min_ec=99999;

    sum_wear = get_plane_sum_wear(ssd, channel, chip, die, plane);   //�˴��ǵ����Ԫ����RBER
    plane_wear = sum_wear / ssd->parameter->block_plane+1;

    for (i = 0; i < ssd->parameter->block_plane; i++)
    {
         if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].next_written_page == ssd->parameter->page_block && i != ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block) //not free and active block
         {
             temp_wear = get_wear(ssd, channel, chip, die, plane, i);
             if (temp_wear < plane_wear)
             {
                 tmp_ec = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].erase_time;
                 if (tmp_ec < min_ec)
                 {
                     min_ec = tmp_ec;
                     block_no = i;
                 }
                 if (tmp_ec == min_ec)
                 {
                     invalid_data1 = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_page_num;
                     invalid_data2 = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num;
                     if (invalid_data1 > invalid_data2)
                     {
                         min_ec = tmp_ec;
                         block_no = i;
                     }
                  }
              }

          }
    }

    erase_cnt = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].erase_time;

    if ((erase_cnt - min_ec) >= TRESHOLD_WL)
    {
        return block_no;
    }
    return -1;
    }
*/
//bsh end

//bsh start
//transfer all data (including valid data and invalid data) in the strong block to free weak block   //Ҫ��s_block��������wl_block,��ĥ��С�Ŀ飩��w_block����gcʱ�����Ŀ�,��ĥ���Ŀ飩,ʵ��Ӧ���������ݾ�����������w_block��gc���Ѳ����������������ݣ�����ֻ�轫s_block������д��w_block�м���
Status wear_leveling(struct ssd_info *ssd, int channel, int chip, int die, int plane, int s_block, int w_block)
{
	int i;
	struct sub_request *wl_req, *sub_w;

	// �ж�channel���Ƿ��ж�w_block����д������������д��������s_block������w_blockǨ�����ݣ����������wl
	// �ڵ�296�б����ԭ���� �� ͨ��channel�Ϲ��ص�����sub_w���жԿ�wl_block��д����������鲻����ΪWL����Ǩ�Ƶ�Ŀ��飬Ӧ�������˴�WL.
	// ���������������һ��������
	// ����w_block��ִ���˲�����������е�page������free�����Խ�s_block������ҳǨ����w_block����������ʱchannel���ص�д��������д���д��w_blockĳ��ҳ��д������������ǿ��Դ��ڵģ���Ϊ�����˵Ŀ飬ӳ���Ὣ������Ӧ������page��Ϊfree��
	// �ṩ���´�д����������ôWL��д��sub_w��д�����������û���д��Ҳ��������GC����ЧҳǨ�Ƶ��ڲ�д����ͻ��
	if (check_conflict_between_subw_and_wl(ssd,channel, chip, die, plane, w_block)) {
		fprintf(ssd->bshfile, "wl conflict block, channel: %4d, chip: %4d, die: %4d, plane: %4d, s_block: %4d\n",
			channel, chip, die, plane, s_block);
		fflush(ssd->bshfile);
		return SUCCESS;
	}
	for (i = 0; i < ssd->parameter->page_block; i++)
	{
		//read data 
		ssd->channel_head[channel].next_state_predict_time = ssd->current_time +
			1 * (7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tR) +
			secno_num_per_page*SECTOR*ssd->parameter->time_characteristics.tRC;

		//generate new write req;
		// create sub request and add to command buffer
		wl_req = (struct sub_request *)malloc(sizeof(struct sub_request));
		alloc_assert(wl_req, "sub_request");
		memset(wl_req, 0, sizeof(struct sub_request));
		if (wl_req == NULL)
		{
			printf("error! can't appply for memory space for  wear leveling request\n");
			getchar();
		}
		wl_req->operation = WRITE;
		wl_req->next_node = NULL;
		wl_req->next_subs = NULL;
		wl_req->update = NULL;

		wl_req->location = (struct local*)malloc(sizeof(struct local));
		memset(wl_req->location, 0, sizeof(struct local));
		wl_req->current_state = SR_WAIT;
		wl_req->current_time = ssd->channel_head[channel].next_state_predict_time;

		wl_req->size = size(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[s_block].page_head[i].valid_state);
		wl_req->state = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[s_block].page_head[i].valid_state;
		wl_req->begin_time = ssd->channel_head[channel].next_state_predict_time;

		//allocate the free pages from the weak block 
		wl_req->location->channel = channel;
		wl_req->location->chip = chip;
		wl_req->location->die = die;
		wl_req->location->plane = plane;
		wl_req->location->block = w_block;
		wl_req->location->page = i;
		wl_req->ppn = find_ppn(ssd, channel, chip, die, plane, w_block, i);
		if (wl_req->size > 0)
			wl_req->lpn = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[s_block].page_head[i].lpn;
		else
			wl_req->lpn = INVALID_LPN;


		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[w_block].last_write_page++;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[w_block].free_page_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;

		// ��ѯ��ǰchannel�Ϲ��ص�������wl_req��lpn��ppn�Ƿ���ͬ�����ж�wl_req����ĺ����ԡ�lpn��ͬ����ʾwl_req�����ݷ����仯���ϲ���ͬ�Ĳ��֣�ppn��ͬ����ʾ��ͬ������ͬһ�������ַд�����Ǵ���ģ�sub_w��wl_req��
		sub_w = ssd->channel_head[channel].subs_w_head;
		while (sub_w != NULL && wl_req->lpn != INVALID_LPN)
		{
			if (sub_w->lpn == wl_req->lpn)  //hit the command buffer 
			{
				sub_w->state = sub_w->state | wl_req->state; // ��wl��д��������sub_w�ĺϲ�
				sub_w->size = size(sub_w->state);
				wl_req->state = 0;
				wl_req->size = 0;
				wl_req->lpn = INVALID_LPN;
				break;
			}

			// channel�Ͻ��е�д�����������û�д��gcд)��wl����д���w_block��page��λ����ͬ��������
			if (sub_w->ppn == wl_req->ppn)  //no possibility to write into the same physical position
			{
				printf("%d\n", wl_req->ppn);
				printf("error: write into the same physical address\n");
				getchar();
			}
			sub_w = sub_w->next_node;
		}


		if (wl_req->size > 0)
		{
			//set mapping table invalid 
			// ���ǽ�wl_req->lpn��Ӧ��pn��Ϊ��Ч�����Ǹ���wl_req->lpn��Ӧ��ӳ����Ŀ
			// ssd->dram->map->map_entry[wl_req->lpn].pn = INVALID_PPN;
			ssd->dram->map->map_entry[wl_req->lpn].pn = wl_req->ppn;
			wl_req->lpn = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[s_block].page_head[i].lpn;
		}
		else
		{
			ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[w_block].invalid_page_num++; // wl_req->size ��ʾ��������Ӧ��ҳȫΪ��Ч
																															 // 
		}



		if (ssd->channel_head[channel].subs_w_tail != NULL)
		{
			ssd->channel_head[channel].subs_w_tail->next_node = wl_req;
			ssd->channel_head[channel].subs_w_tail = wl_req;
		}
		else
		{
			ssd->channel_head[channel].subs_w_head = wl_req;
			ssd->channel_head[channel].subs_w_tail = wl_req;
		}
	}

	//erase 
	erase_operation(ssd, channel, chip, die, plane, s_block);

	return SUCCESS;
}
//bsh end

// �жϵ�ǰchannel�ȴ��������Ƿ��ж�block��д����(����ĥ���ھ������Ҳ���ͻ��block)
// ����ֵ��0 - �� �� 1 - ��
int check_conflict_between_subw_and_wl(struct ssd_info *ssd, int channel, int chip, int die, int plane, int block) {
	struct sub_request *sub_w = ssd->channel_head[channel].subs_w_head;
	int largest_ppn = find_ppn(ssd, channel, chip, die, plane, block, ssd->parameter->page_block - 1);
	int small_ppn = find_ppn(ssd, channel, chip, die, plane, block, 0);
	while (sub_w != NULL){
		if (small_ppn <= sub_w->ppn && sub_w->ppn <= largest_ppn) return 1;
		sub_w = sub_w->next_node;
	}
	return 0;
}
/******************************************************************************************
*function is to read out the old active page, set invalid, migrate to the new valid page
*******************************************************************************************/
Status move_page(struct ssd_info * ssd, struct local *location, unsigned int move_plane,unsigned int * transfer_size)
{
	struct local *new_location = NULL;
	unsigned int free_state = 0, valid_state = 0;
	unsigned int lpn = 0, old_ppn = 0, ppn = 0;

	lpn = ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn;
	valid_state = ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state;
	free_state = ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state;

	// ppn = get_ppn_for_gc(ssd, location->channel, location->chip, location->die, move_plane);    /*Find out the ppn must be in gc operation of the plane, in order to use the copyback operation, for the gc operation to obtain ppn*/

	// new_location = find_location(ssd, ppn);     /*Get new_location based on newly acquired ppn*/
	struct local new_loc = get_ppn_for_gc(ssd, location->channel, location->chip, location->die, location->plane);
	new_location = &new_loc;
	(*transfer_size) += size(valid_state);
	ppn = find_ppn(ssd, new_loc.channel, new_loc.chip, new_loc.die, new_loc.plane, new_loc.block, new_loc.page);
	//Migrate to a new active page
	ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].free_state = free_state;
	ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].lpn = lpn;
	ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].valid_state = valid_state;
	ssd->gc_write_count++;

	//Read out the old valid page operation, set invalid
	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state = 0;
	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn = -1; // page corresponding to location is invalid, so its lpn is set invalid, -1.
	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state = 0;
	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;
	ssd->gc_read_count++;
	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_read_count++;

	ssd->dram->map->map_entry[lpn].pn = ppn; // Modify the mapping table


	return SUCCESS;
}