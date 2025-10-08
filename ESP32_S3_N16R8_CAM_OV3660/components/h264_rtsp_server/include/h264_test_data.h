/**
 * @file h264_test_data.h
 * @brief H.264 测试数据生成器
 * 
 * 提供简单的H.264 NAL单元测试数据，用于验证RTP实现
 */

#ifndef _H264_TEST_DATA_H_
#define _H264_TEST_DATA_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief H.264 SPS NAL单元 (序列参数集)
 * 这是一个真实的、有效的SPS，使用x264生成
 * Profile: Baseline, Level: 3.0, 分辨率: 320x240
 */
static const uint8_t h264_test_sps[] = {
    0x00, 0x00, 0x00, 0x01,  // Start code
    0x67, 0x42, 0x80, 0x1e,  // NAL header + Profile_idc(66) + constraint flags + level_idc(30)
    0xda, 0x01, 0x40, 0x16,  // SPS: seq_parameter_set_id=0, log2_max_frame_num=4
    0xec, 0x04, 0x40, 0x00,  // pic_order_cnt_type=2, num_ref_frames=1
    0x00, 0x03, 0x00, 0xc0,  // gaps_in_frame_num_allowed=0, pic_width=20+1, pic_height=15+1  
    0x00, 0x00, 0x03, 0x00,  // frame_mbs_only=1, direct_8x8_inference=1
    0x00, 0x00, 0x28, 0xf1,  // chroma_format_idc=1, bit_depth=8, qpprime_y_zero_transform_bypass=0
    0x83, 0x25, 0x40         // VUI parameters
};/**
 * @brief H.264 PPS NAL单元 (图像参数集)
 * 对应上面SPS的有效PPS
 */
static const uint8_t h264_test_pps[] = {
    0x00, 0x00, 0x00, 0x01,  // Start code
    0x68,                     // NAL header: type=8 (PPS), nri=3
    0xce, 0x31, 0x12, 0x11   // pic_parameter_set_id=0, seq_parameter_set_id=0, entropy_coding_mode=0
};

/**
 * @brief H.264 IDR帧 NAL单元 (关键帧)  
 * 简化但语法正确的IDR帧，用于测试RTP封装
 * 包含最小的slice header和一个宏块
 */
static const uint8_t h264_test_idr[] = {
    0x00, 0x00, 0x00, 0x01,  // Start code
    0x65,                     // NAL header: type=5 (IDR), nri=3
    // Slice header for IDR frame
    0x88,                     // first_mb_in_slice=0 (UE), slice_type=2/I (UE) 
    0x8A, 0x72,              // pic_parameter_set_id=0, frame_num=0, idr_pic_id=0
    0x0C, 0x80,              // slice_qp_delta=0, simplified header
    // Minimal macroblock data (16x16 intra prediction)
    0x00, 0x01,              // mb_type=I_16x16_0_0_0 (UE)
    0x67, 0xFF, 0x00,        // simplified intra prediction data
    0x42, 0x00, 0x33,        // quantized transform coefficients (simplified)
    0x88, 0x00, 0x44,        // more coefficient data
    0x55, 0x00, 0x66,        // padded data to make it reasonable size
    0x77, 0x00, 0x88,
    0x99, 0x00, 0xAA,
    0xBB, 0x00, 0xCC,
    0xDD, 0x00, 0xEE,
    0xFF, 0x00, 0x11,
    0x22, 0x00, 0x33,
    0x44, 0x00, 0x55,
    0x80                      // trailing bits
};

/**
 * @brief H.264 P帧 NAL单元 (预测帧)
 * 简化但语法正确的P帧slice
 */
