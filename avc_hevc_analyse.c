#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "avc_hevc_analyse.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct GetBitContext
{
    unsigned char *buffer;
    unsigned char *buffer_end;
    int         index;
    int         size_in_bits;
} GetBitContext;

static const unsigned char _ff_golomb_vlc_len[32] = {
    0, 9, 7, 7, 5, 5, 5, 5, 3, 3, 3, 3, 3, 3, 3, 3,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

static const unsigned char ff_log2_table[128] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
};

static const unsigned char _ff_ue_golomb_vlc_code[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
};

static const char ff_se_golomb_vlc_code_[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, -8, 9, -9, 10, -10, 11, -11, 12, -12, 13, -13, 14, -14, 15, -15,
    4, 4, 4, 4, -4, -4, -4, -4, 5, 5, 5, 5, -5, -5, -5, -5, 6, 6, 6, 6, -6, -6, -6, -6, 7, 7, 7, 7, -7, -7, -7, -7,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1,
};
	
static unsigned int get_int(void *ptr)
{
	const unsigned char *buffer = (unsigned char *) ptr;
	return (*buffer) | ((*(buffer + 1)) << 8) | ((*(buffer + 2)) << 16) | ((*(buffer + 3)) << 24);
}

static int _unaligned32_be(void *v)
{
    // unsigned int x = *(unsigned int *) v;
    unsigned int x = get_int(v);
    return (x & 0xFF) << 24 | (x & 0xFF00) << 8 | (x & 0xFF0000) >> 8 | (x & 0xFF000000) >> 24;
}

static void _init_get_bits(GetBitContext * s, unsigned char *buffer, int bit_size)
{
    int buffer_size = ((unsigned int) bit_size + 7) >> 3;

    s->buffer = buffer;
    s->size_in_bits = bit_size;
    s->buffer_end = buffer + buffer_size;
    s->index = 0;
    {
        int         re_index = s->index;
        // int re_cache= H264_unaligned32_be( ((unsigned char *)s->buffer)+(re_index>>3) ) << (re_index&0x07); 

        s->index = re_index;
    }
}

static void _skip_bits(GetBitContext * s, int n)
{
    s->index += n;
}

static void _skip_one_bits(GetBitContext * s)
{
    _skip_bits(s, 1);
}

static unsigned char _get_one_bit(GetBitContext * s)
{

    int         index = s->index;
    unsigned char result = s->buffer[(unsigned int) index >> 3];
    result <<= (index & 0x07);
    result >>= 7;
    index++;
    s->index = index;
    return result;
}

static int unaligned32_be(void *v)
{
    const unsigned char *p = (unsigned char *) v;
    return ((int)p[0] << 24) | ((int)p[1] << 16) | ((int)p[2] << 8) | (p[3]);
}

static long long unaligned64_be(void *v)
{
    const unsigned char *p = (unsigned char *) v;
    return ((long long)p[0] << 56) | ((long long)p[1] << 48) | ((long long)p[2] << 40) | ((long long)p[3] << 32)
		 | ((long long)p[4] << 24) | ((long long)p[5] << 16) | ((long long)p[6] << 8) | (p[7]);
}


static unsigned int bs_get_bits(GetBitContext * s, int n)
{

    int         tmp;
    int         re_index = s->index;
    int         re_cache;
	long long   re_cache_64;
	
	if (n + (re_index & 0x07) < 32)
	{
    	re_cache = unaligned32_be(((unsigned char *) (s)->buffer) + (re_index >> 3)) << (re_index & 0x07);
		tmp = (((unsigned int) (re_cache)) >> (32 - (n)));
	}
	else
	{
		re_cache_64 = unaligned64_be(((unsigned char *) (s)->buffer) + (re_index >> 3)) << (re_index & 0x07);
		tmp = (int)(re_cache_64 >> (64 - n)) & 0xFFFFFFFF;
	}
    re_index += n;
    s->index = re_index;
    return (unsigned int)tmp;
}

