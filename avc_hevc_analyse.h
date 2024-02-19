#ifndef __AVC_HEVC_ANALYS_H__
#define __AVC_HEVC_ANALYS_H__

#ifdef __cplusplus
extern "C"{
#endif

struct video_sps_param_s{
	int width;
	int height;
	int fps;
	int level;
	int profile;
	int max_ref_num;
};

int H264_SPS_Analyse(void *pSps, int len, struct video_sps_param_s *param);
int H265_SPS_Analyse(void *pSps, int len, struct video_sps_param_s *param);


#ifdef __cplusplus
}
#endif
#endif