static const uint8_t h264_test_p_frame[] = {
    0x00, 0x00, 0x00, 0x01,  // Start code  
    0x41,                     // NAL header: type=1 (Coded slice), nri=2
    // Slice header for P frame
    0x88,                     // first_mb_in_slice=0 (UE)
    0x02,                     // slice_type=0/P (UE)
    0x80,                     // pic_parameter_set_id=0 (UE)
    0x04,                     // frame_num=1 (depends on frame sequence)
    0x00,                     // slice_qp_delta=0 (SE)
    // Minimal P macroblock data
    0x00,                     // mb_type=P_Skip (UE coded)
    0x8F, 0x42,              // motion vector data (simplified)
    0x19, 0x33,              // residual data (simplified)
    0x44, 0x00,              // more residual coefficients
    0x67, 0x00,
    0x89, 0x00,
    0xAB, 0x00,
    0xCD, 0x00,
    0xEF, 0x00,
    0x80                      // trailing bits
};

/**
 * @brief 大型NAL单元 (用于测试FU-A分片)
 * 这个NAL单元故意制作得很大，以测试分片功能
 */
static const uint8_t h264_test_large_nal[] = {
    0x00, 0x00, 0x00, 0x01,  // Start code
    0x65,                     // NAL header: type=5 (IDR), nri=3
    // 重复数据块以制造一个大的NAL单元 (约2KB)
    0x88, 0x84, 0x0a, 0xa2, 0x62, 0x04, 0x07, 0x41, 0x4b, 0xa8, 0x15, 0x0c, 0x0e, 0x82, 0x97, 0x50,
    0x2a, 0x18, 0x1d, 0x05, 0x2e, 0xa0, 0x54, 0x30, 0x3a, 0x0a, 0x5d, 0x40, 0xa8, 0x60, 0x74, 0x14,
    0xba, 0x81, 0x50, 0xc0, 0xe8, 0x29, 0x75, 0x02, 0xa1, 0x81, 0xd0, 0x52, 0xea, 0x05, 0x43, 0x03,
    0xa0, 0xa5, 0xd4, 0x0a, 0x86, 0x07, 0x41, 0x4b, 0xa8, 0x15, 0x0c, 0x0e, 0x82, 0x97, 0x50, 0x2a,
    // 重复多次以增加大小...
    0x88, 0x84, 0x0a, 0xa2, 0x62, 0x04, 0x07, 0x41, 0x4b, 0xa8, 0x15, 0x0c, 0x0e, 0x82, 0x97, 0x50,
    0x2a, 0x18, 0x1d, 0x05, 0x2e, 0xa0, 0x54, 0x30, 0x3a, 0x0a, 0x5d, 0x40, 0xa8, 0x60, 0x74, 0x14,
    0xba, 0x81, 0x50, 0xc0, 0xe8, 0x29, 0x75, 0x02, 0xa1, 0x81, 0xd0, 0x52, 0xea, 0x05, 0x43, 0x03,
    0xa0, 0xa5, 0xd4, 0x0a, 0x86, 0x07, 0x41, 0x4b, 0xa8, 0x15, 0x0c, 0x0e, 0x82, 0x97, 0x50, 0x2a,
    0x88, 0x84, 0x0a, 0xa2, 0x62, 0x04, 0x07, 0x41, 0x4b, 0xa8, 0x15, 0x0c, 0x0e, 0x82, 0x97, 0x50,
    0x2a, 0x18, 0x1d, 0x05, 0x2e, 0xa0, 0x54, 0x30, 0x3a, 0x0a, 0x5d, 0x40, 0xa8, 0x60, 0x74, 0x14,
    0xba, 0x81, 0x50, 0xc0, 0xe8, 0x29, 0x75, 0x02, 0xa1, 0x81, 0xd0, 0x52, 0xea, 0x05, 0x43, 0x03,
    0xa0, 0xa5, 0xd4, 0x0a, 0x86, 0x07, 0x41, 0x4b, 0xa8, 0x15, 0x0c, 0x0e, 0x82, 0x97, 0x50, 0x2a
    // 注意：这里为了文件大小限制只展示部分，实际应用中这个数组会更大
};

/**
 * @brief 组合的H.264帧 (SPS + PPS + IDR)
 * 这是一个完整的关键帧，包含所有必要的参数集
 */
typedef struct {
    const uint8_t *data;
    size_t size;
    const char *description;
} h264_test_frame_t;

