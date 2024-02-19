#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include "ts_demuxer.h"
#include "ts_demuxer_priv.h"
#include "avc_hevc_analyse.h"

#ifdef __cplusplus
extern "C" {
#endif 

typedef enum
{
    DMUX_STREAM_ID_VIDEO = 0,
    DMUX_STREAM_ID_AUDIO,
	DMUX_STREAM_ID_BUTT
} demuxer_stream_id_e;


static struct ts_shareinfo *gListshareInfo;
static pthread_mutex_t mutexSharelist = PTHREAD_MUTEX_INITIALIZER;

/**
* @brief: 初始化ts解封装上下文
*
* @filename[in]: ts文件
*
* @return: ts解封装句柄
*
* @note.返回NULL为失败
**/
Handle TS_Open(char *file)
{
	int ret;
	ts_demux_ctx_s* pCtx = (ts_demux_ctx_s *)malloc(sizeof(ts_demux_ctx_s));
	struct ts_shareinfo *pshareinfo = NULL;
	char sync_byte[2] = {0};

	memset(pCtx, 0, sizeof(ts_demux_ctx_s));
	
	pCtx->fp = fopen(file, "r");
	if (!pCtx->fp)
	{
		TS_LOG_ERR("%s open failed!errno:%d %s\n", file, errno, strerror(errno));
		goto ERR;
	}

	/* check file is ts ? */
	fread(&sync_byte[0], 1, 1, pCtx->fp);
	fseek(pCtx->fp, 188, SEEK_SET);
	fread(&sync_byte[1], 1, 1, pCtx->fp);
	if (sync_byte[0] != 0x47 || sync_byte[1] != 0x47)
	{//同步码=0x47，ts包长度固定=188，检测连续两个包释放有同步码
		TS_LOG_ERR("%s isn't ts!\n", file);
		goto ERR;
	}
	fseek(pCtx->fp, 0, SEEK_SET);

	pCtx->name = (char *)malloc(strlen(file)+1);
	strncpy(pCtx->name, file, strlen(file)+1);

	pCtx->cache_size = CACHE_PS_PIECE_SIZE;
	pCtx->pcache	 = (uint8_t *)malloc(CACHE_PS_PIECE_SIZE);

	pCtx->vidRawbuffsize = VIDEO_RAWDATA_BUFF_SIZE;
	pCtx->vidRawbuff = (uint8_t *)malloc(VIDEO_RAWDATA_BUFF_SIZE);

	pthread_mutex_lock(&mutexSharelist);
	if (0 == ts_ref_sharelist(pCtx->name, &pshareinfo) && pshareinfo) //判断是否已经打开过
	{
		pthread_mutex_unlock(&mutexSharelist);
		while(!pshareinfo->active)	 //已经打开文件，但是还没有解析完成
			usleep(100000);
		pCtx->codec = pshareinfo->codec;
		pCtx->fps	= pshareinfo->fps;
		pCtx->width = pshareinfo->width;
		pCtx->height = pshareinfo->height;
		pCtx->duration_time = pshareinfo->duration_time;
		pCtx->start_time	= pshareinfo->start_time;
		pCtx->end_time		= pshareinfo->end_time;
	}
	else
	{
		ts_new_sharelist(file);	//未打开过则将文件添加到共享信息List，之后分析文件，将信息填入List
		pthread_mutex_unlock(&mutexSharelist);
		
		//Get PS info start
		uint8_t *frame = NULL;
		int 	 framelen;
		long	 curPos;
		if (TS_EFAIL == TS_ReadFrame(pCtx, (void **)&frame, &framelen))
		{
			TS_LOG_ERR("%s read frame failed!\n", pCtx->name);
			goto ERR;
		}
		TS_ReleaseFrame(pCtx, frame);
		pCtx->start_time = pCtx->lastvideoTimestamp;

		fseek(pCtx->fp, -(CACHE_PS_PIECE_SIZE), SEEK_END);
		curPos = ftell(pCtx->fp);
		pCtx->cache_data_len  = 0;
		pCtx->cache_pos 	  = 0;
		pCtx->lastTimestamp   = 0;
		pCtx->lastvideoTimestamp = 0;
		pCtx->last_continuity_counter = TS_INVALID_CONTINUE_COUNT;
		pCtx->vidRawbuffpos   = 0;
		pCtx->curopt		  = TS_SEARCH_END;
		while(1)
		{
			ret = TS_ReadFrame(pCtx, (void **)&frame, &framelen);
			if (TS_SOK != ret)
			{
				if (TS_EOF == ret && pCtx->end_time == 0)
				{//从后往前找，直到找到有效的时间戳
					pCtx->cache_data_len  = 0;
					pCtx->cache_pos 	  = 0;
					curPos -= CACHE_PS_PIECE_SIZE;
					if (curPos < 0)
						break;
					fseek(pCtx->fp, curPos, SEEK_SET);
					continue;
				}
				pCtx->curopt	  = TS_SEARCH_NONE;
				break;
			}
			pCtx->end_time = pCtx->lastvideoTimestamp;
			TS_ReleaseFrame(pCtx, frame);
		}

		if (pCtx->end_time < pCtx->start_time)
		{
			TS_LOG_ERR("%s end_time maybe overflow (%lu < %lu), try to repair!\n", pCtx->name, pCtx->end_time, pCtx->start_time);
			if (pCtx->start_time > 0xFFFFFFFF - TS_DURATION_OVERFLOW_THD)
			{//开始时间戳溢出1h以内，认为是时间戳溢出的异常
				TS_LOG_WARN("%s pst overflow try to repair!\n", pCtx->name);
				pCtx->end_time += (0xFFFFFFFF / 90);
			}
			else{//否则通过累积时间戳计算
				TS_LOG_WARN("%s pst overflow, use time_mode1!\n", pCtx->name);
				pCtx->timestamp_mode = 1;
				pCtx->vidRawbuffpos   = 0;
				TS_SetTimePosition((Handle *)pCtx);
			}

		}
		pCtx->duration_time = pCtx->end_time - pCtx->start_time;
		
		ts_set_sharelist(pCtx);//设置共享文件List信息
	}
	
	//Get PS info end
	pCtx->vidRawbuffpos   = 0;
	pCtx->last_continuity_counter = TS_INVALID_CONTINUE_COUNT;
	pCtx->corrTimestamp  = 0;
	pCtx->accumulateTime = 0;
	pCtx->lastTimestamp  = 0;
	pCtx->lastvideoTimestamp = 0;
	pCtx->curTimestamp	 = 0;
	pCtx->cache_data_len = 0;
	pCtx->cache_pos 	 = 0;	
	pCtx->isEof 		 = 0;
	fseek(pCtx->fp, 0, SEEK_SET);

	TS_LOG_INFO("%s open success![%d * %d * %d] during:%lu start:%lu end:%lu\n", pCtx->name, pCtx->width, pCtx->height, pCtx->fps, pCtx->duration_time,
												pCtx->start_time, pCtx->end_time);
	return pCtx;

ERR:
	if (pCtx->fp)
		fclose(pCtx->fp);
	if (pCtx->pcache)
		free(pCtx->pcache);
	if (pCtx->vidRawbuff)
		free(pCtx->vidRawbuff);
	
	free(pCtx);

	return NULL;
}

/**
* @brief: 销毁ts解封装上下文
*
* @Handle[in]: ts句柄
*
* @return: succes 0 / fail -1
*
* @note.
**/
int TS_Close(Handle hdl)
{
	ts_demux_ctx_s *pCtx = (ts_demux_ctx_s *)hdl;
	fclose(pCtx->fp);
	pthread_mutex_lock(&mutexSharelist);
	ts_del_sharelist(pCtx->name);
	pthread_mutex_unlock(&mutexSharelist);
	free(pCtx->vidRawbuff);
	free(pCtx->pcache);
	free(pCtx->name);
	free(pCtx);
	
	return TS_SOK;
}

static int TS_CodectypeConvert(int codec_id)
{
	switch(codec_id)
	{
		case TS_STREAM_TYPE_V_H264:
			return DMUX_VIDEO_TYPE_H264;
		case TS_STREAM_TYPE_V_H265:
			return DMUX_VIDEO_TYPE_H265;

		default:
			TS_LOG_WARN("codec id:%d not support sdk\n",codec_id);
			return DMUX_VIDEO_TYPE_INVALID;		
	}	
}

/**
* @brief: 获取ts码流信息
*
* @Handle[in]: ts句柄
* @fileinfo[out]: 码流信息
*
* @return: succes 0 / fail -1
*
* @note.
**/
int TS_GetFileInfo(Handle hdl, ts_file_info_s *fileinfo)
{
	ts_demux_ctx_s *pCtx = (ts_demux_ctx_s *)hdl;
	
	memset(fileinfo, 0, sizeof(ts_file_info_s));

	fileinfo->start_time = pCtx->start_time; // ms
	fileinfo->end_time = pCtx->end_time; // ms
	fileinfo->streams[TS_STREAMID_VIDEO].enType = 1;
	fileinfo->streams[TS_STREAMID_VIDEO].codecType = TS_CodectypeConvert(pCtx->codec);
	fileinfo->streams[TS_STREAMID_VIDEO].info.video.width = pCtx->width;
	fileinfo->streams[TS_STREAMID_VIDEO].info.video.height = pCtx->height;
	fileinfo->streams[TS_STREAMID_VIDEO].info.video.fps = pCtx->fps>0?pCtx->fps:25;

	return TS_SOK;
}

static int update_cache_buffer(ts_demux_ctx_s *pCtx)
{
	uint32_t datalen;

	if (pCtx->curopt == TS_SEARCH_END && pCtx->cache_data_len != 0)  //寻尾操作一次向前读一片cache
	{
		TS_LOG_WARN("%s OPT_F_END current piece invalid!pos:%d\n", pCtx->name, pCtx->cache_pos);
		return TS_EOF;
	}
	if (pCtx->cache_pos < pCtx->cache_data_len)
	{//移动前一分片剩余数据
		memmove(pCtx->pcache, pCtx->pcache+pCtx->cache_pos, pCtx->cache_data_len-pCtx->cache_pos);
	}
	
	pCtx->cache_data_len = pCtx->cache_data_len-pCtx->cache_pos;
	datalen = fread(pCtx->pcache+pCtx->cache_data_len, 1, pCtx->cache_size-pCtx->cache_data_len, pCtx->fp);
	if (datalen == 0)
	{
		pCtx->isEof = 1;
		return TS_EOF;
	}

	pCtx->cache_pos = 0;
	pCtx->cache_data_len += datalen;
	

	return TS_SOK;
}


/**
* @brief: 读取一帧数据
*
* @Handle[in]: ts句柄
* @frmdata[out]: 帧数据
* @frmlen[out]: 帧长度
*
* @return: STREAM_OK 0 / STREAM_FAIL -1
*
* @note.STREAM_EOF文件读结束;配套TS_ReleaseFrame使用
**/
int TS_ReadFrame(Handle hdl,void **frmdata, int *frmlen)
{
	/*TS流结构：
	* TS_header + adaptation_field + playload
	* playload: PES header + ES 或只有 ES，只有首个包有PES header 
	*
	* 解复用流程：
	* 1.找到PAT，获取PMT的PID;
	* 2.找到PMT，获取视频流的PID;
	* 3.获取视频流数据;
	* 4.组包
	*
	*/
	ts_demux_ctx_s *pCtx = (ts_demux_ctx_s *)hdl;

	if (pCtx->isEof)
	{//EOF直接返回
		TS_LOG_ERR("%s is read eof!\n", pCtx->name);
		return TS_EOF;
	}

	/* 1、Parse TS Header */
	uint8_t *pdata = NULL;
	uint32_t offset = 0;
	uint8_t findTsflg = 0;
	
	while(1){
		if ((pCtx->cache_data_len-pCtx->cache_pos < TS_PACKET_FIXED_LEN) && update_cache_buffer(pCtx) != TS_SOK)
		{//数据不足一个ts pack返回
			TS_LOG_INFO("%s is read eof!\n", pCtx->name);
			return TS_EOF;
		}	

		for(offset = pCtx->cache_pos; offset < pCtx->cache_data_len-4; offset++)
		{
			pdata = pCtx->pcache + offset;
			if (pdata[0] == 0x47)
			{
				findTsflg = 1;
				break;
			}
		}	
		pCtx->cache_pos = offset;  //定位到TS头
		offset = 0;
		if (findTsflg == 0 || pdata == NULL || pCtx->cache_data_len < offset + 188) //剩余数据少于1个Transport Packet
		{
			continue;
		}	
		struct transport_packet ts_header;
		{//read ts head
			ts_header.sync_byte = pdata[0];
			ts_header.ts_err_indicator = (pdata[1] >> 7)&0x1;
			ts_header.payload_unit_start_indicator = (pdata[1] >> 6)&0x1;
			ts_header.ts_priority      = (pdata[1] >> 5)&0x1;
			ts_header.PID              = (((uint16_t)pdata[1] & 0x1F) << 8)|pdata[2];
			ts_header.ts_scrambling_control   = (pdata[3] >> 6)&0x3;
			ts_header.adaptation_field_contro = (pdata[3] >> 4)&0x3;
			ts_header.continuity_counter      = (pdata[3]&0xF);
			offset += 4;
		}

		if (pCtx->PATflg == 0)
		{//需要先找到解析PAT
			if (ts_header.PID != 0)
			{
				pCtx->cache_pos += TS_PACKET_FIXED_LEN;
				continue;
			}
			uint32_t pat_offset = offset;
			uint8_t adapation_field_len = pdata[pat_offset++];
			//跳过adaption_field
			pat_offset += adapation_field_len;

			//program_association_section
			struct program_association_section pat;
			pat.table_id = pdata[pat_offset++];
			pat.section_syntax_indicator = (pdata[pat_offset] >> 7)&0x01;
			pat.zero = (pdata[pat_offset] >> 6)&0x01;
			pat.section_length = (((uint16_t)pdata[pat_offset]&0x0F)<<8)|pdata[pat_offset+1];
			pat_offset += 2;			
			pat.transport_stream_id = (((uint16_t)pdata[pat_offset])<<8)|pdata[pat_offset+1];
			pat_offset += 2;
			pat.version_number = (pdata[pat_offset] >> 1)&0x1F;
			pat.current_next_indicator = pdata[pat_offset++]&0x01;
			pat.section_number = pdata[pat_offset++];
			pat.last_section_number = pdata[pat_offset++];
			printf("PAT section_number[%d~%d]\n", pat.section_number, pat.last_section_number);
			for(int i=0, j=0; i<pat.section_length - 9; i+=sizeof(struct pat_program), j++){
				struct pat_program pat_p;
				pat_p.program_number =(((uint16_t)pdata[pat_offset])<<8)|pdata[pat_offset+1];
				pat_p.program_map_PID =(((uint16_t)pdata[pat_offset+2]&0x1F)<<8)|pdata[pat_offset+3];
				if (pat_p.program_number == 0)
				{
					;//network_PID
				}else{
					//program_map_PID
					pCtx->PAT_Program[j] = (struct pat_program *)malloc(sizeof(struct pat_program));
					memcpy(pCtx->PAT_Program[j], &pat_p, sizeof(struct pat_program));
					printf(" - PAT program:%d PID:%d\n", pCtx->PAT_Program[j]->program_number, pCtx->PAT_Program[j]->program_map_PID);
					
				}
				pat_offset += sizeof(struct pat_program);
			}

			pCtx->cache_pos += TS_PACKET_FIXED_LEN;
			pCtx->PATflg = 1;
			continue;
		}

		if (pCtx->PATflg && !pCtx->PMTflg)
		{//PMT
			uint32_t pmt_offset = offset;
			uint8_t adapation_field_len = pdata[pmt_offset++];
			//跳过adaption_field
			pmt_offset += adapation_field_len;
			for(int i=0; pCtx->PAT_Program[i] != NULL; i++){
				if (pCtx->PAT_Program[i]->program_map_PID == ts_header.PID) //当前TS包是否为PMT信息
				{
					struct TS_program_map_section pmt;
					pmt.table_id = pdata[pmt_offset++];
					pmt.section_syntax_indicator = (pdata[pmt_offset]>>7)&0x01;
					pmt.zero = (pdata[pmt_offset]>>6)&0x01;
					pmt.section_length =(((uint16_t)pdata[pmt_offset]&0x0F)<<8)|pdata[pmt_offset+1];
					pmt_offset += 2;
					pmt.program_number = (((uint16_t)pdata[pmt_offset])<<8)|pdata[pmt_offset+1];
					pmt_offset += 2;
					pmt.version_number = (pdata[pmt_offset]>>1)&0x1F;
					pmt.current_next_indicator = (pdata[pmt_offset++])&0x01;
					pmt.section_number = pdata[pmt_offset++];
					pmt.last_section_number = pdata[pmt_offset++];
					pmt.PCR_PID = (((uint16_t)pdata[pmt_offset]&0x1F)<<8)|pdata[pmt_offset+1];
					pmt_offset += 2;
					pmt.program_info_length = (((uint16_t)pdata[pmt_offset]&0x0F)<<8)|pdata[pmt_offset+1];
					pmt_offset += 2;
					
					//找出video stream pid，当前只支持一路视频流
					pmt_offset += pmt.program_info_length;
					uint32_t pid_info_end = pmt_offset + (pmt.section_length - 9 - pmt.program_info_length - 4);
					while(pid_info_end > pmt_offset){
						uint8_t stream_type = pdata[pmt_offset++];
						uint16_t elementary_PID = (((uint16_t)pdata[pmt_offset]&0x1F) << 8)|pdata[pmt_offset+1];
						pmt_offset += 2;
						uint16_t ES_info_len = (((uint16_t)pdata[pmt_offset]&0x0F) << 8)|pdata[pmt_offset+1];
						pmt_offset += (2+ES_info_len);

						if (stream_type == TS_STREAM_TYPE_V_H264 || stream_type == TS_STREAM_TYPE_V_H265) //H.264 | H.265
						{
							pCtx->stream_video_type = stream_type;
							pCtx->stream_video_pid  = elementary_PID;
							pCtx->PMTflg = 1;
						}
						
						printf(" - PMT stream_type:%#x PID:%#x\n", stream_type, elementary_PID);
					}
					break;
				}
			}

			pCtx->cache_pos += TS_PACKET_FIXED_LEN;
			continue;
		}

		if (ts_header.PID != pCtx->stream_video_pid)
		{//只解析视频包
			pCtx->cache_pos += TS_PACKET_FIXED_LEN;
			continue;
		}

		if (pCtx->last_continuity_counter == TS_INVALID_CONTINUE_COUNT && ts_header.payload_unit_start_indicator != 1)
		{
			pCtx->packetlossflg = 1; //第一帧需要从新一帧开始解析
		}
		
		if (ts_header.continuity_counter != ((pCtx->last_continuity_counter+1)&0xF) && pCtx->last_continuity_counter != TS_INVALID_CONTINUE_COUNT)
		{
			TS_LOG_WARN("ts continuity_cnt(%d), last_cnt(%d), may loss frame!\n", ts_header.continuity_counter, pCtx->last_continuity_counter);
			pCtx->packetlossflg = 1; //丢帧后需要从新一帧开始解析
		}
		pCtx->last_continuity_counter = ts_header.continuity_counter;		
		
		if (ts_header.adaptation_field_contro == 0b10 || ts_header.adaptation_field_contro == 0b11)
		{//have adaptation field			
			uint8_t adapation_field_len = pdata[offset];			
			uint32_t adap_offset = offset + 1;
			if (adapation_field_len > 0){
				uint8_t discontinuity_indicator = (pdata[adap_offset] >> 7)&0x1;
				uint8_t random_access_indicator = (pdata[adap_offset] >> 6)&0x1;
				uint8_t elementary_stream_priority_indicator = (pdata[adap_offset] >> 5)&0x1;
				uint8_t PCR_flag = (pdata[adap_offset] >> 4)&0x1;;
				uint8_t OPCR_flag = (pdata[adap_offset] >> 3)&0x1;
				uint8_t splicing_point_flag = (pdata[adap_offset] >> 2)&0x1;
				uint8_t transport_private_data_flag = (pdata[adap_offset] >> 1)&0x1;
				uint8_t adaptation_field_extension_flag = (pdata[adap_offset++])&0x1;
				if (PCR_flag == 0x01){
					adap_offset += 6;
					//program_clock_reference_base 33
					//reserved 6
					//program_clock_reference_extension 9
				}
				if (OPCR_flag == 0x01){
					adap_offset += 6;
					//program_clock_reference_base 33
					//reserved 6
					//program_clock_reference_extension 9
				}
				if (splicing_point_flag == 0x01){
					adap_offset += 1;
					//splice_countdown 8
				}
				if (transport_private_data_flag == 0x01){
					uint8_t adaptation_field_extension_len = pdata[adap_offset++];
					for (uint32_t i=0; i<adaptation_field_extension_len; i++){
						adap_offset++; //private_data_byte 8
					}
				}
				if (adaptation_field_extension_flag == 0x01){
					uint8_t adapation_field_extension_len = pdata[adap_offset++];
					uint32_t next_offset = adapation_field_extension_len - adap_offset;
					uint8_t ltw_flag = (pdata[adap_offset] >> 7)&0x1;
					uint8_t piecewise_rate_flag = (pdata[adap_offset] >> 6)&0x1;
					uint8_t seamless_splice_flag = (pdata[adap_offset] >> 5)&0x1;
					uint8_t reserved = pdata[adap_offset++]&0x1F;
					if (ltw_flag == 0x01){
						//ltw_valid_flag  1
						//ltw_offset      15
						adap_offset += 2;
					}
					if (piecewise_rate_flag == 0x01){
						//reserved  2
						//piecewise_rate  22
						adap_offset += 3;
					}
					if (seamless_splice_flag == 0x01){
						//splice_type          4
						//DTS_netx_AU[32..30]  3
						//marker_bit           1
						//DTS_next_AU[29..15]  15
						//marker_bit           1
						//DTS_next_AU[14..0]   15
						//marker_bit           1
						adap_offset += 5;
					}
					//reserved
					if (next_offset > adap_offset)
						adap_offset = next_offset;
				}

				//stuffing_byte
			}
			offset += (adapation_field_len + 1);
		}

		if (ts_header.adaptation_field_contro == 0b01 || ts_header.adaptation_field_contro == 0b11)
		{//have payload
			//PES Packet
			if (ts_header.payload_unit_start_indicator == 0)
			{
				if (pCtx->vidRawbuffpos != 0 && !pCtx->packetlossflg)
				{
					uint32_t eslen = TS_PACKET_FIXED_LEN - offset;
					memcpy(pCtx->vidRawbuff+pCtx->vidRawbuffpos, &pdata[offset], eslen);
					pCtx->vidRawbuffpos += eslen;
					TS_LOG_DEBUG("raw:%d eslen:%d\n", pCtx->vidRawbuffpos, eslen);
				}
				pCtx->cache_pos += TS_PACKET_FIXED_LEN;
				continue;
			}
			if (pCtx->curopt != TS_SEARCH_NONE)
			{
				pCtx->lastfileoffset = pCtx->thisfileoffset;
				pCtx->thisfileoffset = ftell(pCtx->fp) - (pCtx->cache_size- pCtx->cache_pos);		
			}
			pCtx->packetlossflg = 0;  //新一帧开始
			
			//PES Header		
			if (!(pdata[offset] == 0x00 && pdata[offset+1] == 0x00 && pdata[offset+2] == 0x01
				&& pdata[offset+3] == PES_STREAM_VIDEO_ID))
			{//当前PES非视频数据包，异常
				pCtx->cache_pos += offset;
				TS_LOG_ERR("%s find pes head[%x %x %x %x] failed!\n", pCtx->name, pdata[offset], pdata[offset+1], pdata[offset+2], pdata[offset+3]);
				return TS_EFAIL;
			}
			if (pCtx->last_pes_packet_len != 0)
			{
				pCtx->last_pes_packet_len = pCtx->this_pes_packet_len;
				pCtx->this_pes_packet_len = (pdata[offset+4] << 8)|pdata[offset+5];
			}
			else
				pCtx->last_pes_packet_len = pCtx->this_pes_packet_len = (pdata[offset+4] << 8)|pdata[offset+5];
			
			if (pdata[offset+3] == PES_STREAM_VIDEO_ID)
			{			
				uint8_t PTS_DTS_flags = pdata[offset+7]&0xC0;
				uint8_t PES_header_data_length = pdata[offset+8];
				/*
				* '0010'      --4
				* PTS[32..30] --3
				* marker_bit  --1
				* PTS[29..15] --15
				* marker_bit  --1
				* PTS[14..0]  --15
				* marker_bit  --1
				*/
				if (PTS_DTS_flags & 0x80)
				{
					unsigned char *pts_buf = pdata + offset + 9;
					uint64_t pts = ((unsigned long long )(pts_buf[0] & 0x0E)) << 29;
					pts |= pts_buf[1] << 22;
					pts |= (pts_buf[2] & 0xFE) << 14;
					pts |= pts_buf[3] << 7;
					pts |= (pts_buf[4] & 0xFE) >> 1;
					pts = pts / 90;

					pCtx->curTimestamp = pts + pCtx->corrTimestamp;
					if (pCtx->lastTimestamp > pCtx->curTimestamp)
					{
						TS_LOG_ERR("%s pst error! last:%lu cur:%lu corr:%lu!\n", pCtx->name, pCtx->lastTimestamp, pCtx->curTimestamp, pCtx->corrTimestamp);
						if (pCtx->lastTimestamp > 0xFFFFFFFF-TS_DIFF_OVERFLOW_THD) //时间戳溢出情形判定
						{
							TS_LOG_WARN("%s pst overflow try to repair!\n", pCtx->name);
							pCtx->corrTimestamp = pCtx->lastTimestamp;
							pCtx->curTimestamp = pts + pCtx->corrTimestamp;
						}
					}

	                TS_LOG_DEBUG("[%s]video pes timestamp[%lu]\n",  pCtx->name, pCtx->curTimestamp);
	                if(pCtx->lastTimestamp != pCtx->curTimestamp)
	                {
	                    int diff = pCtx->curTimestamp - pCtx->lastTimestamp;
	                    if( diff > 0 && diff < 1000 )
	                    {
	                        pCtx->accumulateTime += diff;
	                    }else{
							pCtx->accumulateTime += 40;
						}

	                    pCtx->lastTimestamp = pCtx->curTimestamp;
	                }		

					pCtx->lastvideoTimestamp = pCtx->videoTimestamp;
					if(pCtx->timestamp_mode == 0)
			        	pCtx->videoTimestamp = pCtx->curTimestamp;
					else
						pCtx->videoTimestamp = pCtx->accumulateTime;
				}
				//忽略DTS
				//PES Header END

				//定位到PES数据（RAW Data）
				uint8_t *pNalu =  pdata + offset + 9 + PES_header_data_length;
				uint32_t rawlen = TS_PACKET_FIXED_LEN - offset - 9 - PES_header_data_length;   //ES数据长度 = TS包长度 - TS头 - 自适应长度 - PES_header
				if (pCtx->vidRawbuffpos+rawlen < pCtx->vidRawbuffsize)
				{//拷贝到临时缓存区，多PES包数据通过Pos偏移取出					
					if(pCtx->vidRawbuffpos !=0 && ts_header.payload_unit_start_indicator == 1)
					{//counter==0，完成一帧视频数据传输
						//解析上一帧视频参数
						if(pCtx->stream_video_type == TS_STREAM_TYPE_V_H264){
							int idx = 0;
							while(idx++ < pCtx->vidRawbuffpos)
							{							
								if (pCtx->vidRawbuff[idx] == 0x00 && pCtx->vidRawbuff[idx+1] == 0x00 && pCtx->vidRawbuff[idx+2] == 0x01 && ((pCtx->vidRawbuff[idx+3]&0x1F)==7 || (pCtx->vidRawbuff[idx+3]&0x1F)==0x0F))
								{
									//H.264 I帧一般以SPS NALU打头
									struct video_sps_param_s spsparm;
									if (TS_SOK == H264_SPS_Analyse(pCtx->vidRawbuff+idx-1, pCtx->vidRawbuffpos-idx, &spsparm))
									{
										pCtx->width  = spsparm.width;
										pCtx->height = spsparm.height;
										pCtx->fps	 = spsparm.fps;
										pCtx->codec  = TS_STREAM_TYPE_V_H264;
									}

									pCtx->isCurKey = 1;
									break;
								}
								else
									pCtx->isCurKey = 0;
							}
						}
						else if(pCtx->stream_video_type == TS_STREAM_TYPE_V_H265){
							if (pCtx->vidRawbuff[0] == 0x00 && pCtx->vidRawbuff[1] == 0x00 && pCtx->vidRawbuff[2] == 0x00 
								&& pCtx->vidRawbuff[3] == 0x01 && ((pCtx->vidRawbuff[4] & 0x7E) == 0x40)){
								//H.265 I帧一般以VPS NALU打头
								int idx = 0;
								while(idx++ < pCtx->vidRawbuffpos){
									if (pCtx->vidRawbuff[idx] == 0x00 && pCtx->vidRawbuff[idx+1] == 0x00 && pCtx->vidRawbuff[idx+2] == 0x00 
										&& pCtx->vidRawbuff[idx+3] == 0x01 && ((pCtx->vidRawbuff[idx+4] & 0x7E) == 0x42)){//识别到SPS
										struct video_sps_param_s spsparm;
										if (TS_SOK == H265_SPS_Analyse(pCtx->vidRawbuff+idx, pCtx->vidRawbuffpos-idx, &spsparm))
										{
											pCtx->width  = spsparm.width;
											pCtx->height = spsparm.height;	
											pCtx->fps	 = spsparm.fps;
											pCtx->codec  = TS_STREAM_TYPE_V_H265;
										}
										break;
									}
								}
								pCtx->isCurKey = 1;
							}
							else
								pCtx->isCurKey = 0;
						}
						else
						{
							TS_LOG_ERR("%s meet payload=%#x not support!\n", pCtx->name, pCtx->stream_video_type);
							return TS_EFAIL;
						}
						
						if (pCtx->curopt != TS_SEARCH_NONE)
						{//统计时长，不返回数据
							*frmdata = NULL;
							if (pCtx->isCurKey)
							{//I帧更新offset
								pCtx->curfileoffset = pCtx->lastfileoffset;
							}
						}
						else
						{
							char *p = (char *)malloc(pCtx->vidRawbuffpos);
							memcpy(p, pCtx->vidRawbuff, pCtx->vidRawbuffpos);
							*(char **)frmdata = p;
							*frmlen  = pCtx->vidRawbuffpos;
						}
						
						//缓存区保存当前帧首包
						memcpy(pCtx->vidRawbuff, pNalu, rawlen);
						pCtx->vidRawbuffpos = rawlen;
						pCtx->cache_pos += TS_PACKET_FIXED_LEN;
						goto EXIT;
					}
					else
					{
						memcpy(pCtx->vidRawbuff+pCtx->vidRawbuffpos, pNalu, rawlen);
						pCtx->vidRawbuffpos += rawlen;
						TS_LOG_DEBUG("first raw:%d eslen:%d\n", pCtx->vidRawbuffpos, rawlen);

						//拷贝下一个PES包分片
						pCtx->cache_pos += TS_PACKET_FIXED_LEN;
						continue;
					}
				}
				else
				{
					TS_LOG_ERR("%s pos:%d framelen:%d too long!\n", pCtx->name, pCtx->vidRawbuffpos, rawlen)
				}
			}
		}
		
		pCtx->cache_pos = TS_PACKET_FIXED_LEN;
		break;
	}

EXIT:	
	return TS_SOK;
}


/**
* @brief: 释放一帧数据
*
* @Handle[in]: ts句柄
* @frmdata[int]: 帧数据
*
* @return: succes 0 / fail -1
*
* @note.
**/
int TS_ReleaseFrame(Handle hdl,void *frmdata)
{
	if (frmdata)
		free(frmdata);
	
	return TS_SOK;
}


/**
* @brief: 按照比例跳转最近I帧数据位置
*
* @Handle[in]: ts句柄
* @pos[in]: start_time~end_time 单位ms
* @mode[in]: seek模式 0:forward 1:backward
*
* @return: succes 0 / fail -1
*
* @note.
**/
int TS_SeekPos(Handle hdl, unsigned long long pos, int mode)
{
	ts_demux_ctx_s *pCtx = (ts_demux_ctx_s *)hdl;
	int ret;
	uint8_t *frame;
	int framelen;
	
	if (pos < pCtx->start_time || pos > pCtx->end_time || mode > 1)
	{
		TS_LOG_ERR("%s pos:%llu over limit[%lu, %lu]!\n", pCtx->name, pos, pCtx->start_time, pCtx->end_time);
		return TS_EFAIL;
	}
	
	pCtx->seek_time = pos;
	pCtx->seek_mode = mode;
	pCtx->corrTimestamp  = 0;
	pCtx->accumulateTime = 0;
	pCtx->lastTimestamp  = 0;
	pCtx->lastvideoTimestamp = 0;
	pCtx->curTimestamp	 = 0;
	pCtx->cache_data_len = 0;
	pCtx->cache_pos 	 = 0;
	pCtx->vidRawbuffpos  = 0;
	pCtx->last_continuity_counter  = TS_INVALID_CONTINUE_COUNT;
	
	struct ts_shareinfo *pnode = NULL;
	int idx = 0, lastidx = 0;
	
	ret = ts_get_sharelist(pCtx->name, &pnode);
	if (ret != 0 || pnode == NULL || pnode->atimepos == NULL)
	{
		TS_LOG_ERR("%s find share failed!\n", pCtx->name);
		return TS_EFAIL;
	}

	pthread_mutex_lock(&pnode->mutex);
	if (pCtx->seek_time <= pnode->atimepos[0].postime)
	{//Seek时间早于开始时刻
		idx = 0;
	}
	else if (pCtx->seek_time >= pnode->atimepos[pnode->timeposmax-1].postime)
	{//Seek时间晚于最后时刻
		idx = pnode->timeposmax - 1;
	}
	else
	{
		if (pnode->timeposscale != 0)
			idx = (pCtx->seek_time - pCtx->start_time)/pnode->timeposscale;

		while(1)
		{
			if (pnode->atimepos[idx].postime == pCtx->seek_time)
			{
				break;
			}
			else if(pnode->atimepos[idx].postime < pCtx->seek_time)
			{
				lastidx = idx;
				idx++;
			}else{
				lastidx = idx;
				idx--;
			}

			if (idx < 0 || idx >= pnode->timeposmax)
			{
				TS_LOG_ERR("%s seek_map timestamp:%lu idx(%d) over limit(0, %d)!\n", pCtx->name, pCtx->seek_time, idx, pnode->timeposmax);
				pthread_mutex_unlock(&pnode->mutex);
				return TS_EFAIL;
			}

			if ((pnode->atimepos[lastidx].postime < pCtx->seek_time && pnode->atimepos[idx].postime > pCtx->seek_time)	 \
				|| (pnode->atimepos[lastidx].postime > pCtx->seek_time && pnode->atimepos[idx].postime < pCtx->seek_time))
			{//找到时间临界点
				if (pCtx->seek_mode == 0)
					idx = (lastidx<idx)?(idx):(lastidx);
				else
					idx = (lastidx<idx)?(lastidx):(idx);

				break;
			}
		}
	}

	fseek(pCtx->fp, pnode->atimepos[idx].offset, SEEK_SET);
	pCtx->isEof  = 0;
	pthread_mutex_unlock(&pnode->mutex);
	
	TS_LOG_INFO("%s seek_map timestamp:%lu idx:%d pos_time:%lu offset:%#lx!\n", pCtx->name, pCtx->seek_time, idx, pnode->atimepos[idx].postime, pnode->atimepos[idx].offset);
	return TS_SOK;
}


/**
* @brief: 构建文件时间坐标以加速seek操作
*
* @Handle[in]:ts句柄
*
* @return: succes 0 / fail -1
*
* @note.
**/
int TS_SetTimePosition(Handle hdl)
{
	int ret;
	ts_demux_ctx_s *pCtx = (ts_demux_ctx_s *)hdl;
	struct ts_shareinfo *pnode = NULL;
	char *frame;
	int framelen;
	int idx = 0;

	ret = ts_get_sharelist(pCtx->name, &pnode);
	if (ret != 0 || pnode == NULL)
	{
		TS_LOG_ERR("%s find share failed!\n", pCtx->name);
		return TS_EFAIL;
	}

	pthread_mutex_lock(&pnode->mutex);
	if (pnode->atimepos == NULL)
	{
		//创建时间索引
		timepos_info_s *atimemap = (timepos_info_s *)malloc(sizeof(timepos_info_s)*TS_MAX_LIMIT_POS_NUM);
		pCtx->accumulateTime = 0;
		pCtx->cache_data_len  = 0;
		pCtx->cache_pos 	  = 0;
		pCtx->last_continuity_counter = TS_INVALID_CONTINUE_COUNT;
		pCtx->vidRawbuffpos   = 0;		
		fseek(pCtx->fp, 0, SEEK_SET);
		if (pCtx->timestamp_mode)
		{//mode=1表示需要通过累积时间戳统计时间
			pCtx->start_time = 0;
			pCtx->end_time	 = 0;
		}
		pCtx->curopt	 = TS_SEARCH_READALL;
		while(1)
		{
			ret = TS_ReadFrame(pCtx, (void **)&frame, &framelen);
			if (TS_SOK != ret)
				break;
			if (pCtx->isCurKey)
			{
				if (idx >= TS_MAX_LIMIT_POS_NUM)
				{
					timepos_info_s *atimemaptmp = (timepos_info_s *)malloc(sizeof(timepos_info_s)*(idx + TS_MAX_LIMIT_POS_NUM - 1));
					memcpy(atimemaptmp, atimemap, idx*sizeof(timepos_info_s));
					free(atimemap);
					atimemap = atimemaptmp;
				}
				atimemap[idx].offset = pCtx->curfileoffset;
				atimemap[idx].postime = pCtx->lastvideoTimestamp;
				idx++;
			}

			if (pCtx->timestamp_mode)
				pCtx->end_time = pCtx->lastvideoTimestamp;
			
			TS_ReleaseFrame(pCtx, frame);
		}

		pnode->timeposmax   = idx;
		if(idx >= 2)
			pnode->timeposscale = atimemap[1].postime - atimemap[0].postime;  //坐标单位长度，用于大致确定seek位置
		pnode->atimepos     = atimemap;

		//复位解复用器
		pCtx->curopt        = TS_SEARCH_NONE;
		pCtx->corrTimestamp  = 0;
		pCtx->accumulateTime = 0;
		pCtx->lastTimestamp  = 0;
		pCtx->lastvideoTimestamp  = 0;
		pCtx->curTimestamp   = 0;
		pCtx->cache_data_len = 0;
		pCtx->cache_pos      = 0;
		pCtx->isEof          = 0;
		pCtx->last_continuity_counter = TS_INVALID_CONTINUE_COUNT;
		pCtx->vidRawbuffpos   = 0;		
		fseek(pCtx->fp, 0, SEEK_SET);
	}
	pthread_mutex_unlock(&pnode->mutex);
	
	TS_LOG_INFO("%s build time position map ok!idx:%d scale:%d\n", pCtx->name, pnode->timeposmax, pnode->timeposscale);
	
	return TS_SOK;

}

static int ts_ref_sharelist(char *name, struct ts_shareinfo **pshare)
{
	struct ts_shareinfo *plist = gListshareInfo;
	struct ts_shareinfo *node = NULL;

	for (node = plist; node != NULL; node = node->next)
	{
		if (0 == strcmp(node->name, name) && node->ref > 0)
		{
			node->ref++;
			*pshare = node;
			pthread_mutex_unlock(&node->mutex);
			return 0;
		}
	}	

	*pshare = NULL;
	return -1;
}

static int ts_get_sharelist(char *name, struct ts_shareinfo **pshare)
{
	struct ts_shareinfo *plist = gListshareInfo;
	struct ts_shareinfo *node = NULL;

	for (node = plist; node != NULL; node = node->next)
	{
		if (0 == strcmp(node->name, name) && node->ref > 0)
		{
			pthread_mutex_lock(&node->mutex);
			*pshare = node;
			pthread_mutex_unlock(&node->mutex);
			return 0;
		}
	}	

	*pshare = NULL;
	return -1;
}

static int ts_set_sharelist(ts_demux_ctx_s *pCtx)
{
	struct ts_shareinfo *plist = gListshareInfo;
	struct ts_shareinfo *node = NULL;

	for (node = plist; node != NULL; node = node->next)
	{
		if (0 == strcmp(node->name, pCtx->name) && node->ref > 0)
		{
			pthread_mutex_lock(&node->mutex);
			node->codec = pCtx->codec;
			node->fps  = pCtx->fps;
			node->width = pCtx->width;
			node->height = pCtx->height;
			node->duration_time = pCtx->duration_time;
			node->start_time = pCtx->start_time;
			node->end_time	 = pCtx->end_time;
			node->active     = 1;
			pthread_mutex_unlock(&node->mutex);
			return 0;
		}
	}	

	return -1;
}

static int ts_new_sharelist(char *name)
{
	struct ts_shareinfo *node = gListshareInfo;

	if (node == NULL)
	{
		node = (struct ts_shareinfo *)malloc(sizeof(struct ts_shareinfo));
		memset(node, 0, sizeof(struct ts_shareinfo));
		node->next       = NULL;
		node->ref        = 1;
		node->name       = (char *)malloc(strlen(name)+1);
		memset(node->name, 0, sizeof(strlen(name)+1));
		//node->atimepos   = 
		strcpy(node->name, name);
		pthread_mutex_init(&node->mutex, NULL);
		
		gListshareInfo = node;
		return 0;
	}
	while(1){
		if (node->next == NULL)
		{
			struct ts_shareinfo *tmp = (struct ts_shareinfo *)malloc(sizeof(struct ts_shareinfo));
			memset(tmp, 0, sizeof(struct ts_shareinfo));
			tmp->next		 = NULL;
			tmp->ref         = 1;
			tmp->name		 = (char *)malloc(strlen(name)+1);
			memset(tmp->name, 0, sizeof(strlen(name)+1));
			strcpy(tmp->name, name);
			pthread_mutex_init(&tmp->mutex, NULL);
			//osa_mutex_lock(&node->mutex);
			node->next = tmp;
			//osa_mutex_unlock(&node->mutex);
			break;
		}
		node = node->next;
	}	

	return 0;
}

static int ts_del_sharelist(char *name)
{
	struct ts_shareinfo *node = NULL;
	struct ts_shareinfo *prenode = NULL;

	for (node = gListshareInfo; node != NULL; prenode = node, node = node->next)
	{
		pthread_mutex_lock(&node->mutex);
		if (0 == strcmp(node->name, name))
		{
			if (0 >= --node->ref)
			{
				if (prenode)
					prenode->next = node->next;  //删除当前节点
				else
					gListshareInfo = node->next; //头结点
				free(node->atimepos);
				pthread_mutex_unlock(&node->mutex);
				pthread_mutex_destroy(&node->mutex);
				free(node->name);
				free(node);
					
				return 0;
			}
			pthread_mutex_unlock(&node->mutex);
			return 0;
		}
		pthread_mutex_unlock(&node->mutex);
	}	

	TS_LOG_ERR("%s is not find!\n", name);
	return -1;
}


#ifdef __cplusplus
}
#endif 