#if 0
static unsigned int get_bits_long(GetBitContext * s, int n)
{
    if (n <= 17)
    {
        return bs_get_bits(s, n);
    }
    else
    {
        int ret = bs_get_bits(s, 16) << (n - 16);
        return ret | bs_get_bits(s, n - 16);
    }
}
#endif

static int bs_av_log2(unsigned int v)
{
    int         n = 0;
    if (v & 0xffff0000)
    {
        v >>= 16;
        n += 16;
        if (v & 0xff00)
        {
            v >>= 8;
            n += 8;
        }
    }
    else if (v & 0xff00)
    {
        v >>= 8;
        n += 8;
    }
    n += ff_log2_table[v >> 1];

    return n;
}

static int bs_get_ue_golomb(GetBitContext * gb)
{
    unsigned int buf;
    int         log;
    int         re_index = gb->index;
    int         re_cache;

    re_cache = _unaligned32_be(((unsigned char *) (gb)->buffer) + (re_index >> 3)) << (re_index & 0x07);

    buf = (unsigned int) re_cache;

    if (buf >= (1 << 27))
    {
        buf >>= 32 - 9;
        re_index += _ff_golomb_vlc_len[buf >> 4];

        gb->index = re_index;

        if (buf < 256)
        {
            return _ff_ue_golomb_vlc_code[buf];
        }
        else
        {
            return 0;
        }
    }
    else
    {
        log = 2 * bs_av_log2(buf) - 31;
        buf >>= log;
        buf--;
        re_index += (32 - log);
        gb->index = re_index;
        return buf;
    }
}

static int bs_get_se_golomb(GetBitContext * gb)
{
    unsigned int buf;
    int         log;
    int         re_index = gb->index;
    int         re_cache;

    re_cache = _unaligned32_be(((unsigned char *) (gb)->buffer) + (re_index >> 3)) << (re_index & 0x07);

    buf = (unsigned int)re_cache;

    if (buf >= (1 << 27))
    {
        buf >>= 32 - 9;
        re_index += _ff_golomb_vlc_len[buf >> 4];
        gb->index = re_index;
        if (buf < 256)
        {
            return ff_se_golomb_vlc_code_[buf];
        }
        else
        {
            return 0;
        }
    }
    else
    {
        log = 2 * bs_av_log2(buf) - 31;
        buf >>= log;
        re_index += (32 - log);
        gb->index = re_index;

        if (buf & 1)
            buf = -(buf >> 1); //buf定义的无符号的类型，这里又是有符号,先保持源代码不变
        else
            buf = (buf >> 1);

        return buf;
    }
}

