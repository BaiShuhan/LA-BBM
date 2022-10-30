/*****************************************************************************************************************************
This is a project on 3Dsim, based on ssdsim under the framework of the completion of structures, the main function:
1.Support for 3D commands, for example:mutli plane\interleave\copyback\program suspend/Resume..etc
2.Multi - level parallel simulation
3.Clear hierarchical interface
4.4-layer structure

FileName： ssd.c
Author: Zuo Lu 		Version: 2.0	Date:2017/02/07
Description: System main function c file, Contains the basic flow of simulation.
Mainly includes: initialization, make_aged, pre_process_page three parts

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
#include <crtdbg.h>  


#include "ssd.h"
#include "initialize.h"
#include "flash.h"
#include "buffer.h"
#include "interface.h"
#include "ftl.h"
#include "fcl.h"

int secno_num_per_page, secno_num_sub_page;

//zy add start
lifetime_zy[576] = { 279,232,241,279,235,244,279,230,243,278,237,242,313,288,288,312,288,294,314,289,289,317,290,295,295,257,261,300,262,266,300,259,262,301,258,263,300,265,268,302,270,273,303,270,274,303,269,272,301,266,271,304,268,271,304,268,270,301,269,274,305,278,277,305,280,282,306,277,277,309,281,277,305,274,279,305,278,282,308,283,283,305,275,279,304,283,283,309,284,283,307,288,282,311,283,282,222,194,186,244,196,187,242,195,184,229,197,186,246,203,203,257,205,201,256,205,201,255,206,203,249,202,201,255,209,203,253,206,200,252,205,201,258,215,209,261,215,210,262,217,211,264,212,209,257,214,209,260,215,207,259,214,205,261,212,206,266,225,218,267,229,221,268,225,220,265,229,223,263,222,216,264,222,220,266,223,222,267,219,218,268,235,228,270,234,232,270,234,229,266,234,231,266,226,223,266,232,225,268,229,226,266,229,222,266,238,233,266,238,234,266,239,235,272,239,234,262,229,224,266,231,229,267,233,226,265,234,228,269,237,237,268,237,237,270,240,239,268,239,235,263,229,225,265,229,229,262,229,233,264,229,229,266,234,234,266,237,233,264,237,233,264,237,236,265,229,227,261,226,226,267,229,226,265,229,226,271,235,232,271,237,232,268,239,235,268,237,234,266,226,222,270,229,225,269,222,223,267,222,219,278,230,224,277,232,225,277,233,225,278,229,227,283,236,225,293,238,218,295,238,228,290,236,218,286,230,211,288,232,215,288,231,212,282,230,212,282,229,213,287,230,212,286,229,213,283,232,212,284,231,215,290,232,213,292,230,214,289,228,210,292,237,220,294,236,221,290,234,222,293,234,218,296,242,228,296,242,227,295,241,225,294,241,225,298,253,235,305,260,232,305,251,233,302,247,231,311,261,240,306,266,241,308,264,242,309,259,235,319,270,259,319,273,259,319,276,259,317,271,246,326,273,271,326,286,269,322,279,267,321,278,265,330,291,281,331,288,280,331,290,278,329,288,272,336,295,278,333,297,285,336,295,282,336,290,283,343,304,294,346,304,291,342,305,288,341,300,291,348,311,302,349,313,298,346,310,296,346,312,295,310,262,232,313,261,235,312,258,234,311,259,233,316,270,237,320,275,240,317,267,239,317,268,232,329,276,245,330,278,246,330,281,248,329,280,249,332,274,256,333,282,256,333,279,249,335,279,262,332,286,261,342,288,267,342,285,271,337,292,265,340,291,275,343,297,275,348,301,275,343,297,278,354,310,290,357,314,290,354,312,298,357,307,295,287,204,173,290,209,200,292,221,203,298,229,229 };


//parameters路径名
char *parameters_file = "page.parameters";

//trace 路径名
char *trace_file = "usr0_16GB.ascii";

//输出结构 路径名
char *result_file_statistic = "hm0_statistic.dat";
char *result_file_ex = "hm0_TDA_ex.dat";

char *zy_file = "zy_ts0.txt";
char *bsh_wl_skip = "bsh_wl_skipped_rec.txt";
char *bsh_bad_block = "bsh_bad_block.txt";

/********************************************************************************************************************************
1，the initiation() used to initialize ssd;
2，the make_aged() used to handle old processing on ssd, Simulation used for some time ssd;
3，the pre_process_page() handle all read request in advance and established lpn<-->ppn of read request in advance ,in order to 
pre-processing trace to prevent the read request is not read the data;
4. the pre_process_write() ensure that up to a valid block contains free page, to meet the actual ssd mechanism after make_aged and pre-process()
5，the simulate() is the actual simulation handles the core functions
6，the statistic_output () outputs the information in the ssd structure to the output file, which outputs statistics and averaging data
7，the free_all_node() release the request for the entire main function
*********************************************************************************************************************************/

void main()
{
	struct ssd_info *ssd;

	//初始化ssd结构体
	ssd = (struct ssd_info*)malloc(sizeof(struct ssd_info));
	alloc_assert(ssd, "ssd");
	memset(ssd, 0, sizeof(struct ssd_info));
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//输入配置文件参数
	strcpy_s(ssd->parameterfilename, 50, parameters_file);

	//输入trace文件参数，输出文件名
	strcpy_s(ssd->tracefilename, 50, trace_file);
	strcpy_s(ssd->outputfilename, 50, result_file_ex);
	strcpy_s(ssd->statisticfilename, 50, result_file_statistic);
	strcpy_s(ssd->zyfilename, 50, zy_file);
	strcpy_s(ssd->bsh_wl_skip_filename, 50, bsh_wl_skip);
	strcpy_s(ssd->bsh_bad_block, 50, bsh_bad_block);
	printf("tracefile:%s begin simulate-------------------------\n", ssd->tracefilename);
	//getchar();
	//开始对当前trace进行仿真
	tracefile_sim(ssd);

	//仿真结束，释放所有的节点
	closefiles(ssd);
	printf("tracefile:%s end simulate---------------------------\n\n\n", ssd->tracefilename);
	free_all_node(ssd);
	
	_CrtDumpMemoryLeaks();  //Memory leak detection

	//所有trace跑完停止当前程序
	system("pause");
}

void closefiles(struct ssd_info *ssd) {
	print_current_times(ssd);
	fflush(ssd->bshfile);
	fclose(ssd->bshfile);
	fflush(ssd->bsh_bad_block_file);
	fclose(ssd->bsh_bad_block_file);
}
/*Find all the sub - requests for the farest arrival of the next state of the time*/

