#ifndef __TS_DEMUX_H__
#define __TS_DEMUX_H__

#ifdef __cplusplus
extern "C" {
#endif 


#define DMUX_VIDEO_TYPE_H264 0
#define DMUX_VIDEO_TYPE_H265 1
#define GEN2_VIDEO_H264 2
#define GEN2_VIDEO_H265 3
#define DMUX_VIDEO_TYPE_INVALID 4



typedef void* Handle;

typedef enum
{
    TS_SOK = 0,
    TS_EFAIL = -1,
	TS_EOF = -2
} ts_error_e;

typedef enum
{
    TS_STREAMID_VIDEO = 0,
    TS_STREAMID_AUDIO,
	TS_STREAMID_BUTT
}ts_stream_id_e;


typedef struct ts_stream_info
{
	int 	enType;
	int		codecType;     				 			 
    union
    {
        struct
        {
            unsigned int  width;                      
            unsigned int  height;                     
            unsigned int  fps;                        
        } video;
        struct
        {
            int      	  sampleRate;                 
            int      	  channels;                       
            int      	  bitrate;                    
            int      	  sampleDepth;                
        } audio;
    }info;	
	int				   reserved[4];
}ts_stream_info_s;

typedef struct ts_file_info
{
	ts_stream_info_s  streams[TS_STREAMID_BUTT];
	long 			   start_time;//单位ms 
	long 	   		   end_time;  //单位ms
	int				   reserved[4];
}ts_file_info_s;

/**
* @brief: 初始化ts解封装上下文
*
* @filename[in]: ts文件
*
* @return: ts解封装句柄
*
* @note.返回NULL为失败
**/
Handle TS_Open(char *file);

/**
* @brief: 销毁ts解封装上下文
*
* @Handle[in]: ts句柄
*
* @return: succes 0 / fail -1
*
* @note.
**/
int TS_Close(Handle hdl);

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
int TS_GetFileInfo(Handle hdl, ts_file_info_s *fileinfo);


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
int TS_ReadFrame(Handle hdl,void **frmdata, int *frmlen);


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
int TS_ReleaseFrame(Handle hdl,void *frmdata);


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
int TS_SeekPos(Handle hdl, unsigned long long pos, int mode);


/**
* @brief: 构建文件时间坐标以加速seek操作
*
* @Handle[in]:ts句柄
*
* @return: succes 0 / fail -1
*
* @note.
**/
int TS_SetTimePosition(Handle hdl);


#ifdef __cplusplus
}
#endif 

#endif