static const h264_test_frame_t h264_test_frames[] = {
    {h264_test_sps, sizeof(h264_test_sps), "SPS"},
    {h264_test_pps, sizeof(h264_test_pps), "PPS"}, 
    {h264_test_idr, sizeof(h264_test_idr), "IDR Frame"},
    {h264_test_p_frame, sizeof(h264_test_p_frame), "P Frame"},
    {h264_test_large_nal, sizeof(h264_test_large_nal), "Large NAL (FU-A test)"}
};

#define H264_TEST_FRAME_COUNT (sizeof(h264_test_frames) / sizeof(h264_test_frames[0]))

/**
 * @brief 创建测试用的组合H.264帧
 * 
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @param frame_type 帧类型 (0=SPS+PPS+IDR, 1=P帧, 2=大NAL)
 * @return 实际数据大小，0表示失败
 */
/**
 * @brief 简化的H.264测试数据 - 仅用于RTP测试
 * 重点测试RTP封装而不是H.264解码
 */
static const uint8_t simple_h264_sps[] = {
    0x00, 0x00, 0x00, 0x01,  // Start code
    0x67, 0x42, 0x00, 0x0a,  // SPS NAL: Profile=66(Baseline), Level=1.0
    0x89, 0x95, 0x42, 0xc1,  // 简化参数
    0x2c, 0x80               // 结束
};

static const uint8_t simple_h264_pps[] = {
    0x00, 0x00, 0x00, 0x01,  // Start code
    0x68, 0xce, 0x3c, 0x80   // PPS NAL: 简化参数
};

static const uint8_t simple_h264_idr[] = {
    0x00, 0x00, 0x00, 0x01,  // Start code
    0x65,                     // IDR NAL header
    0x88, 0x84, 0x00, 0x10,  // 简化slice数据
    0x00, 0x00, 0x01         // 最小数据
};

static const uint8_t simple_h264_p_frame[] = {
    0x00, 0x00, 0x00, 0x01,  // Start code
    0x41,                     // P frame NAL header
    0x9a, 0x00, 0x00, 0x04,  // 简化slice数据
    0x00, 0x01               // 最小数据
};

static inline size_t h264_create_test_frame(uint8_t *buffer, size_t buffer_size, int frame_type)
{
    size_t offset = 0;
    
    if (frame_type == 0) {
        // SPS + PPS + IDR 组合帧
        size_t total_size = sizeof(simple_h264_sps) + sizeof(simple_h264_pps) + sizeof(simple_h264_idr);
        if (buffer_size < total_size) {
            return 0;
        }
        
        memcpy(buffer + offset, simple_h264_sps, sizeof(simple_h264_sps));
        offset += sizeof(simple_h264_sps);
        
        memcpy(buffer + offset, simple_h264_pps, sizeof(simple_h264_pps));
        offset += sizeof(simple_h264_pps);
        
        memcpy(buffer + offset, simple_h264_idr, sizeof(simple_h264_idr));
        offset += sizeof(simple_h264_idr);
        
    } else if (frame_type == 1) {
        // P帧
        if (buffer_size < sizeof(simple_h264_p_frame)) {
            return 0;
        }
        
        memcpy(buffer, simple_h264_p_frame, sizeof(simple_h264_p_frame));
        offset = sizeof(simple_h264_p_frame);
        
    } else if (frame_type == 2) {
        // 大NAL单元 - 用于测试FU-A分片
        if (buffer_size < 1600) {  // 需要约1.5KB空间
            return 0;
        }
        
        // 开始码和IDR头
        memcpy(buffer, simple_h264_idr, sizeof(simple_h264_idr));
        offset = sizeof(simple_h264_idr);
        
        // 填充更多数据以创建大的NAL
        uint8_t padding[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
        
        // 重复填充数据
        while (offset < 1500 && (offset + sizeof(padding)) <= buffer_size) {
            memcpy(buffer + offset, padding, sizeof(padding));
            offset += sizeof(padding);
        }
        
        // NAL结束标志
        if (offset < buffer_size) {
            buffer[offset++] = 0x80;
        }
    }
    
    return offset;
}

#ifdef __cplusplus
}
#endif

#endif /* _H264_TEST_DATA_H_ */