__int64 find_farest_event(struct ssd_info *ssd)
{
	unsigned int i, j;
	__int64 time = 0x7fffffffffffffff;
	__int64 time1 = 0x7fffffffffffffff;
	__int64 time2 = 0x7fffffffffffffff;

	time1 = 0;
	time2 = 0;
	for (i = 0; i<ssd->parameter->channel_number; i++)
	{
		if (ssd->channel_head[i].next_state == CHANNEL_IDLE)
		{
			if (time1<ssd->channel_head[i].next_state_predict_time)
			{
				if (ssd->channel_head[i].next_state_predict_time >= ssd->current_time)//
				{
					time1 = ssd->channel_head[i].next_state_predict_time;
				}

			}
		}

		for (j = 0; j<ssd->parameter->chip_channel[i]; j++)
		{
			if (time2<ssd->channel_head[i].chip_head[j].next_state_predict_time)
			{
				if (ssd->channel_head[i].chip_head[j].next_state_predict_time >= ssd->current_time)
				{
					time2 = ssd->channel_head[i].chip_head[j].next_state_predict_time;
				}
			}
		}
	}
	time = (time1<time2) ? time2 : time1;

	return time;
}

void tracefile_sim(struct ssd_info *ssd)
{
	unsigned  int i, j, k, p, m, n, invalid = 0;
	int cnt = 0;

	#ifdef DEBUG
	printf("enter main\n"); 
	#endif

	//SSD参数初始化
	ssd = initiation(ssd);
	print_current_times(ssd);

	//SSD旧化方法
	warm_flash(ssd);
	print_cur_plane_status(ssd);
	//check_warmflash_corrections(ssd);
	printf("\ntotal pages num per plane = %d\n\n", ssd->parameter->block_plane*ssd->parameter->page_block);

	fprintf(ssd->outputfile,"\t\t\t\t\t\t\t\t\tOUTPUT\n");
	fprintf(ssd->outputfile,"****************** TRACE INFO ******************\n");

	ssd->simulated_start_time = 0;
	while (cnt < 100)
	{
		ssd = simulate(ssd);
		ssd->simulated_start_time = find_farest_event(ssd);
		ssd->trace_over_flag = 0;
		printf("\n\ncnt = %d\n\n", cnt);
		fprintf(ssd->bshfile, "\n\ncnt = %d\n\n", cnt);
		cnt++;
		print_current_times(ssd);
		print_free_page_nums(ssd);
		print_used_and_pe_cyc(ssd);
	}
	print_free_page_nums(ssd);
	statistic_output(ssd);  
	printf("\n");
	printf("the simulation is completed!\n");

	//system("pause");
 	
}
void print_used_and_pe_cyc(struct ssd_info *ssd) {
	unsigned int max_used_ppn = 0;
	unsigned int max_pe_cyc = 0;
	for (int i = 0; i < ssd->parameter->channel_number; i++)
	{
		for (int m = 0; m < ssd->parameter->chip_channel[i]; m++)
		{
			for (int j = 0; j < ssd->parameter->die_chip; j++)
			{
				for (int k = 0; k < ssd->parameter->plane_die; k++)
				{
					for (int b = 0; b < ssd->parameter->block_plane; ++b) {
						for (int p = 0; p < ssd->parameter->page_block; ++p) {
							struct page_info _page = ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[b].page_head[p];
							if (_page.valid_state > 0) {
								max_used_ppn = find_ppn(ssd, i, m, j, k, b, p);
							}
							if (max_pe_cyc < _page.written_count) max_pe_cyc = _page.written_count;
						}
					}
				}
			}
		}
	}
	unsigned int max_ppn = (int)(ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->plane_die*ssd->parameter->die_chip*ssd->parameter->chip_num);
	printf("\n----------	Report status start --------\n");
#define GB (1024*1024)
	printf("max_ppn			:		%u(%lfGB)\n", max_ppn, max_ppn * 16. / GB);
	printf("max_used_ppn	:		%u(%lfGB)\n", max_used_ppn, max_used_ppn * 16. / GB);
	printf("max_pe_cyc		:		%u\n", max_pe_cyc);
	printf("\n----------	Report status end	--------\n");
}
/******************simulate() *********************************************************************
*Simulation () is the core processing function, the main implementation of the features include:
*1, get_requests() :Get a request from the trace file and hang to ssd-> request
*2，buffer_management()/distribute()/no_buffer_distribute() :Make read and write requests through 
the buff layer processing, resulting in read and write sub_requests,linked to ssd-> channel or ssd

*3，process() :Follow these events to handle these read and write sub_requests
*4，trace_output() :Process the completed request, and count the simulation results
**************************************************************************************************/
struct ssd_info *simulate(struct ssd_info *ssd)
{
	int flag=1;
	double output_step=0;
	unsigned int a=0,b=0;
	errno_t err;
	unsigned int i, j, k,m,p;

	printf("\n");
	printf("begin simulating.......................\n");
	printf("\n");
	printf("\n");
	printf("   ^o^    OK, please wait a moment, and enjoy music and coffee   ^o^    \n");

	//Empty the allocation token in pre_process()
	ssd->token = 0;
	for (i = 0; i < ssd->parameter->channel_number; i++)
	{
		for (j = 0; j < ssd->parameter->chip_channel[i]; j++)
		{
			for (k = 0; k < ssd->parameter->die_chip; k++)
			{
				ssd->channel_head[i].chip_head[j].die_head[k].token = 0;
			}
			ssd->channel_head[i].chip_head[j].token = 0;
		}
		ssd->channel_head[i].token = 0;
	}


	if((err=fopen_s(&(ssd->tracefile),ssd->tracefilename,"r"))!=0)
	{  
		printf("the trace file can't open\n");
		return NULL;
	}

	/*fprintf(ssd->outputfile,"      arrive           lsn     size ope     begin time    response time    process time\n");	
	fflush(ssd->outputfile);*/

	int cnt_bsh = 0;
	while(flag!=100)      
	{        
		/*interface layer*/
		flag = get_requests(ssd);        
		// printf("cnt_bsh = %d\n", cnt_bsh++);
		/*buffer layer*/
		if (flag == 1 || (flag == 0 && ssd->request_work != NULL))
		{   
			if (ssd->parameter->dram_capacity!=0)
			{
				if (ssd->buffer_full_flag == 0)				//buffer don't block,it can be handle.
				{
					buffer_management(ssd);
				}
			} 
			else
			{
				no_buffer_distribute(ssd);
			}

			if (ssd->request_work->cmplt_flag == 1) // 已经执行
			{
				if (ssd->request_work != ssd->request_tail)
					ssd->request_work = ssd->request_work->next_node;
				else
					ssd->request_work = NULL;
			}

		}
		
		/*ftl+fcl+flash layer*/
		process(ssd); 

		//output_response_time(ssd);
		trace_output(ssd);
		if (ssd->request_lz_count % 1000 == 0)
			output_response_time(ssd);
		if (flag == 0 && ssd->request_queue == NULL)
			flag = 100;
	}