int H264_SPS_Analyse(void *pSps, int len, struct video_sps_param_s *param)
{
	GetBitContext ctx;
	unsigned char *p = (unsigned char *)pSps;
	uint8_t profile_idc;
	uint8_t level_idc;
	uint32_t chroma_format_idc = 1;// 7.4.2.1 ,当chroma_format_idc不存在时，应推断其值为1（4：2：0的色度格式）
	uint32_t pic_width_in_mbs_minus1;
	uint32_t pic_height_in_map_units_minus1;
	uint32_t pic_order_cnt_type;
	uint32_t num_ref_frames_in_pic_order_cnt_cycle;
	uint32_t num_units_in_tick;
	uint32_t time_scale;
	uint8_t frame_mbs_only_flag;
	uint32_t width;
	uint32_t height;
	float    fps;
	//Analyse ref T-REC-H.264-202108-I
	_init_get_bits(&ctx, p+5, len-5); //跳过start_code和nal_unit_header， 定位到SPS rbsp开始处
	profile_idc = bs_get_bits(&ctx, 8);
	_skip_bits(&ctx, 8);              //skip constraint_set0_flag ~ constraint_set5_flag, reserved_zero_2bits
	level_idc = bs_get_bits(&ctx, 8);
	bs_get_ue_golomb(&ctx);           //skip seq_parameter_set_id
	if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 244
		|| profile_idc == 44 || profile_idc == 83 || profile_idc == 86 || profile_idc == 118
		|| profile_idc == 128 || profile_idc == 138 || profile_idc == 139 || profile_idc == 134
		|| profile_idc == 135)
	{
		chroma_format_idc = bs_get_ue_golomb(&ctx);
		if (chroma_format_idc == 3)
			_skip_one_bits(&ctx);   //separate_colour_plane_flag
		bs_get_ue_golomb(&ctx);     //bit_depth_luma_minus8
		bs_get_ue_golomb(&ctx);     //bit_depth_chroma_minus8
		_skip_one_bits(&ctx);       //qpprime_y_zero_transform_bypass_flag   
		if (_get_one_bit(&ctx))     //seq_scaling_matrix_present_flag
		{
			for (int i=0; i<(chroma_format_idc!=3?8:12); i++){
				if (_get_one_bit(&ctx)){  //seq_scaling_list_present_flag[i]
					int loop;
					if (i < 6){
						loop = 16;

					}else{
						loop = 64;
					}
					int lastScale = 8;
					int nextScale = 8;
					int delta_scale;
					for (int j=0; j < loop; j++){
						if (nextScale != 0){
							delta_scale = bs_get_se_golomb(&ctx); //delta_scale
							nextScale = (lastScale + delta_scale + 256)%256;
						}
						lastScale = (nextScale == 0)?lastScale:nextScale;
					}
				}
			}
		}
	}

	if (0 == profile_idc)
		return -1;

	bs_get_ue_golomb(&ctx);    //log2_max_frame_num_minus4
	pic_order_cnt_type = bs_get_ue_golomb(&ctx);
	if (pic_order_cnt_type == 0){
		bs_get_ue_golomb(&ctx);     //log2_max_pic_order_cnt_lsb_minus4
	}else if (pic_order_cnt_type == 1){
		_skip_one_bits(&ctx);       //delta_pic_order_always_zero_flag
		bs_get_se_golomb(&ctx);     //offset_for_non_ref_pic
		bs_get_se_golomb(&ctx);     //offset_for_top_to_bottom_field
		num_ref_frames_in_pic_order_cnt_cycle = bs_get_ue_golomb(&ctx);
		for (int i=0; i<num_ref_frames_in_pic_order_cnt_cycle; i++)
			bs_get_se_golomb(&ctx); //offset_for_ref_frame[i]
	}

	bs_get_ue_golomb(&ctx); //max_num_ref_frames
	_skip_one_bits(&ctx);   //gaps_in_frame_num_value_allowed_flag
	pic_width_in_mbs_minus1 = bs_get_ue_golomb(&ctx);
	pic_height_in_map_units_minus1 = bs_get_ue_golomb(&ctx);
	frame_mbs_only_flag = _get_one_bit(&ctx);
	if(!frame_mbs_only_flag)  //frame_mbs_only_flag
		_skip_one_bits(&ctx); //mb_adaptive_frame_field_flag

	width  = (pic_width_in_mbs_minus1+1) * 16;
	height = (2 - frame_mbs_only_flag)* (pic_height_in_map_units_minus1 +1) * 16;	

	_skip_one_bits(&ctx);   //direct_8x8_inference_flag
	if(_get_one_bit(&ctx))  //frame_cropping_flag
	{
        int  frame_crop_left_offset =0;
        int  frame_crop_right_offset =0;
        int  frame_crop_top_offset =0;
        int  frame_crop_bottom_offset =0;
        int crop_unit_x;	
        int crop_unit_y;
       
        frame_crop_left_offset = bs_get_ue_golomb(&ctx);                      // frame_crop_left_offset
        frame_crop_right_offset = bs_get_ue_golomb(&ctx);                     // frame_crop_right_offset
        frame_crop_top_offset = bs_get_ue_golomb(&ctx);                       // frame_crop_top_offset
        frame_crop_bottom_offset = bs_get_ue_golomb(&ctx);                    // frame_crop_bottom_offset

        // 7.4.2.1.1 RBSP syntax
        if (0 == chroma_format_idc) // monochrome
        {
        	crop_unit_x = 1;
        	crop_unit_y = 2 - frame_mbs_only_flag;
        }
        else if (1 == chroma_format_idc) // 4:2:0
        {
        	crop_unit_x = 2;
        	crop_unit_y = 2 * (2 - frame_mbs_only_flag);
        }
        else if (2 == chroma_format_idc) // 4:2:2
        {
        	crop_unit_x = 2;
        	crop_unit_y = 2 - frame_mbs_only_flag;
        }
        else // 3 == sps.chroma_format_idc   // 4:4:4
        {
        	crop_unit_x = 1;
        	crop_unit_y = 2 - frame_mbs_only_flag;
        }
        width  -= crop_unit_x * (frame_crop_left_offset + frame_crop_right_offset);
        height -= crop_unit_y * (frame_crop_top_offset  + frame_crop_bottom_offset);		
	}
	
	
	//VUI信息，计算视频帧率
	if (_get_one_bit(&ctx)) //vui_parameters_present_flag
	{
		if(_get_one_bit(&ctx)) //aspect_ratio_info_present_flag
		{
			if (bs_get_bits(&ctx, 8) == 255) //aspect_ratio_idc
			{
				_skip_bits(&ctx, 32); //sar_width、sarheight
			}
		}

		if (_get_one_bit(&ctx))    //overscan_info_present_flag
			_skip_one_bits(&ctx);  //overscan_appropriate_flag
		if (_get_one_bit(&ctx))    //video_signal_type_present_flag
		{
			_skip_bits(&ctx, 3);   //video_format
			_skip_bits(&ctx, 1);   //video_full_range_flag
			if (_get_one_bit(&ctx))   //colour_description_present_flag
			{
				_skip_bits(&ctx, 8);   //colour_primaries
				_skip_bits(&ctx, 8);   //transfer_characteristics
				_skip_bits(&ctx, 8);   //matrix_coefficients
			}
		}

		if (_get_one_bit(&ctx))   //chroma_loc_info_present_flag
		{
			bs_get_ue_golomb(&ctx);	 //chroma_sample_loc_type_top_field
			bs_get_ue_golomb(&ctx);  //chroma_sample_loc_type_bottom_field
		}
		if (_get_one_bit(&ctx))   //timing_info_present_flag
		{
			num_units_in_tick = bs_get_bits(&ctx, 32);
			time_scale = bs_get_bits(&ctx, 32);
			_skip_one_bits(&ctx);  //fixed_frame_rate_flag

			fps = (float)(time_scale)/(float)(num_units_in_tick*2);
		}

		uint8_t nal_hrd_parameters_present_flag = _get_one_bit(&ctx);
		if (nal_hrd_parameters_present_flag)  //nal_hrd_parameters_present_flag
		{//hrd_parameters()
			int ct = bs_get_ue_golomb(&ctx) + 1;
	        _skip_bits(&ctx, 8);
	        for (int i = 0; i < ct; i++)
	        {
	            bs_get_ue_golomb(&ctx);
	            bs_get_ue_golomb(&ctx);
	            _skip_one_bits(&ctx);
	        }
	        _skip_bits(&ctx, 20);		
		}
		uint8_t vcl_hrd_parameters_present_flag = _get_one_bit(&ctx);
		if (vcl_hrd_parameters_present_flag)  //vcl_hrd_parameters_present_flag
		{//hrd_parameters()
			int ct = bs_get_ue_golomb(&ctx) + 1;
			_skip_bits(&ctx, 8);
			for (int i = 0; i < ct; i++)
			{
				bs_get_ue_golomb(&ctx);
				bs_get_ue_golomb(&ctx);
				_skip_one_bits(&ctx);
			}
			_skip_bits(&ctx, 20);		
		}
		if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
		{
			_skip_one_bits(&ctx); //low_delay_hrd_flag
		}
		_skip_one_bits(&ctx);     //pic_struct_present_flag
		if (_get_one_bit(&ctx))   //bitstream_restrictrion_flag
		{   
			_skip_one_bits(&ctx);    //motion_vectors_over_pic_boundaries_flag
			bs_get_ue_golomb(&ctx);  //max_bytes_per_pic_denom
			bs_get_ue_golomb(&ctx);  //max_bits_per_mb_denom
			bs_get_ue_golomb(&ctx);  //log2_max_mv_length_horizontal
			bs_get_ue_golomb(&ctx);  //log2_max_mv_length_vertical
			bs_get_ue_golomb(&ctx);  //max_num_reorder_frames
			bs_get_ue_golomb(&ctx);  //max_dec_frame_buffering
		}
	}
	
	param->width = width;
	param->height = height;
	param->fps   = (int)fps;
	param->level = level_idc;
	param->profile = profile_idc;
	return 0;
}

