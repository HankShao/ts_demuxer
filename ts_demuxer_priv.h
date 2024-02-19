#ifndef __TS_DEMUX_PRIV_H__
#define __TS_DEMUX_PRIV_H__


#ifdef __cplusplus
extern "C" {
#endif 


#define TS_LOG_DEBUG(fmt, args ...) //printf("\033[0;32;32m""WARN (%s|%d):"fmt "\033[m", __func__, __LINE__, ##args);
#define TS_LOG_INFO(fmt, args ...) printf("\033[0;32;32m""INFO (%s|%d):" fmt "\033[m", __func__, __LINE__, ##args);
#define TS_LOG_WARN(fmt, args ...) printf("\033[1;33m""WARN (%s|%d):" fmt "\033[m", __func__, __LINE__, ##args);
#define TS_LOG_ERR(fmt, args ...)  printf("\033[0;32;31m""ERR (%s|%d):" fmt "\033[m", __func__, __LINE__, ##args);


#define  CACHE_PS_PIECE_SIZE  (512*1024)
#define  VIDEO_RAWDATA_BUFF_SIZE  (1024*1024)
#define  TS_DIFF_OVERFLOW_THD     (2*1000*90)
#define  TS_DURATION_OVERFLOW_THD (60*60*1000*90)
#define  TS_INVALID_CONTINUE_COUNT 0xFF

#define  TS_PACKET_FIXED_LEN   188

enum{
	TS_SEARCH_NONE,
	TS_SEARCH_START,
	TS_SEARCH_END,
	TS_SEARCH_READALL
}TS_SEARCH_OPT;

typedef struct{
	char *name;
	FILE *fp;
	uint64_t  tid;
	uint32_t  width;
	uint32_t  height;
	uint32_t  fps;
	uint32_t  codec;
	uint32_t  timestamp_mode;
	uint32_t  seek_mode;
	uint64_t  seek_time;
	uint64_t  duration_time;
	uint64_t  start_time;
	uint64_t  end_time;
	uint8_t   *pcache;
	uint32_t  cache_size;
	uint32_t  cache_data_len;
	uint32_t  cache_pos;

	uint8_t   stream_video_type;
	uint8_t   stream_audio_type;
	uint8_t   stream_video_pid;
	uint8_t   stream_audio_pid;
	uint8_t   *vidRawbuff;
	uint32_t  vidRawbuffsize;
	uint32_t  vidRawbuffpos;
	uint32_t last_pes_packet_len;     //上一个PES包长度
	uint32_t this_pes_packet_len;     //当前PES包长度
	uint32_t last_continuity_counter; //上一TS包计数
	uint8_t  packetlossflg;           //包不连续标识
	
	uint64_t accumulateTime;
	uint64_t curTimestamp;   //PES当前包时间戳
	uint64_t lastTimestamp;  //PES上一包时间戳
	uint64_t videoTimestamp;     //修正的当前视频帧时间戳
	uint64_t lastvideoTimestamp; //修正的上一帧视频帧时间戳
	uint64_t corrTimestamp;	 //帧时间累积

	uint8_t  isCurKey;       //当前帧是否为I帧
	uint8_t  PATflg;         //已经找到PAT
	uint8_t  PMTflg;         //已经找到PMT
	struct pat_program *PAT_Program[10];
	uint8_t  curopt;         //当前操作
	uint8_t  isEof;
	int64_t  lastfileoffset; //上一帧文件读位置
	int64_t  thisfileoffset; //上一帧文件读位置
	int64_t  curfileoffset;  //当前返回帧文件读位置
	int32_t  counter;
}ts_demux_ctx_s;

//ISO13818协议定义区



#define TS_MAX_LIMIT_POS_NUM 1800   //默认最大记录1800个时间标记

typedef struct {
	uint64_t postime;
	uint64_t offset;
}timepos_info_s;

struct ts_shareinfo{
	char *name;
	uint32_t  active;
	uint32_t  ref;
	uint64_t  duration_time;
	uint64_t  start_time;
	uint64_t  end_time;
	uint32_t  width;
	uint32_t  height;
	uint32_t  codec;
	uint32_t  fps;
	timepos_info_s  *atimepos;    //时间节点坐标
	uint32_t  timeposscale;       //时间节点刻度
	uint32_t  timeposmax;         //时间刻度最大值	
	struct ts_shareinfo *next;
	pthread_mutex_t mutex;
};

struct transport_packet{
	unsigned sync_byte:8;
	unsigned ts_err_indicator:1;
	unsigned payload_unit_start_indicator:1;
	unsigned ts_priority:1;
	unsigned PID:13;
	unsigned ts_scrambling_control:2;
	unsigned adaptation_field_contro:2;
	unsigned continuity_counter:4;
} __attribute__((packed));

struct pat_program{
	unsigned program_number:16;
	unsigned reserved:3;
	unsigned program_map_PID:13;
}__attribute__((packed));
struct program_association_section{
	unsigned table_id:8;
	unsigned section_syntax_indicator:1;
	unsigned zero:1;
	unsigned reserved1:2;
	unsigned section_length:12;
	unsigned transport_stream_id:16;
	unsigned reserved2:2;
	unsigned version_number:5;
	unsigned current_next_indicator:1;
	unsigned section_number:8;
	unsigned last_section_number:8;
	//pat_program
	//CRC_32
} __attribute__((packed));

#define  TS_STREAM_TYPE_V_H264 0x1b
#define  TS_STREAM_TYPE_V_H265 0x24

#define  PES_STREAM_VIDEO_ID   0xE0

struct TS_program_map_section{
	unsigned table_id :8;
	unsigned section_syntax_indicator :1;
	unsigned zero:1;
	unsigned reserved :2;
	unsigned section_length :12;
	unsigned program_number :16;
	unsigned reserved1 :2;
	unsigned version_number :5;
	unsigned current_next_indicator :1;
	unsigned section_number :8;
	unsigned last_section_number :8;
	unsigned reserved2 :3;
	unsigned PCR_PID :13;
	unsigned reserved3 :4;
	unsigned program_info_length :12;
}__attribute__((packed));


static int ts_ref_sharelist(char *name, struct ts_shareinfo **pshare);
static int ts_new_sharelist(char *name);
static int ts_del_sharelist(char *name);
static int ts_set_sharelist(ts_demux_ctx_s *pCtx);
static int ts_get_sharelist(char *name, struct ts_shareinfo **pshare);


#ifdef __cplusplus
}
#endif 

#endif