	fclose(ssd->tracefile);
	return ssd;
}

void output_response_time(struct ssd_info *ssd) {
	static int g_cnt = 0;
	if (g_cnt++ == 0) {
		FILE *f = fopen("Response_time.txt", "w");
		fclose(f);
	}
	else {
		FILE *f = fopen("Response_time.txt", "a+");
		//if (ssd->warm_flash_cmplt == 1) 
		fprintf(f, "write_cnt = %7d, write_avg = %.5lf, read time = %15.1lf, write time = %15.1lf\n",
			ssd->write_request_count, ssd->ave_write_size,
			(double)ssd->read_avg/ssd->read_request_count, 
			(double)ssd->write_avg/ssd->write_request_count);
		fflush(f);
		fclose(f);
	}
}

/********************************************************
*the main function :Controls the state change processing 
*of the read request and the write request
*********************************************************/
// int bsh = 0;
struct ssd_info *process(struct ssd_info *ssd)
{
	// printf("bsh = %d\n", bsh++);
	int old_ppn = -1, flag_die = -1;
	unsigned int i,j,k,m,p,chan, random_num;
	unsigned int flag = 0, new_write = 0, chg_cur_time_flag = 1, flag2 = 0, flag_gc = 0;
	unsigned int count1;
	__int64 time, channel_time = 0x7fffffffffffffff;

#ifdef DEBUG
	printf("enter process,  current time:%I64u\n", ssd->current_time);
#endif

	/*
	if (ssd->channel_head[0].subs_r_head == NULL && ssd->channel_head[0].subs_r_tail != NULL)
		printf("lz\n");
	*/

	/*********************************************************
	*flag=0, processing read and write sub_requests
	*flag=1, processing gc request
	**********************************************************/
	for (i = 0; i<ssd->parameter->channel_number; i++)
	{
		if ((ssd->channel_head[i].subs_r_head == NULL) && (ssd->channel_head[i].subs_w_head == NULL) && (ssd->subs_w_head == NULL))
		{
			flag = 1;
		}
		else
		{
			flag = 0;
			break;
		}
	}
	if (flag == 1)
	{
		ssd->flag = 1;
		if (ssd->gc_request>0)                                                           
		{
			gc(ssd, 0, 1); // exec active gc each time process() is entered at first.   /*Active gc, require all channels must traverse*/
		}
		return ssd;
	}
	else
	{
		ssd->flag = 0;
	}

	/*********************************************************
	*Gc operation is completed, the read and write state changes
	**********************************************************/
	time = ssd->current_time;
	services_2_r_read(ssd);
	services_2_r_complete(ssd);

	random_num = ssd->token;
	for (chan = 0; chan<ssd->parameter->channel_number; chan++)
	{
		i = (random_num + chan) % ssd->parameter->channel_number;
		flag_gc = 0;																		
		ssd->channel_head[i].channel_busy_flag = 0;		
		if ((ssd->channel_head[i].current_state == CHANNEL_IDLE) || (ssd->channel_head[i].next_state == CHANNEL_IDLE&&ssd->channel_head[i].next_state_predict_time <= ssd->current_time))
		{
			//先处理是否有挂起的gc擦除操作，再处理普通的gc操作
			if (ssd->gc_request > 0)
			{
				if (ssd->channel_head[i].gc_soft == 1 && ssd->channel_head[i].gc_hard == 1 && ssd->channel_head[i].gc_command != NULL)
				{
					flag_gc = gc(ssd, i, 0);
					if (flag_gc == 1)
					{
						continue;
					}
				}
			}
			
			/*********************************************************
			*1.First read the wait state of the read request, 
			*2.followed by the state of the read flash
			*3.end by processing write sub_request
			**********************************************************/
			services_2_r_wait(ssd, i);							  //flag=1,channel is busy，else idle....                  
			if ((ssd->channel_head[i].channel_busy_flag == 0) && (ssd->channel_head[i].subs_r_head != NULL))					  //chg_cur_time_flag=1,current_time has changed，chg_cur_time_flag=0,current_time has not changed  			
				services_2_r_data_trans(ssd, i);

			//Write request state jump
			if (ssd->channel_head[i].channel_busy_flag == 0)
				services_2_write(ssd, i); 
		}	
	}
	ssd->token = (ssd->token + 1) % ssd->parameter->channel_number;

	return ssd;
}