/* 去掉防竞争字节 */
static uint32_t EBSPtoRBSP(uint8_t *streamBuffer, uint32_t end_bytepos, uint32_t begin_bytepos)
{
    uint32_t i;
    uint32_t j;
    uint32_t count;
    
    count = 0;
    j = begin_bytepos;
    
    for (i = begin_bytepos; i < end_bytepos; i++)
    {
        // in NAL unit, 0x000000, 0x000001, 0x000002 shall not occur at any byte-aligned position
        if (count == 2 && streamBuffer[i] < 0x03)
        {
            return -1;
        }
        
        if (count == 2 && streamBuffer[i] == 0x03)
        {
            // check the 4th byte after 0x000003, except when cabac.....
            if ((i < end_bytepos - 1) && (streamBuffer[i + 1] > 0x03))
            {
                return -1;
            }
            
            if (i == end_bytepos - 1)
            {
                return j;
            }
            
            // escape 0x03 byte!
            i++;
            count = 0;
        }
        
        streamBuffer[j] = streamBuffer[i];
        //printf("[%02u] 0x%02x\n", j, streamBuffer[j]);
        
        if (streamBuffer[i] == 0x00)
        {
            count++;
        }
        else
        {
            count = 0;
        }
        
        j++;
    }
    
    return j;
}