/**********************************************************************
*The trace_output () is executed after all the sub requests of each request 
*are processed by the process () 
*Print the output of the relevant results to the outputfile file
**********************************************************************/
void trace_output(struct ssd_info* ssd){
	int flag = 1;
	__int64 start_time, end_time;
	struct request *req, *pre_node;
	struct sub_request *sub, *tmp;

#ifdef DEBUG
	printf("enter trace_output,  current time:%I64u\n", ssd->current_time);
#endif

	pre_node = NULL;
	req = ssd->request_queue;
	start_time = 0;
	end_time = 0;

	if (req == NULL)
		return;

	while (req != NULL)
	{
		sub = req->subs;
		flag = 1;
		start_time = 0;
		end_time = 0;
		if (req->response_time != 0)
		{
			//fprintf(ssd->outputfile, "%16I64u %10u %6u %2u %16I64u %16I64u %10I64u\n", req->time, req->lsn, req->size, req->operation, req->begin_time, req->response_time, req->response_time - req->time);
			//fflush(ssd->outputfile);

			if (req->response_time - req->begin_time == 0)
			{
				printf("the response time is 0?? \n");
				getchar();
			}

			if (req->operation == READ)
			{
				ssd->read_request_count++;
				ssd->read_avg = ssd->read_avg + (req->response_time - req->time);
			}
			else
			{
				ssd->write_request_count++;
				ssd->write_avg = ssd->write_avg + (req->response_time - req->time);
			}

			if (pre_node == NULL)
			{
				if (req->next_node == NULL)
				{
					free(req->need_distr_flag);
					req->need_distr_flag = NULL;
					free(req);
					req = NULL;
					ssd->request_queue = NULL;
					ssd->request_tail = NULL;
					ssd->request_queue_length--;
				}
				else
				{
					ssd->request_queue = req->next_node;
					pre_node = req;
					req = req->next_node;
					free(pre_node->need_distr_flag);
					pre_node->need_distr_flag = NULL;
					free((void *)pre_node);
					pre_node = NULL;
					ssd->request_queue_length--;
				}
			}
			else
			{
				if (req->next_node == NULL)
				{
					pre_node->next_node = NULL;
					free(req->need_distr_flag);
					req->need_distr_flag = NULL;
					free(req);
					req = NULL;
					ssd->request_tail = pre_node;
					ssd->request_queue_length--;
				}
				else
				{
					pre_node->next_node = req->next_node;
					free(req->need_distr_flag);
					req->need_distr_flag = NULL;
					free((void *)req);
					req = pre_node->next_node;
					ssd->request_queue_length--;
				}
			}
		}
		else
		{
			flag = 1;
			while (sub != NULL)
			{
				if (start_time == 0)
					start_time = sub->begin_time;
				if (start_time > sub->begin_time)
					start_time = sub->begin_time;
				if (end_time < sub->complete_time)
					end_time = sub->complete_time;
				if ((sub->current_state == SR_COMPLETE) || ((sub->next_state == SR_COMPLETE) && (sub->next_state_predict_time <= ssd->current_time)))	// if any sub-request is not completed, the request is not completed
				{
					sub = sub->next_subs;
				}
				else
				{
					flag = 0;
					break;
				}

			}

			if (flag == 1)
			{
				//fprintf(ssd->outputfile, "%16I64u %10u %6u %2u %16I64u %16I64u %10I64u\n", req->time, req->lsn, req->size, req->operation, start_time, end_time, end_time - req->time);
				//fflush(ssd->outputfile);

				if (end_time - start_time == 0)
				{
					printf("the response time is 0?? \n");
					getchar();
				}

				if (req->operation == READ)
				{
					ssd->read_request_count++;
					ssd->read_avg = ssd->read_avg + (end_time - req->time);
				}
				else
				{
					ssd->write_request_count++;
					ssd->write_avg = ssd->write_avg + (end_time - req->time);
				}

				while (req->subs != NULL)
				{
					tmp = req->subs;
					req->subs = tmp->next_subs;
					if (tmp->update != NULL)
					{
						free(tmp->update->location);
						tmp->update->location = NULL;
						free(tmp->update);
						tmp->update = NULL;
					}
					free(tmp->location);
					tmp->location = NULL;
					free(tmp);
					tmp = NULL;

				}

				if (pre_node == NULL)
				{
					if (req->next_node == NULL)
					{
						free(req->need_distr_flag);
						req->need_distr_flag = NULL;
						free(req);
						req = NULL;
						ssd->request_queue = NULL;
						ssd->request_tail = NULL;
						ssd->request_queue_length--;
					}
					else
					{
						ssd->request_queue = req->next_node;
						pre_node = req;
						req = req->next_node;
						free(pre_node->need_distr_flag);
						pre_node->need_distr_flag = NULL;
						free(pre_node);
						pre_node = NULL;
						ssd->request_queue_length--;
					}
				}
				else
				{
					if (req->next_node == NULL)
					{
						pre_node->next_node = NULL;
						free(req->need_distr_flag);
						req->need_distr_flag = NULL;
						free(req);
						req = NULL;
						ssd->request_tail = pre_node;
						ssd->request_queue_length--;
					}
					else
					{
						pre_node->next_node = req->next_node;
						free(req->need_distr_flag);
						req->need_distr_flag = NULL;
						free(req);
						req = pre_node->next_node;
						ssd->request_queue_length--;
					}

				}
			}
			else
			{
				pre_node = req;
				req = req->next_node;
			}
		}
	}
}


/*******************************************************************************
*statistic_output() output processing of a request after the relevant processing information
*1，Calculate the number of erasures per plane, ie plane_erase and the total number of erasures
*2，Print min_lsn, max_lsn, read_count, program_count and other statistics to the file outputfile.
*3，Print the same information into the file statisticfile
*******************************************************************************/
void statistic_output(struct ssd_info *ssd)
{
	unsigned int lpn_count=0,i,j,k,m,p,erase=0,plane_erase=0;
	unsigned int blk_read = 0, plane_read = 0;
	unsigned int blk_write = 0, plane_write = 0;
	unsigned int pre_plane_write = 0;
	double gc_energy=0.0;
#ifdef DEBUG
	printf("enter statistic_output,  current time:%I64u\n",ssd->current_time);
#endif

	for(i = 0;i<ssd->parameter->channel_number;i++)
	{
		for (p = 0; p < ssd->parameter->chip_channel[i]; p++)
		{
			for (j = 0; j < ssd->parameter->die_chip; j++)
			{
				for (k = 0; k < ssd->parameter->plane_die; k++)
				{
					for (m = 0; m < ssd->parameter->block_plane; m++)
					{
						if (ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].erase_count > 0)
						{
							ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_erase_count += ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].erase_count;
						}

						if (ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].page_read_count > 0)
						{
							ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_read_count += ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].page_read_count;
						}

						if (ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].page_write_count > 0)
						{
							ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_program_count += ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].page_write_count;
						}

						if (ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].pre_write_count > 0)
						{
							ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].pre_plane_write_count += ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].pre_write_count;
						}

						//fprintf(ssd->outputfile, "%d ", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].erase_count);
					}
					fprintf(ssd->outputfile, "the %d channel, %d chip, %d die, %d plane has : ", i, p, j, k);
					fprintf(ssd->outputfile, "%3d erase operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_erase_count);
					fprintf(ssd->outputfile, "%3d read operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_read_count);
					fprintf(ssd->outputfile, "%3d write operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_program_count);
					fprintf(ssd->outputfile, "%3d pre_process write operations\n", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].pre_plane_write_count);
					
					//fprintf(ssd->outputfile, "\n");
					
					fprintf(ssd->statisticfile, "the %d channel, %d chip, %d die, %d plane has : ", i, p, j, k);
					fprintf(ssd->statisticfile, "%3d erase operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_erase_count);
					fprintf(ssd->statisticfile, "%3d read operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_read_count);
					fprintf(ssd->statisticfile, "%3d write operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_program_count);
					fprintf(ssd->statisticfile, "%3d pre_process write operations\n", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].pre_plane_write_count);
				}
			}
		}
	}

	fprintf(ssd->outputfile,"\n");
	fprintf(ssd->outputfile,"\n");
	fprintf(ssd->outputfile,"---------------------------statistic data---------------------------\n");	 
	fprintf(ssd->outputfile,"min lsn: %13d\n",ssd->min_lsn);	
	fprintf(ssd->outputfile,"max lsn: %13d\n",ssd->max_lsn);
	fprintf(ssd->outputfile,"read count: %13d\n",ssd->read_count);	  
	fprintf(ssd->outputfile,"the read operation leaded by un-covered update count: %13d\n",ssd->update_read_count);
	fprintf(ssd->outputfile, "the read operation leaded by gc read count: %13d\n", ssd->gc_read_count);
	fprintf(ssd->outputfile, "\n");
	fprintf(ssd->outputfile, "program count: %13d\n", ssd->program_count);
	fprintf(ssd->outputfile, "the write operation leaded by pre_process write count: %13d\n", ssd->pre_all_write);
	fprintf(ssd->outputfile, "the write operation leaded by un-covered update count: %13d\n", ssd->update_write_count);
	fprintf(ssd->outputfile, "the write operation leaded by gc read count: %13d\n", ssd->gc_write_count);
	fprintf(ssd->outputfile, "\n");
	fprintf(ssd->outputfile,"erase count: %13d\n",ssd->erase_count);
	fprintf(ssd->outputfile,"direct erase count: %13d\n",ssd->direct_erase_count);
	fprintf(ssd->outputfile, "gc count: %13d\n", ssd->gc_count);

	fprintf(ssd->outputfile, "multi-plane program count: %13d\n", ssd->m_plane_prog_count);
	fprintf(ssd->outputfile, "multi-plane read count: %13d\n", ssd->m_plane_read_count);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "mutli plane one shot program count : %13d\n", ssd->mutliplane_oneshot_prog_count);
	fprintf(ssd->outputfile, "one shot program count : %13d\n", ssd->ontshot_prog_count);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "half page read count : %13d\n", ssd->half_page_read_count);
	fprintf(ssd->outputfile, "one shot read count : %13d\n", ssd->one_shot_read_count);
	fprintf(ssd->outputfile, "mutli plane one shot read count : %13d\n", ssd->one_shot_mutli_plane_count);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "erase suspend count : %13d\n", ssd->suspend_count);
	fprintf(ssd->outputfile, "erase resume  count : %13d\n", ssd->resume_count);
	fprintf(ssd->outputfile, "suspend read  count : %13d\n", ssd->suspend_read_count);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "update sub request count : %13d\n", ssd->update_sub_request);
	fprintf(ssd->outputfile,"write flash count: %13d\n",ssd->write_flash_count);
	fprintf(ssd->outputfile, "\n");
	
	fprintf(ssd->outputfile,"read request count: %13d\n",ssd->read_request_count);
	fprintf(ssd->outputfile,"write request count: %13d\n",ssd->write_request_count);
	fprintf(ssd->outputfile, "\n");
	fprintf(ssd->outputfile,"read request average size: %13f\n",ssd->ave_read_size);
	fprintf(ssd->outputfile,"write request average size: %13f\n",ssd->ave_write_size);
	fprintf(ssd->outputfile, "\n");
	if (ssd->read_request_count != 0)
		fprintf(ssd->outputfile, "read request average response time: %.3lf\n", (double)ssd->read_avg / ssd->read_request_count);
	if (ssd->write_request_count != 0)
		fprintf(ssd->outputfile, "write request average response time: %.3lf\n", (double)ssd->write_avg / ssd->write_request_count);
	fprintf(ssd->outputfile, "\n");
	fprintf(ssd->outputfile,"buffer read hits: %13d\n",ssd->dram->buffer->read_hit);
	fprintf(ssd->outputfile,"buffer read miss: %13d\n",ssd->dram->buffer->read_miss_hit);
	fprintf(ssd->outputfile,"buffer write hits: %13d\n",ssd->dram->buffer->write_hit);
	fprintf(ssd->outputfile,"buffer write miss: %13d\n",ssd->dram->buffer->write_miss_hit);
	
	fprintf(ssd->outputfile, "update sub request count : %13d\n", ssd->update_sub_request);
	fprintf(ssd->outputfile, "half page read count : %13d\n", ssd->half_page_read_count);
	fprintf(ssd->outputfile, "mutli plane one shot program count : %13d\n", ssd->mutliplane_oneshot_prog_count);
	fprintf(ssd->outputfile, "one shot read count : %13d\n", ssd->one_shot_read_count);
	fprintf(ssd->outputfile, "mutli plane one shot read count : %13d\n", ssd->one_shot_mutli_plane_count);
	fprintf(ssd->outputfile, "erase suspend count : %13d\n", ssd->suspend_count);
	fprintf(ssd->outputfile, "erase resume  count : %13d\n", ssd->resume_count);
	fprintf(ssd->outputfile, "suspend read  count : %13d\n", ssd->suspend_read_count);

	fprintf(ssd->outputfile, "\n");
	fflush(ssd->outputfile);

	fclose(ssd->outputfile);


	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile, "---------------------------statistic data---------------------------\n");
	fprintf(ssd->statisticfile, "min lsn: %13d\n", ssd->min_lsn);
	fprintf(ssd->statisticfile, "max lsn: %13d\n", ssd->max_lsn);
	fprintf(ssd->statisticfile, "read count: %13d\n", ssd->read_count);
	fprintf(ssd->statisticfile, "the read operation leaded by un-covered update count: %13d\n", ssd->update_read_count);
	fprintf(ssd->statisticfile, "the read operation leaded by gc read count: %13d\n", ssd->gc_read_count);
	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile, "program count: %13d\n", ssd->program_count);
	fprintf(ssd->statisticfile, "the write operation leaded by pre_process write count: %13d\n", ssd->pre_all_write);
	fprintf(ssd->statisticfile, "the write operation leaded by un-covered update count: %13d\n", ssd->update_write_count);
	fprintf(ssd->statisticfile, "the write operation leaded by gc read count: %13d\n", ssd->gc_write_count);
	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile,"erase count: %13d\n",ssd->erase_count);	  
	fprintf(ssd->statisticfile,"direct erase count: %13d\n",ssd->direct_erase_count);
	fprintf(ssd->statisticfile, "gc count: %13d\n", ssd->gc_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile,"multi-plane program count: %13d\n",ssd->m_plane_prog_count);
	fprintf(ssd->statisticfile,"multi-plane read count: %13d\n",ssd->m_plane_read_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "mutli plane one shot program count : %13d\n", ssd->mutliplane_oneshot_prog_count);
	fprintf(ssd->statisticfile, "one shot program count : %13d\n", ssd->ontshot_prog_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "half page read count : %13d\n", ssd->half_page_read_count);
	fprintf(ssd->statisticfile, "one shot read count : %13d\n", ssd->one_shot_read_count);
	fprintf(ssd->statisticfile, "mutli plane one shot read count : %13d\n", ssd->one_shot_mutli_plane_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "erase suspend count : %13d\n", ssd->suspend_count);
	fprintf(ssd->statisticfile, "erase resume  count : %13d\n", ssd->resume_count);
	fprintf(ssd->statisticfile, "suspend read  count : %13d\n", ssd->suspend_read_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "update sub request count : %13d\n", ssd->update_sub_request);
	fprintf(ssd->statisticfile,"write flash count: %13d\n",ssd->write_flash_count);
	fprintf(ssd->statisticfile, "\n");
	
	fprintf(ssd->statisticfile,"read request count: %13d\n",ssd->read_request_count);
	fprintf(ssd->statisticfile, "write request count: %13d\n", ssd->write_request_count);
	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile,"read request average size: %13f\n",ssd->ave_read_size);
	fprintf(ssd->statisticfile,"write request average size: %13f\n",ssd->ave_write_size);
	fprintf(ssd->statisticfile, "\n");
	if (ssd->read_request_count != 0)
		fprintf(ssd->statisticfile, "read request average response time: %.3lf\n",(double)ssd->read_avg / ssd->read_request_count);
	if (ssd->write_request_count != 0)
		fprintf(ssd->statisticfile, "write request average response time: %.3lf\n", (double)ssd->write_avg / ssd->write_request_count);
	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile,"buffer read hits: %13d\n",ssd->dram->buffer->read_hit);
	fprintf(ssd->statisticfile,"buffer read miss: %13d\n",ssd->dram->buffer->read_miss_hit);
	fprintf(ssd->statisticfile,"buffer write hits: %13d\n",ssd->dram->buffer->write_hit);
	fprintf(ssd->statisticfile,"buffer write miss: %13d\n",ssd->dram->buffer->write_miss_hit);
	fprintf(ssd->statisticfile, "\n");
	fflush(ssd->statisticfile);

	fclose(ssd->statisticfile);
}