int H265_SPS_Analyse(void *pSps, int len, struct video_sps_param_s *param)
{
	//获取SPS NALU长度
	GetBitContext ctx;
	int spslen;
	uint8_t *p = NULL;
	uint8_t  sps_max_sub_layers_minus1;
	uint32_t profile_idc;
	uint32_t level_idc;
	uint32_t sub_layer_profile_present_flag[512];
	uint32_t chroma_format_idc;
	uint32_t pic_width_in_luma_samples, pic_height_in_luma_samples;
	uint8_t  conformance_window_flag;
	uint8_t  sps_sub_layer_ordering_info_present_flag;
	uint32_t sps_max_dec_pic_buffering[7] = {0};
	uint32_t num_short_term_ref_pic_sets;
	uint32_t log2_max_pic_order_cnt_lsb_minus4;
	uint8_t inter_ref_pic_set_prediction_flag;
	uint32_t RefRpsIdx, delta_idx_minus1;
	uint32_t NumDeltaPocs[64];
	uint32_t vui_num_units_in_tick, vui_time_scale;
	float    fps;
	int j;

	p = (uint8_t *)pSps;
	for(spslen=4; spslen < len; spslen++){
		if (p[spslen]==0x00 && p[spslen+1]==0x00 && p[spslen+2]==0x00 && p[spslen+3]==0x01)
			break;
	}
	p = (uint8_t *)malloc(spslen);
	memcpy(p, pSps, spslen); //解析需要去掉防竞争码，不修改原码流

	spslen = EBSPtoRBSP(p, spslen, 0); //去掉防0x03竞争码
	
	//Analyse ref T-REC-H.265-202108-I
	_init_get_bits(&ctx, p+6, spslen-6); //跳过start_code和nal_unit_header， 定位到SPS rbsp开始处	
	bs_get_bits(&ctx, 4);     //sps_video_parameter_set_id
	sps_max_sub_layers_minus1 = bs_get_bits(&ctx, 3);
	_skip_one_bits(&ctx);     //sps_temporal_id_nesting_flag

	/* begin parse_ptl */
	//general_profile_space
	bs_get_bits(&ctx, 2);
	//general_tier_flag
	bs_get_bits(&ctx, 1);
	//general_profile_idc
	profile_idc = bs_get_bits(&ctx, 5);
	
	//general_profile_compatibility_flag
	for (j = 0; j < 32; j++)
    {
        _skip_one_bits(&ctx);
    }
	//general_progressive_source_flag
	//general_interlaced_source_flag
	//general_non_packed_constraint_flag
	//general_frame_only_constraint_flag
	_skip_bits(&ctx, 4);
	
	//XXX_reserved_zero_44bits[0..15]
	bs_get_bits(&ctx, 16);
	//XXX_reserved_zero_44bits[16..31]
	bs_get_bits(&ctx, 16);
	//XXX_reserved_zero_44bits[32..43]
	bs_get_bits(&ctx, 12);
	//general_level_idc
	level_idc = bs_get_bits(&ctx, 8);
	
	for (j = 0; j < sps_max_sub_layers_minus1; j++)
    {
    	//sub_layer_profile_present_flag
        sub_layer_profile_present_flag[j] = bs_get_bits(&ctx, 1);
		//sub_layer_level_present_flag
		_skip_one_bits(&ctx);
    }

	if (sps_max_sub_layers_minus1 > 0)
    {
        for (j = sps_max_sub_layers_minus1; j < 8; j++)
        {
            //reserved_zero_2bits
			bs_get_bits(&ctx, 2);
        }
    }

	for (j = 0; j < sps_max_sub_layers_minus1; j++)
    {
    	//sub_layer_profile_present_flag
        if (sub_layer_profile_present_flag[j])
        {
        	//sub_layer_profile_space
			bs_get_bits(&ctx, 2);
			//sub_layer_tier_flag
			bs_get_bits(&ctx, 1);
			//sub_layer_profile_idc
			bs_get_bits(&ctx, 5);
		}
		
    }		
	/* end parse_ptl */

	//sps_seq_parameter_set_id
    bs_get_ue_golomb(&ctx);
	//chroma_format_idc
    chroma_format_idc = bs_get_ue_golomb(&ctx);
	if (3 == chroma_format_idc)
	{
		//separate_colour_plane_flag
		_skip_one_bits(&ctx);
	}
   
    pic_width_in_luma_samples  = bs_get_ue_golomb(&ctx);
    pic_height_in_luma_samples = bs_get_ue_golomb(&ctx);

	//conformance_window_flag
	conformance_window_flag = bs_get_bits(&ctx, 1);
	if(conformance_window_flag)
	{
		//conf_win_left_offset
		bs_get_ue_golomb(&ctx);
		//conf_win_right_offset  
		bs_get_ue_golomb(&ctx);
		//conf_win_top_offset    
		bs_get_ue_golomb(&ctx);
		//conf_win_bottom_offset
		bs_get_ue_golomb(&ctx);
	}

	bs_get_ue_golomb(&ctx); //bit_depth_luma_minus8
	bs_get_ue_golomb(&ctx); //bit_depth_chroma_minus8

	//log2_max_pic_order_cnt_lsb_minus4
	log2_max_pic_order_cnt_lsb_minus4 = bs_get_ue_golomb(&ctx);

	sps_sub_layer_ordering_info_present_flag = bs_get_bits(&ctx, 1);
	j = sps_sub_layer_ordering_info_present_flag?0:sps_max_sub_layers_minus1;
	for (j = (sps_sub_layer_ordering_info_present_flag?0:sps_max_sub_layers_minus1); j <= sps_max_sub_layers_minus1; j++)
	{
		sps_max_dec_pic_buffering[j] = bs_get_ue_golomb(&ctx);
		bs_get_ue_golomb(&ctx); //sps_max_num_reorder_pics
		bs_get_ue_golomb(&ctx); //sps_max_latency_increase_plus1
	}

	bs_get_ue_golomb(&ctx);   //log2_min_luma_coding_block_size_minus3
	bs_get_ue_golomb(&ctx);   //log2_diff_max_min_luma_coding_block_size
	bs_get_ue_golomb(&ctx);   //log2_min_luma_transform_block_size_minus2
	bs_get_ue_golomb(&ctx);   //log2_diff_max_min_luma_transform_block_size
	bs_get_ue_golomb(&ctx);   //max_transform_hierarchy_depth_inter
	bs_get_ue_golomb(&ctx);   //max_transform_hierarchy_depth_intra
	     
	if (_get_one_bit(&ctx))     //scaling_list_enabled_flag
	{
		if (_get_one_bit(&ctx)) //sps_scaling_list_data_present_flag
		{
			//scaling_list_data
			uint32_t scaling_list_pred_mode_flag[4][6];	
			for(int sizeId=0; sizeId<4; sizeId++){
				for(int maxtrixId=0; maxtrixId<6; maxtrixId+=(sizeId==3)?3:1){
					scaling_list_pred_mode_flag[sizeId][maxtrixId] = _get_one_bit(&ctx);
					if (!scaling_list_pred_mode_flag[sizeId][maxtrixId])
						bs_get_ue_golomb(&ctx);   //scaling_list_pred_matrix_id_delta
					else{
						int coefNum=64<(1<<(4+(sizeId<<1)))?64:(1<<(4+(sizeId<<1)));
						if (sizeId>1){
							bs_get_se_golomb(&ctx);    //scaling_list_dc_coef_minus8
						}
						for(j=0; j<coefNum;j++){
							bs_get_se_golomb(&ctx);    //scaling_list_delta_coef
						}
					}
				}
			}
		}
	}
	_get_one_bit(&ctx);       //amp_enabled_flag
	_get_one_bit(&ctx);       //sample_adaptive_offset_enabled_flag
	if(_get_one_bit(&ctx))    //pcm_enabled_flag
	{
		bs_get_bits(&ctx, 4); //pcm_sample_bit_depth_luma_minus1
		bs_get_bits(&ctx, 4); //pcm_sample_bit_depth_chroma_minus1
		bs_get_ue_golomb(&ctx);//log2_min_pcm_luma_coding_block_size_minus3
		bs_get_ue_golomb(&ctx);//log2_diff_max_min_pcm_luma_coding_block_size
		_get_one_bit(&ctx);   //pcm_loop_filter_disabled_flag
	}
	num_short_term_ref_pic_sets = bs_get_ue_golomb(&ctx);   //num_short_term_ref_pic_sets
	for(j=0; j<num_short_term_ref_pic_sets; j++){
		//st_ref_pic_set(j)
		inter_ref_pic_set_prediction_flag = 0;
		delta_idx_minus1 = 0;
		if (j != 0)
			inter_ref_pic_set_prediction_flag = bs_get_bits(&ctx, 1);
		if (inter_ref_pic_set_prediction_flag){
			if (j == num_short_term_ref_pic_sets)
				delta_idx_minus1 = bs_get_ue_golomb(&ctx); //delta_idx_minus1
			_skip_one_bits(&ctx);       //delta_rps_sign
			bs_get_ue_golomb(&ctx);     //abs_delta_rps_minus1
			RefRpsIdx = j-(delta_idx_minus1 + 1);
			for(int i=0; i<=NumDeltaPocs[RefRpsIdx]; i++){
				if (bs_get_bits(&ctx, 1)) //used_by_curr_pic_flag[i]
					bs_get_bits(&ctx, 1); //use_delta_flag[i]
			}
		}else{
			uint32_t num_negative_pics = bs_get_ue_golomb(&ctx);
			uint32_t num_positive_pics = bs_get_ue_golomb(&ctx);
			for (int i=0; i<num_negative_pics; i++){
				bs_get_ue_golomb(&ctx);  //delta_poc_s0_minus1[i]
				bs_get_bits(&ctx, 1);    //used_by_curr_pic_s0_flag[i]
			}
			for (int i=0; i<num_positive_pics; i++){
				bs_get_ue_golomb(&ctx);  //delta_poc_s1_minus1[i]
				bs_get_bits(&ctx, 1);    //used_by_curr_pic_s1_flag[i]
			}
			NumDeltaPocs[j] = num_negative_pics + num_positive_pics;
		}
		
	}
	if (_get_one_bit(&ctx)){   //long_term_ref_pics_present_flag
		uint32_t num_long_term_ref_pics_sps = bs_get_ue_golomb(&ctx);
		for (j=0; j<num_long_term_ref_pics_sps; j++){
			bs_get_bits(&ctx, log2_max_pic_order_cnt_lsb_minus4+4);
			_get_one_bit(&ctx);   //used_by_curr_pic_lt_sps_flag[i]
		}
	}
	_get_one_bit(&ctx);   //sps_temporal_mvp_enabled_flag
	_get_one_bit(&ctx);   //strong_intra_smoothing_enabled_flag
	if (_get_one_bit(&ctx))   //vui_parameters_present_flag
	{//vui_parameters
		if (_get_one_bit(&ctx)){              //aspect_ratio_info_present_flag
			if (bs_get_bits(&ctx, 8) == 255){ //aspect_ratio_flag
				bs_get_bits(&ctx, 16);   //sar_width
				bs_get_bits(&ctx, 16);   //sar_height
			}
		}
		if (_get_one_bit(&ctx))    //overscan_info_present_flag
			_get_one_bit(&ctx);    //oversacn_appropriate_flag
		
		if (_get_one_bit(&ctx)){   //video_signal_type_present_flag
			bs_get_bits(&ctx, 3);  //video_format
			bs_get_bits(&ctx, 1);  //video_full_range_flag
			if (_get_one_bit(&ctx)){  //colour_description_present_flag
				bs_get_bits(&ctx, 8); //colour_primaries
				bs_get_bits(&ctx, 8); //transfer_characteristics
				bs_get_bits(&ctx, 8); //matrix_coeffs
			}
		}

		if (_get_one_bit(&ctx)){ //chroma_loc_info_present_flag
			bs_get_ue_golomb(&ctx);  //chroma_sample_loc_type_top_field
			bs_get_ue_golomb(&ctx);  //chroma_sample_loc_type_bottom_field
		}
		_skip_one_bits(&ctx);   //neutral_chroma_indication_flag
		_skip_one_bits(&ctx);   //field_seq_flag
		_skip_one_bits(&ctx);   //frame_field_info_present_flag
		if (_get_one_bit(&ctx)){//default_display_window_flag
			bs_get_ue_golomb(&ctx);   //def_disp_win_left_offset
			bs_get_ue_golomb(&ctx);   //def_disp_win_right_offset
			bs_get_ue_golomb(&ctx);   //def_disp_win_top_offset
			bs_get_ue_golomb(&ctx);   //def_disp_win_bottom_offset
		}
		if (_get_one_bit(&ctx)){//vui_timing_info_present_flag
			vui_num_units_in_tick = bs_get_bits(&ctx, 32);
			vui_time_scale = bs_get_bits(&ctx, 32);
			fps = (float)(vui_time_scale)/(float)(vui_num_units_in_tick);
			//余下参数忽略
		}
	}	

	
	param->width = pic_width_in_luma_samples;
	param->height = pic_height_in_luma_samples;
	param->fps   = (int)fps;
	param->level = level_idc;
	param->profile = profile_idc;
	param->max_ref_num = sps_max_dec_pic_buffering[0];
	free(p);
	
	return 0;
}




#ifdef __cplusplus
}
#endif