/***********************************************
*free_all_node(): release all applied nodes
************************************************/
void free_all_node(struct ssd_info *ssd)
{
	unsigned int i,j,k,l,n,p;
	struct buffer_group *pt=NULL;
	struct direct_erase * erase_node=NULL;

//	struct gc_operation *gc_node = NULL;


	for (i=0;i<ssd->parameter->channel_number;i++)
	{
		for (j=0;j<ssd->parameter->chip_channel[0];j++)
		{
			for (k=0;k<ssd->parameter->die_chip;k++)
			{
				for (l=0;l<ssd->parameter->plane_die;l++)
				{
					for (n=0;n<ssd->parameter->block_plane;n++)
					{
						free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head);
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head=NULL;
					}
					free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head);
					ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head=NULL;
					while(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node!=NULL)
					{
						erase_node=ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node;
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node=erase_node->next_node;
						free(erase_node);
						erase_node=NULL;
					}
				}
				
				free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head);
				ssd->channel_head[i].chip_head[j].die_head[k].plane_head=NULL;
			}
			free(ssd->channel_head[i].chip_head[j].die_head);
			ssd->channel_head[i].chip_head[j].die_head=NULL;
		}
		free(ssd->channel_head[i].chip_head);
		ssd->channel_head[i].chip_head=NULL;

		//free掉没有执行的gc_node
		/*
		while (ssd->channel_head[i].gc_command != NULL)
		{
			gc_node = ssd->channel_head[i].gc_command;
			ssd->channel_head[i].gc_command = gc_node->next_node;
			free(gc_node);
			gc_node = NULL;
		}*/
	}
	free(ssd->channel_head);
	ssd->channel_head=NULL;

	avlTreeDestroy( ssd->dram->buffer);
	ssd->dram->buffer=NULL;
	avlTreeDestroy(ssd->dram->command_buffer);
	ssd->dram->command_buffer = NULL;
	
	for (p = 0; p < DIE_NUMBER; p++)
	{
		avlTreeDestroy(ssd->dram->static_die_buffer[p]);
		ssd->dram->static_die_buffer[p] = NULL;
	}

	free(ssd->dram->map->map_entry);
	ssd->dram->map->map_entry=NULL;
	free(ssd->dram->map);
	ssd->dram->map=NULL;
	free(ssd->dram);
	ssd->dram=NULL;
	free(ssd->parameter);
	ssd->parameter=NULL;

	free(ssd);
	ssd=NULL;
}

// warm_flash: 
//	1. for Read operations, data read by read op need be filled at first, i.e., convert Read op into Write op. But, Write ops in trace aren't exec.
//	2. running trace for make ssd aged , in order to test its steady performance.
//	In a word, warm_flash = pre_process_page + make_aged, warm_flash is used by flashsim while pre_xx + make_aged is used by ssdsim.
struct ssd_info *warm_flash(struct ssd_info *ssd)
{
	int flag = 1;
	double output_step = 0;
	unsigned int a = 0, b = 0;
	errno_t err;
	unsigned int i, j, k, m, p;

	//判断配置文件是否支持warm_flash
	if (ssd->parameter->warm_flash != 1){
		ssd->warm_flash_cmplt = 1;
		return ssd;
	}
	
	ssd->warm_flash_cmplt = 0;

	printf("\n");
	printf("begin warm_flash.......................\n");
	printf("\n");

	//Empty the allocation token in pre_process()
	ssd->token = 0;
	for (i = 0; i < ssd->parameter->channel_number; i++)
	{
		for (j = 0; j < ssd->parameter->chip_channel[i]; j++)
		{
			for (k = 0; k < ssd->parameter->die_chip; k++)
			{
				ssd->channel_head[i].chip_head[j].die_head[k].token = 0;
			}
			ssd->channel_head[i].chip_head[j].token = 0;
		}
		ssd->channel_head[i].token = 0;
	}

	if ((err = fopen_s(&(ssd->tracefile), ssd->tracefilename, "r")) != 0)
	{
		printf("the trace file can't open\n");
		return NULL;
	}

	while (flag != 100)
	{
		/*interface layer*/
		flag = get_requests(ssd); // -1: buffer full or flash not finish yet  0: request block(request queue length too long) or trace is over  1: normal, read next trace 

		/*buffer layer*/
		if (flag == 1 || (flag == 0 && ssd->request_work != NULL))
		{
			if (flag == 0 && ssd->request_work != NULL)
				getchar();
			if (ssd->parameter->dram_capacity != 0)
			{
				if (ssd->buffer_full_flag == 0)				//buffer don't block,it can be handle.
				{
					buffer_management(ssd);
				}
			}
			else
			{
				no_buffer_distribute(ssd);
			}

			if (ssd->request_work->cmplt_flag == 1)
			{
				if (ssd->request_work != ssd->request_tail)
					ssd->request_work = ssd->request_work->next_node;
				else
					ssd->request_work = NULL;
			}

		}

		/*ftl+fcl+flash layer*/
		process(ssd);

		trace_output(ssd);

		if (flag == 0 && ssd->request_queue == NULL)
			flag = 100;
	}
	fclose(ssd->tracefile);

	if (ssd->dram->buffer->buffer_sector_count != 0)
		flush_all(ssd);
	while (ssd->request_queue != NULL)
	{
		ssd->current_time = find_nearest_event(ssd);
		process(ssd);
		trace_output(ssd);
	}
	ssd->warm_flash_cmplt = 1;

	//清空warm计数参数
	// shouldn't clear up request cnt
	initialize_statistic(ssd);
	ssd->request_work = NULL;
	ssd->dram->buffer->write_hit = 0;
	ssd->dram->buffer->write_miss_hit = 0;
	for (i = 0; i < ssd->parameter->channel_number; i++)
	{
		for (j = 0; j < ssd->parameter->chip_channel[i]; j++)
		{
			ssd->channel_head[i].chip_head[j].current_time = 0;
			ssd->channel_head[i].chip_head[j].next_state_predict_time = 0;
			ssd->channel_head[i].chip_head[j].current_state = 100;
			ssd->channel_head[i].chip_head[j].next_state = 100;
			for (k = 0; k < ssd->parameter->die_chip; k++)
			{
				for (m = 0; m < ssd->parameter->plane_die; m++)
				{
					for (p = 0; p < ssd->parameter->block_plane; p++)
					{
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[p].erase_count = 0;
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[p].page_read_count = 0;
						if (ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[p].page_write_count > 0)
							ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].pre_plane_write_count += ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[p].page_write_count;
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[p].page_write_count = 0;
					}
					ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].plane_erase_count = 0;
					ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].test_gc_count = 0;
				}
			}
		}
		ssd->channel_head[i].current_time = 0;
		ssd->channel_head[i].next_state_predict_time = 0;
		ssd->channel_head[i].current_state = 0;
		ssd->channel_head[i].next_state = 0;
	}

	printf("warmflash is completed!\n");
	FILE *f = fopen("bsh.log", "w");
	fprintf(f, "warmflash is complt!\n");
	fclose(f);
	return ssd;
}

/************************************************
*Assert,open failed，printf“open ... error”
*************************************************/
void file_assert(int error, char *s)
{
	if (error == 0) return;
	printf("open %s error\n", s);
	getchar();
	exit(-1);
}

/*****************************************************
*Assert,malloc failed，printf“malloc ... error”
******************************************************/
void alloc_assert(void *p, char *s)
{
	if (p != NULL) return;
	printf("malloc %s error\n", s);
	getchar();
	exit(-1);
}

/*********************************************************************************
*Assert
*A，trace about time_t，device，lsn，size，ope <0 ，printf“trace error:.....”
*B，trace about time_t，device，lsn，size，ope =0，printf“probable read a blank line”
**********************************************************************************/
void trace_assert(_int64 time_t, int device, unsigned int lsn, int size, int ope)
{
	if (time_t <0 || device < 0 || lsn < 0 || size < 0 || ope < 0)
	{
		printf("trace error:%I64u %d %d %d %d\n", time_t, device, lsn, size, ope);
		getchar();
		exit(-1);
	}
	if (time_t == 0 && device == 0 && lsn == 0 && size == 0 && ope == 0)
	{
		printf("probable read a blank line\n");
		getchar();
	}
}

/*
* ----------------------codes below for debugging-------------------------------------
*/
// display current time when process run in position where it's called.
void print_current_times(struct ssd_info *ssd)
{
	char *wday[] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
	time_t timep;
	struct tm *p;
	time(&timep);
	p = localtime(&timep);
	char buff[100];
	snprintf(buff, sizeof(buff), "%d-%02d-%02d %s %02d:%02d:%02d",
		(1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday,
		wday[p->tm_wday], p->tm_hour, p->tm_min, p->tm_sec);
	fprintf(ssd->bshfile, "\n-------------------------recording-------------------\n");
	fprintf(ssd->bshfile, "\nCurrent time : %s\n\n", buff);
	printf("\nCurrent time : %s\n", buff);
}

void print_free_page_nums(struct ssd_info * ssd) {
	int i, m, j, k, p;
	//int total_free_pages = 0;
	int total_fail_pages = 0;
	int total_pages;
	int total_block_free_pages = 0;
	for (i = 0; i<ssd->parameter->channel_number; i++)
	{
		for (m = 0; m < ssd->parameter->chip_channel[i]; m++)
		{
			for (j = 0; j < ssd->parameter->die_chip; j++)
			{
				for (k = 0; k < ssd->parameter->plane_die; k++)
				{
					int plane_free_pages = 0;
					for (p = 0; p < ssd->parameter->block_plane; p++)
					{
						//printf("%d,%d,%d,%d,%d:  %5d\n", i, m, j, k, p,ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].last_write_page);
						plane_free_pages += ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].free_page_num;
					}
					//fprintf(ssd->bshfile, "channel: %d, chip: %d, die: %d, plane: %d, free_page: %5d\n", 
						//i, m, j, k, ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].free_page);
					fprintf(ssd->bshfile, "channel: %d, chip: %d, die: %d, plane: %d, free_pages: %5d\n",
						 i, m, j, k, plane_free_pages);
					//total_free_pages += ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].free_page;
					total_fail_pages += ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].fail_pages;
					total_block_free_pages += plane_free_pages;
				}
			}
		}
	}
	total_pages = ssd->parameter->channel_number*ssd->parameter->chip_channel[0] * ssd->parameter->die_chip*ssd->parameter->plane_die*ssd->parameter->block_plane*ssd->parameter->page_block;
	fprintf(ssd->bshfile, "total_pages = %d, fail_pages_ratio = %lf, total_block_free_pages = %d, block_free_pages ratio = %lf\n", 
		total_pages, 
		(double)total_fail_pages / total_pages,
		total_block_free_pages,
		(double)total_block_free_pages / total_pages
		);
	printf("total_pages = %d, fail_pages_ratio = %lf, total_block_free_pages = %d, block_free_pages ratio = %lf\n",
		total_pages,
		(double)total_fail_pages / total_pages,
		total_block_free_pages,
		(double)total_block_free_pages / total_pages
	);
#if 0
	print_all_level_pages_status(ssd);
	print_all_pages_valid_free_fail(ssd);
#endif
	fflush(ssd->bshfile);

	print_current_times(ssd);
}


void print_all_level_pages_status(struct ssd_info * ssd) {
	int i, j, k, m, n;
	int layer = 0;
	int page = 0;
	for (i = 0; i < ssd->parameter->channel_number; i++) {
		for (j = 0; j < ssd->parameter->chip_channel[i]; j++) {
			for (k = 0; k < ssd->parameter->die_chip; k++) {
				for (m = 0; m < ssd->parameter->plane_die; m++) {
					for (n = 0; n < ssd->parameter->block_plane; n++) {
						for (layer = 0; layer < 48; layer++) {
							fprintf(ssd->bshfile, "channel = %d, chip = %d, die = %d, plane = %d, block = %3d, layer = %2d, pages = ",
								i, j, k, m, n, layer);
							for (page = 0; page < 12; page++) {
								fprintf(ssd->bshfile, "%d, ", ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[n].page_head[layer * 12 + page].fail);
							}
							fprintf(ssd->bshfile, "\n");
							fflush(ssd->bshfile);
						}
					}
				}
			}
		}
	}
}

void print_all_pages_valid_free_fail(struct ssd_info * ssd) {
	int i, j, k, m, n;
	int layer = 0;
	int page = 0;
	int fail_cnt = 0, free_cnt = 0, valid_cnt = 0, invalid_cnt = 0;
	int t_fail_cnt = 0, t_free_cnt = 0, t_valid_cnt = 0, t_invalid_cnt = 0;
	struct page_info tmppage;
	FILE * tfile_ = fopen("print_free_valid.txt", "w");
	for (i = 0; i < ssd->parameter->channel_number; i++) {
		for (j = 0; j < ssd->parameter->chip_channel[i]; j++) {
			for (k = 0; k < ssd->parameter->die_chip; k++) {
				for (m = 0; m < ssd->parameter->plane_die; m++) {
					for (n = 0; n < ssd->parameter->block_plane; n++) {
						for (layer = 0; layer < 48; layer++) {
							fprintf(ssd->bshfile, "channel = %d, chip = %d, die = %d, plane = %d, block = %3d, layer = %2d, pages = ",
								i, j, k, m, n, layer);
							fail_cnt = 0, free_cnt = 0, valid_cnt = 0, invalid_cnt = 0;
							for (page = 0; page < 12; page++) {
								tmppage = ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[n].page_head[layer * 12 + page];
								if (tmppage.fail = 1) {
									fprintf(ssd->bshfile, "F, ");
									fail_cnt++;
									t_fail_cnt++;
								}
								if (tmppage.free_state == 1) {
									fprintf(ssd->bshfile, "R, ");
									free_cnt++;
									t_free_cnt++;
								}
								else {
									if (tmppage.valid_state & 0xffff == 0) {
										fprintf(ssd->bshfile, "I, ");
										invalid_cnt++;
										t_invalid_cnt++;
									}
									else {
										fprintf(ssd->bshfile, "V, ");
										valid_cnt++;
										t_valid_cnt++;
									}
								}								
								fprintf(tfile_, "valid_state = %2d, free_state = %2d, fail = %d\n", tmppage.valid_state, tmppage.free_state, tmppage.fail);
								fflush(tfile_);
							}
							fprintf(ssd->bshfile, " fail_cnt = %2d, free_cnt = %2d, invalid_cnt = %2d, valid_cnt = %2d,", fail_cnt, free_cnt, invalid_cnt, valid_cnt);
							fprintf(ssd->bshfile, "\n");
							fflush(ssd->bshfile);
						}
					}
				}
			}
		}
	}
	fclose(tfile_);
	fprintf(ssd->bshfile, "t_fail_cnt = %4d, t_free_cnt = %4d, t_invalid_cnt = %4d, t_valid_cnt = %4d,", t_fail_cnt, t_free_cnt, t_invalid_cnt, t_valid_cnt);
	fflush(ssd->bshfile);
}

// print plane status, i.e., planes free_page, each block in each plane last_write_page
void print_cur_plane_status(struct ssd_info * ssd) {
	//After the preprocessing is complete, the page offset address of each plane should be guaranteed to be consistent
	for (int i = 0; i<ssd->parameter->channel_number; i++)
	{
		for (int m = 0; m < ssd->parameter->chip_channel[i]; m++)
		{
			for (int j = 0; j < ssd->parameter->die_chip; j++)
			{
				for (int k = 0; k < ssd->parameter->plane_die; k++)
				{
					for (int p = 0; p < ssd->parameter->block_plane; p++)
					{
						fprintf(ssd->bshfile, "%d, %d, %d, %d, %3d:  %5d %2d\n", i, m, j, k, p, ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].last_write_page,
							ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].erase_count);
					}
				
					printf("free_page: %d,%d,%d,%d:  %5d\n", i, m, j, k, ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].free_page);
					fprintf(ssd->bshfile, "free_page: %d ,%d, %d, %d:  %5d\n", i, m, j, k, ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].free_page);
				}
			}
		}
	}
}

// compare whether two vals are equal
Status check_corrections(int a, int b, const char * s) {
	if (a != b) {
		printf("Error, %d, %d, %s\n", a, b, s);
		return FAILURE;
	}
	return SUCCESS;
}

// just check correction, check for each block 
// if vals aren't equal, return FAILURE, otherwise return SUCCESS
Status check_block_free_page_issue(struct ssd_info * ssd, int channel, int chip, int die, int plane, int block, int failed_page) {
	int valid_pages_cnt_ = 0, invalid_pages_cnt_ = 0, free_pages_cnt_ = 0, fail_pages_cnt_ = 0;
	struct blk_info tmpblk = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block];
	for (int i = 0; i < ssd->parameter->page_block; i++) {
		if (tmpblk.page_head[i].free_state == 1) free_pages_cnt_++;
		else {
			if (tmpblk.page_head[i].valid_state != 0) valid_pages_cnt_++;
			else {
				if (tmpblk.page_head[i].fail == 0) invalid_pages_cnt_++;
				else fail_pages_cnt_++;
			}
		}
	}
	int a1 = check_corrections(failed_page, fail_pages_cnt_, "fail");
	int a2 = check_corrections(tmpblk.invalid_page_num, invalid_pages_cnt_, "invalid");
	int a3 = check_corrections(tmpblk.free_page_num, free_pages_cnt_, "free");
	int a4 = check_corrections(free_pages_cnt_ + valid_pages_cnt_ + invalid_pages_cnt_ + fail_pages_cnt_, ssd->parameter->page_block, "page_block");
	int a5 = check_corrections(tmpblk.last_write_page, ssd->parameter->page_block - free_pages_cnt_ - 1, "last_write_page");
	return !(a1 == FAILURE || a2 == FAILURE || a3 == FAILURE || a4 == FAILURE);
}

Status check_plane_free_page_issue(struct ssd_info * ssd, int channel, int chip, int die, int plane) {
	int check_free_pages_cnt = 0;
	struct plane_info pln = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane];
	for (int i = 0; i < ssd->parameter->block_plane; i++) {
		if (check_block_free_page_issue(ssd, channel, chip, die, plane, i, 0) == FAILURE) return FAILURE;
		for (int j = 0; j < ssd->parameter->page_block; j++) {
			if (pln.blk_head[i].page_head[j].fail == 0 && pln.blk_head[i].page_head[j].free_state == 1) { // healthy
				check_free_pages_cnt++;
			}
		}
	}
	int a5 = check_corrections(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page, check_free_pages_cnt, "plane free_page");
	return !(a5 == FAILURE);
}

void check_warmflash_corrections(struct ssd_info *ssd) {
	printf("\nchecking warmflash......\n");
	for (int i = 0; i < ssd->parameter->channel_number; i++)
	{
		for (int m = 0; m < ssd->parameter->chip_channel[i]; m++)
		{
			for (int j = 0; j < ssd->parameter->die_chip; j++)
			{
				for (int k = 0; k < ssd->parameter->plane_die; k++)
				{
					if (check_plane_free_page_issue(ssd, i, m, j, k) == FAILURE) {
						printf("Error, warm flash free pages\n");
						while (1) {}
					}
				}
			}
		}
	}
}