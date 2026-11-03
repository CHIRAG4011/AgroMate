#include "SafeJpegEncoder.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"

#define JPGE_MAX(a,b) (((a)>(b))?(a):(b))
#define JPGE_MIN(a,b) (((a)<(b))?(a):(b))

namespace safe_jpge {

    typedef unsigned char  uint8;
    typedef signed short   int16;
    typedef signed int     int32;
    typedef unsigned short uint16;
    typedef unsigned int   uint32;
    typedef unsigned int   uint;

    enum subsampling_t { Y_ONLY = 0, H1V1 = 1, H2V1 = 2, H2V2 = 3 };

    struct params {
        inline params() : m_quality(85), m_subsampling(H2V2) { }
        inline bool check() const {
            return (m_quality >= 1 && m_quality <= 100 && (uint)m_subsampling <= (uint)H2V2);
        }
        int m_quality;
        subsampling_t m_subsampling;
    };

    class output_stream {
    public:
        virtual ~output_stream() { }
        virtual bool put_buf(const void* Pbuf, int len) = 0;
        virtual uint get_size() const = 0;
    };

    class memory_stream : public output_stream {
    protected:
        uint8* out_buf;
        size_t max_len, index;
    public:
        memory_stream(void* pBuf, uint buf_size) : out_buf(static_cast<uint8*>(pBuf)), max_len(buf_size), index(0) { }
        virtual ~memory_stream() { }
        virtual bool put_buf(const void* pBuf, int len) {
            if (!pBuf || len <= 0) return true;
            if ((size_t)len > (max_len - index)) {
                len = max_len - index;
            }
            if (len > 0 && out_buf) {
                memcpy(out_buf + index, pBuf, len);
                index += len;
            }
            return true;
        }
        virtual uint get_size() const { return index; }
    };

    // JPEG enums and tables
    enum { M_SOF0 = 0xC0, M_DHT = 0xC4, M_SOI = 0xD8, M_EOI = 0xD9, M_SOS = 0xDA, M_DQT = 0xDB, M_APP0 = 0xE0 };
    enum { DC_LUM_CODES = 12, AC_LUM_CODES = 256, DC_CHROMA_CODES = 12, AC_CHROMA_CODES = 256 };

    static const uint8 s_zag[64] = {
        0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,35,42,49,56,
        57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
    };
    static const int16 s_std_lum_quant[64] = {
        16,11,12,14,12,10,16,14,13,14,18,17,16,19,24,40,26,24,22,22,24,49,35,37,29,40,58,51,61,60,57,51,
        56,55,64,72,92,78,64,68,87,69,55,56,80,109,81,87,95,98,103,104,103,62,77,113,121,112,100,120,92,101,103,99
    };
    static const int16 s_std_croma_quant[64] = {
        17,18,18,24,21,24,47,26,26,47,99,66,56,66,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,
        99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99
    };
    static const uint8 s_dc_lum_bits[17] = { 0,0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0 };
    static const uint8 s_dc_lum_val[DC_LUM_CODES] = { 0,1,2,3,4,5,6,7,8,9,10,11 };
    static const uint8 s_ac_lum_bits[17] = { 0,0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d };
    static const uint8 s_ac_lum_val[AC_LUM_CODES]  = {
        0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,
        0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
        0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
        0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,
        0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
        0xf9,0xfa
    };
    static const uint8 s_dc_chroma_bits[17] = { 0,0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0 };
    static const uint8 s_dc_chroma_val[DC_CHROMA_CODES]  = { 0,1,2,3,4,5,6,7,8,9,10,11 };
    static const uint8 s_ac_chroma_bits[17] = { 0,0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,0x77 };
    static const uint8 s_ac_chroma_val[AC_CHROMA_CODES] = {
        0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,
        0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,
        0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,
        0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,
        0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
        0xf9,0xfa
    };

    const int YR = 19595, YG = 38470, YB = 7471, CB_R = -11059, CB_G = -21709, CB_B = 32768, CR_R = 32768, CR_G = -27439, CR_B = -5329;

    static inline uint8 clamp_u8(int i) {
        if (i < 0) return 0;
        if (i > 255) return 255;
        return (uint8)i;
    }

    static void RGB_to_YCC(uint8* pDst, const uint8 *pSrc, int num_pixels) {
        for ( ; num_pixels; pDst += 3, pSrc += 3, num_pixels--) {
            const int r = pSrc[0], g = pSrc[1], b = pSrc[2];
            pDst[0] = (uint8)((r * YR + g * YG + b * YB + 32768) >> 16);
            pDst[1] = clamp_u8(128 + ((r * CB_R + g * CB_G + b * CB_B + 32768) >> 16));
            pDst[2] = clamp_u8(128 + ((r * CR_R + g * CR_G + b * CR_B + 32768) >> 16));
        }
    }

    enum { CONST_BITS = 13, ROW_BITS = 2 };
    #define DCT_DESCALE(x, n) (((x) + (((int32)1) << ((n) - 1))) >> (n))
    #define DCT_MUL(var, c) (static_cast<int16>(var) * static_cast<int32>(c))
    #define DCT1D(s0, s1, s2, s3, s4, s5, s6, s7) \
        int32 t0 = s0 + s7, t7 = s0 - s7, t1 = s1 + s6, t6 = s1 - s6, t2 = s2 + s5, t5 = s2 - s5, t3 = s3 + s4, t4 = s3 - s4; \
        int32 t10 = t0 + t3, t13 = t0 - t3, t11 = t1 + t2, t12 = t1 - t2; \
        int32 u1 = DCT_MUL(t12 + t13, 4433); \
        s2 = u1 + DCT_MUL(t13, 6270); \
        s6 = u1 + DCT_MUL(t12, -15137); \
        u1 = t4 + t7; \
        int32 u2 = t5 + t6, u3 = t4 + t6, u4 = t5 + t7; \
        int32 z5 = DCT_MUL(u3 + u4, 9633); \
        t4 = DCT_MUL(t4, 2446); t5 = DCT_MUL(t5, 16819); \
        t6 = DCT_MUL(t6, 25172); t7 = DCT_MUL(t7, 12299); \
        u1 = DCT_MUL(u1, -7373); u2 = DCT_MUL(u2, -20995); \
        u3 = DCT_MUL(u3, -16069); u4 = DCT_MUL(u4, -3196); \
        u3 += z5; u4 += z5; \
        s0 = t10 + t11; s1 = t7 + u1 + u4; s3 = t6 + u2 + u3; s4 = t10 - t11; s5 = t5 + u2 + u4; s7 = t4 + u1 + u3;

    static void DCT2D(int32 *p) {
        int32 c, *q = p;
        for (c = 7; c >= 0; c--, q += 8) {
            int32 s0 = q[0], s1 = q[1], s2 = q[2], s3 = q[3], s4 = q[4], s5 = q[5], s6 = q[6], s7 = q[7];
            DCT1D(s0, s1, s2, s3, s4, s5, s6, s7);
            q[0] = s0 << ROW_BITS; q[1] = DCT_DESCALE(s1, CONST_BITS-ROW_BITS); q[2] = DCT_DESCALE(s2, CONST_BITS-ROW_BITS); q[3] = DCT_DESCALE(s3, CONST_BITS-ROW_BITS);
            q[4] = s4 << ROW_BITS; q[5] = DCT_DESCALE(s5, CONST_BITS-ROW_BITS); q[6] = DCT_DESCALE(s6, CONST_BITS-ROW_BITS); q[7] = DCT_DESCALE(s7, CONST_BITS-ROW_BITS);
        }
        for (q = p, c = 7; c >= 0; c--, q++) {
            int32 s0 = q[0*8], s1 = q[1*8], s2 = q[2*8], s3 = q[3*8], s4 = q[4*8], s5 = q[5*8], s6 = q[6*8], s7 = q[7*8];
            DCT1D(s0, s1, s2, s3, s4, s5, s6, s7);
            q[0*8] = DCT_DESCALE(s0, ROW_BITS+3); q[1*8] = DCT_DESCALE(s1, CONST_BITS+ROW_BITS+3); q[2*8] = DCT_DESCALE(s2, CONST_BITS+ROW_BITS+3); q[3*8] = DCT_DESCALE(s3, CONST_BITS+ROW_BITS+3);
            q[4*8] = DCT_DESCALE(s4, ROW_BITS+3); q[5*8] = DCT_DESCALE(s5, CONST_BITS+ROW_BITS+3); q[6*8] = DCT_DESCALE(s6, CONST_BITS+ROW_BITS+3); q[7*8] = DCT_DESCALE(s7, CONST_BITS+ROW_BITS+3);
        }
    }

    static void compute_huffman_table(uint *codes, uint8 *code_sizes, const uint8 *bits, const uint8 *val) {
        int i, l, last_p, si;
        uint8 huff_size[257];
        uint huff_code[257];
        uint code;

        int p = 0;
        for (l = 1; l <= 16; l++) {
            for (i = 1; i <= bits[l]; i++) {
                huff_size[p++] = (uint8)l;
            }
        }
        huff_size[p] = 0;
        last_p = p;

        code = 0; si = huff_size[0]; p = 0;
        while (huff_size[p]) {
            while (huff_size[p] == si) {
                huff_code[p++] = code++;
            }
            code <<= 1;
            si++;
        }

        memset(codes, 0, sizeof(codes[0])*256);
        memset(code_sizes, 0, sizeof(code_sizes[0])*256);
        for (p = 0; p < last_p; p++) {
            codes[val[p]]      = huff_code[p];
            code_sizes[val[p]] = huff_size[p];
        }
    }

    class jpeg_encoder {
    public:
        jpeg_encoder();
        ~jpeg_encoder();
        bool init(output_stream *pStream, int width, int height, int src_channels, const params &comp_params);
        bool process_scanline(const void* pScanline);
        void deinit();

    private:
        typedef int32 sample_array_t;
        enum { JPGE_OUT_BUF_SIZE = 512 };

        output_stream *m_pStream;
        params m_params;
        uint8 m_num_components;
        uint8 m_comp_h_samp[3], m_comp_v_samp[3];
        int m_image_x, m_image_y, m_image_bpp;
        int m_image_x_mcu, m_image_y_mcu;
        int m_image_bpl_xlt, m_image_bpl_mcu;
        int m_mcus_per_row;
        int m_mcu_x, m_mcu_y;
        uint8 *m_mcu_lines[16];
        uint8 m_mcu_y_ofs;
        sample_array_t m_sample_array[64];
        int16 m_coefficient_array[64];
        int32 m_quantization_tables[2][64];

        uint m_huff_codes[4][256];
        uint8 m_huff_code_sizes[4][256];

        int m_last_dc_val[3];
        uint8 m_out_buf[JPGE_OUT_BUF_SIZE];
        uint8 *m_pOut_buf;
        uint m_out_buf_left;
        uint32 m_bit_buffer;
        uint m_bits_in;
        uint8 m_pass_num;
        bool m_all_stream_writes_succeeded;

        bool jpg_open(int p_x_res, int p_y_res, int src_channels);
        void flush_output_buffer();
        void put_bits(uint bits, uint len);
        void emit_byte(uint8 i);
        void emit_word(uint i);
        void emit_marker(int marker);
        void emit_jfif_app0();
        void emit_dqt();
        void emit_sof();
        void emit_dht(const uint8 *bits, const uint8 *val, int index, bool ac_flag);
        void emit_dhts();
        void emit_sos();
        void compute_quant_table(int32 *pDst, const int16 *pSrc);
        void load_block_8_8(int x, int y, int c);
        void load_block_16_8(int x, int c);
        void load_quantized_coefficients(int component_num);
        void code_coefficients_pass_two(int component_num);
        void code_block(int component_num);
        void process_mcu_row();
        void load_mcu(const void *pSrc);
        bool process_end_of_image();
        void clear();
    };

    jpeg_encoder::jpeg_encoder() { clear(); }
    jpeg_encoder::~jpeg_encoder() { deinit(); }

    void jpeg_encoder::clear() {
        m_mcu_lines[0] = NULL;
        m_pass_num = 0;
        m_all_stream_writes_succeeded = true;
    }

    void jpeg_encoder::deinit() {
        if (m_mcu_lines[0]) {
            free(m_mcu_lines[0]);
            m_mcu_lines[0] = NULL;
        }
        clear();
    }

    void jpeg_encoder::flush_output_buffer() {
        if (m_out_buf_left != JPGE_OUT_BUF_SIZE) {
            m_all_stream_writes_succeeded = m_all_stream_writes_succeeded && m_pStream->put_buf(m_out_buf, JPGE_OUT_BUF_SIZE - m_out_buf_left);
        }
        m_pOut_buf = m_out_buf;
        m_out_buf_left = JPGE_OUT_BUF_SIZE;
    }

    void jpeg_encoder::emit_byte(uint8 i) {
        *m_pOut_buf++ = i;
        if (--m_out_buf_left == 0) {
            flush_output_buffer();
        }
    }

    void jpeg_encoder::emit_word(uint i) {
        emit_byte((uint8)(i >> 8));
        emit_byte((uint8)(i & 0xFF));
    }

    void jpeg_encoder::emit_marker(int marker) {
        emit_byte(0xFF);
        emit_byte((uint8)marker);
    }

    void jpeg_encoder::put_bits(uint bits, uint len) {
        m_bit_buffer |= ((uint32)bits << (24 - (m_bits_in += len)));
        while (m_bits_in >= 8) {
            uint8 c = (uint8)((m_bit_buffer >> 16) & 0xFF);
            emit_byte(c);
            if (c == 0xFF) emit_byte(0);
            m_bit_buffer <<= 8;
            m_bits_in -= 8;
        }
    }

    void jpeg_encoder::emit_jfif_app0() {
        emit_marker(M_APP0);
        emit_word(2 + 4 + 1 + 2 + 1 + 2 + 2 + 1 + 1);
        emit_byte(0x4A); emit_byte(0x46); emit_byte(0x49); emit_byte(0x46);
        emit_byte(0); emit_byte(1); emit_byte(1); emit_byte(0);
        emit_word(1); emit_word(1); emit_byte(0); emit_byte(0);
    }

    void jpeg_encoder::emit_dqt() {
        for (int i = 0; i < ((m_num_components == 3) ? 2 : 1); i++) {
            emit_marker(M_DQT);
            emit_word(64 + 1 + 2);
            emit_byte((uint8)i);
            for (int j = 0; j < 64; j++)
                emit_byte((uint8)m_quantization_tables[i][j]);
        }
    }

    void jpeg_encoder::emit_sof() {
        emit_marker(M_SOF0);
        emit_word(3 * m_num_components + 2 + 5 + 1);
        emit_byte(8);
        emit_word(m_image_y);
        emit_word(m_image_x);
        emit_byte(m_num_components);
        for (int i = 0; i < m_num_components; i++) {
            emit_byte((uint8)(i + 1));
            emit_byte((m_comp_h_samp[i] << 4) + m_comp_v_samp[i]);
            emit_byte(i > 0);
        }
    }

    void jpeg_encoder::emit_dht(const uint8 *bits, const uint8 *val, int index, bool ac_flag) {
        emit_marker(M_DHT);
        int length = 0;
        for (int i = 1; i <= 16; i++) length += bits[i];
        emit_word(length + 2 + 1 + 16);
        emit_byte((uint8)(index + (ac_flag << 4)));
        for (int i = 1; i <= 16; i++) emit_byte(bits[i]);
        for (int i = 0; i < length; i++) emit_byte(val[i]);
    }

    void jpeg_encoder::emit_dhts() {
        emit_dht(s_dc_lum_bits, s_dc_lum_val, 0, false);
        emit_dht(s_ac_lum_bits, s_ac_lum_val, 0, true);
        if (m_num_components == 3) {
            emit_dht(s_dc_chroma_bits, s_dc_chroma_val, 1, false);
            emit_dht(s_ac_chroma_bits, s_ac_chroma_val, 1, true);
        }
    }

    void jpeg_encoder::emit_sos() {
        emit_marker(M_SOS);
        emit_word(2 * m_num_components + 2 + 1 + 3);
        emit_byte(m_num_components);
        for (int i = 0; i < m_num_components; i++) {
            emit_byte((uint8)(i + 1));
            emit_byte(i == 0 ? 0 : (1 << 4) + 1);
        }
        emit_byte(0); emit_byte(63); emit_byte(0);
    }

    void jpeg_encoder::compute_quant_table(int32 *pDst, const int16 *pSrc) {
        int32 q = (m_params.m_quality < 50) ? (5000 / m_params.m_quality) : (200 - m_params.m_quality * 2);
        for (int i = 0; i < 64; i++) {
            int32 j = *pSrc++; j = (j * q + 50L) / 100L;
            *pDst++ = JPGE_MIN(JPGE_MAX(j, 1), 255);
        }
    }

    void jpeg_encoder::load_block_8_8(int x, int y, int c) {
        uint8 *pSrc;
        sample_array_t *pDst = m_sample_array;
        x = (x * (8 * 3)) + c;
        y <<= 3;
        for (int i = 0; i < 8; i++, pDst += 8) {
            pSrc = m_mcu_lines[y + i] + x;
            pDst[0] = pSrc[0 * 3] - 128; pDst[1] = pSrc[1 * 3] - 128; pDst[2] = pSrc[2 * 3] - 128; pDst[3] = pSrc[3 * 3] - 128;
            pDst[4] = pSrc[4 * 3] - 128; pDst[5] = pSrc[5 * 3] - 128; pDst[6] = pSrc[6 * 3] - 128; pDst[7] = pSrc[7 * 3] - 128;
        }
    }

    void jpeg_encoder::load_block_16_8(int x, int c) {
        uint8 *pSrc1, *pSrc2;
        sample_array_t *pDst = m_sample_array;
        x = (x * (16 * 3)) + c;
        int a = 0, b = 2;
        for (int i = 0; i < 16; i += 2, pDst += 8) {
            pSrc1 = m_mcu_lines[i + 0] + x;
            pSrc2 = m_mcu_lines[i + 1] + x;
            pDst[0] = ((pSrc1[ 0 * 3] + pSrc1[ 1 * 3] + pSrc2[ 0 * 3] + pSrc2[ 1 * 3] + a) >> 2) - 128;
            pDst[1] = ((pSrc1[ 2 * 3] + pSrc1[ 3 * 3] + pSrc2[ 2 * 3] + pSrc2[ 3 * 3] + b) >> 2) - 128;
            pDst[2] = ((pSrc1[ 4 * 3] + pSrc1[ 5 * 3] + pSrc2[ 4 * 3] + pSrc2[ 5 * 3] + a) >> 2) - 128;
            pDst[3] = ((pSrc1[ 6 * 3] + pSrc1[ 7 * 3] + pSrc2[ 6 * 3] + pSrc2[ 7 * 3] + b) >> 2) - 128;
            pDst[4] = ((pSrc1[ 8 * 3] + pSrc1[ 9 * 3] + pSrc2[ 8 * 3] + pSrc2[ 9 * 3] + a) >> 2) - 128;
            pDst[5] = ((pSrc1[10 * 3] + pSrc1[11 * 3] + pSrc2[10 * 3] + pSrc2[11 * 3] + b) >> 2) - 128;
            pDst[6] = ((pSrc1[12 * 3] + pSrc1[13 * 3] + pSrc2[12 * 3] + pSrc2[13 * 3] + a) >> 2) - 128;
            pDst[7] = ((pSrc1[14 * 3] + pSrc1[15 * 3] + pSrc2[14 * 3] + pSrc2[15 * 3] + b) >> 2) - 128;
            int temp = a; a = b; b = temp;
        }
    }

    void jpeg_encoder::load_quantized_coefficients(int component_num) {
        int32 *q = m_quantization_tables[component_num > 0];
        int16 *pDst = m_coefficient_array;
        for (int i = 0; i < 64; i++) {
            sample_array_t j = m_sample_array[s_zag[i]];
            if (j < 0) {
                if ((j = -j + (*q >> 1)) < *q) *pDst++ = 0;
                else *pDst++ = (int16)(-(j / *q));
            } else {
                if ((j = j + (*q >> 1)) < *q) *pDst++ = 0;
                else *pDst++ = (int16)(j / *q);
            }
            q++;
        }
    }

    void jpeg_encoder::code_coefficients_pass_two(int component_num) {
        int i, j, run_len, nbits, temp1, temp2;
        int16 *pSrc = m_coefficient_array;
        uint *codes[2];
        uint8 *code_sizes[2];

        if (component_num == 0) {
            codes[0] = m_huff_codes[0 + 0]; codes[1] = m_huff_codes[2 + 0];
            code_sizes[0] = m_huff_code_sizes[0 + 0]; code_sizes[1] = m_huff_code_sizes[2 + 0];
        } else {
            codes[0] = m_huff_codes[0 + 1]; codes[1] = m_huff_codes[2 + 1];
            code_sizes[0] = m_huff_code_sizes[0 + 1]; code_sizes[1] = m_huff_code_sizes[2 + 1];
        }

        temp1 = temp2 = pSrc[0] - m_last_dc_val[component_num];
        m_last_dc_val[component_num] = pSrc[0];
        if (temp1 < 0) { temp1 = -temp1; temp2--; }
        nbits = 0;
        while (temp1) { nbits++; temp1 >>= 1; }

        put_bits(codes[0][nbits], code_sizes[0][nbits]);
        if (nbits) put_bits(temp2 & ((1 << nbits) - 1), nbits);

        for (run_len = 0, i = 1; i < 64; i++) {
            if ((temp1 = m_coefficient_array[i]) == 0) {
                run_len++;
            } else {
                while (run_len >= 16) {
                    put_bits(codes[1][0xF0], code_sizes[1][0xF0]);
                    run_len -= 16;
                }
                if ((temp2 = temp1) < 0) { temp1 = -temp1; temp2--; }
                nbits = 1;
                while (temp1 >>= 1) nbits++;
                j = (run_len << 4) + nbits;
                put_bits(codes[1][j], code_sizes[1][j]);
                put_bits(temp2 & ((1 << nbits) - 1), nbits);
                run_len = 0;
            }
        }
        if (run_len) put_bits(codes[1][0], code_sizes[1][0]);
    }

    void jpeg_encoder::code_block(int component_num) {
        DCT2D(m_sample_array);
        load_quantized_coefficients(component_num);
        code_coefficients_pass_two(component_num);
    }

    void jpeg_encoder::process_mcu_row() {
        for (int i = 0; i < m_mcus_per_row; i++) {
            load_block_8_8(i * 2 + 0, 0, 0); code_block(0);
            load_block_8_8(i * 2 + 1, 0, 0); code_block(0);
            load_block_8_8(i * 2 + 0, 1, 0); code_block(0);
            load_block_8_8(i * 2 + 1, 1, 0); code_block(0);
            load_block_16_8(i, 1); code_block(1);
            load_block_16_8(i, 2); code_block(2);
        }
    }

    void jpeg_encoder::load_mcu(const void *pSrc) {
        const uint8* Psrc = reinterpret_cast<const uint8*>(pSrc);
        uint8* pDst = m_mcu_lines[m_mcu_y_ofs];
        RGB_to_YCC(pDst, Psrc, m_image_x);

        // Pad scanline if not multiple of 16
        const uint8 y = pDst[m_image_bpl_xlt - 3 + 0], cb = pDst[m_image_bpl_xlt - 3 + 1], cr = pDst[m_image_bpl_xlt - 3 + 2];
        uint8 *q = m_mcu_lines[m_mcu_y_ofs] + m_image_bpl_xlt;
        for (int i = m_image_x; i < m_image_x_mcu; i++) {
            *q++ = y; *q++ = cb; *q++ = cr;
        }

        if (++m_mcu_y_ofs == m_mcu_y) {
            process_mcu_row();
            m_mcu_y_ofs = 0;
        }
    }

    bool jpeg_encoder::jpg_open(int p_x_res, int p_y_res, int src_channels) {
        m_num_components = 3;
        m_comp_h_samp[0] = 2; m_comp_v_samp[0] = 2;
        m_comp_h_samp[1] = 1; m_comp_v_samp[1] = 1;
        m_comp_h_samp[2] = 1; m_comp_v_samp[2] = 1;
        m_mcu_x = 16; m_mcu_y = 16;

        m_image_x = p_x_res; m_image_y = p_y_res;
        m_image_bpp = src_channels;
        m_image_x_mcu = (m_image_x + m_mcu_x - 1) & (~(m_mcu_x - 1));
        m_image_y_mcu = (m_image_y + m_mcu_y - 1) & (~(m_mcu_y - 1));
        m_image_bpl_xlt = m_image_x * m_num_components;
        m_image_bpl_mcu = m_image_x_mcu * m_num_components;
        m_mcus_per_row = m_image_x_mcu / m_mcu_x;

        // CRITICAL FIX: Allocate m_mcu_lines exclusively in INTERNAL DRAM (MALLOC_CAP_INTERNAL).
        // This completely eliminates StoreProhibited exceptions caused by 8-bit stores to PSRAM.
        size_t mcuAllocSize = m_image_bpl_mcu * m_mcu_y;
        m_mcu_lines[0] = static_cast<uint8*>(heap_caps_malloc(mcuAllocSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        if (!m_mcu_lines[0]) {
            return false;
        }
        for (int i = 1; i < m_mcu_y; i++) {
            m_mcu_lines[i] = m_mcu_lines[i-1] + m_image_bpl_mcu;
        }

        compute_quant_table(m_quantization_tables[0], s_std_lum_quant);
        compute_quant_table(m_quantization_tables[1], s_std_croma_quant);

        compute_huffman_table(&m_huff_codes[0+0][0], &m_huff_code_sizes[0+0][0], s_dc_lum_bits, s_dc_lum_val);
        compute_huffman_table(&m_huff_codes[2+0][0], &m_huff_code_sizes[2+0][0], s_ac_lum_bits, s_ac_lum_val);
        compute_huffman_table(&m_huff_codes[0+1][0], &m_huff_code_sizes[0+1][0], s_dc_chroma_bits, s_dc_chroma_val);
        compute_huffman_table(&m_huff_codes[2+1][0], &m_huff_code_sizes[2+1][0], s_ac_chroma_bits, s_ac_chroma_val);

        m_out_buf_left = JPGE_OUT_BUF_SIZE;
        m_pOut_buf = m_out_buf;
        m_bit_buffer = 0;
        m_bits_in = 0;
        m_mcu_y_ofs = 0;
        m_pass_num = 2;
        memset(m_last_dc_val, 0, sizeof(m_last_dc_val));

        emit_marker(M_SOI);
        emit_jfif_app0();
        emit_dqt();
        emit_sof();
        emit_dhts();
        emit_sos();

        return m_all_stream_writes_succeeded;
    }

    bool jpeg_encoder::process_end_of_image() {
        if (m_mcu_y_ofs) {
            for (int i = m_mcu_y_ofs; i < m_mcu_y; i++) {
                memcpy(m_mcu_lines[i], m_mcu_lines[m_mcu_y_ofs - 1], m_image_bpl_mcu);
            }
            process_mcu_row();
        }
        put_bits(0x7F, 7);
        emit_marker(M_EOI);
        flush_output_buffer();
        m_pass_num++;
        return m_all_stream_writes_succeeded;
    }

    bool jpeg_encoder::init(output_stream *pStream, int width, int height, int src_channels, const params &comp_params) {
        deinit();
        if (!pStream || width < 1 || height < 1 || src_channels != 3 || !comp_params.check()) return false;
        m_pStream = pStream;
        m_params = comp_params;
        return jpg_open(width, height, src_channels);
    }

    bool jpeg_encoder::process_scanline(const void* pScanline) {
        if (m_pass_num < 1 || m_pass_num > 2) return false;
        if (m_all_stream_writes_succeeded) {
            if (!pScanline) {
                return process_end_of_image();
            } else {
                load_mcu(pScanline);
            }
        }
        return m_all_stream_writes_succeeded;
    }

} // namespace safe_jpge

// ──────────────────────────────────────────────────────────────────────────────
//  Public API: encodeYUV422ToJpeg
// ──────────────────────────────────────────────────────────────────────────────
bool encodeYUV422ToJpeg(const uint8_t* yuv422, int width, int height, int quality, uint8_t** outJpg, size_t* outLen) {
    if (!yuv422 || width <= 0 || height <= 0 || !outJpg || !outLen) return false;
    *outJpg = nullptr;
    *outLen = 0;

    // Allocate intermediate scanline buffer in INTERNAL DRAM (640 * 3 = 1920 bytes)
    size_t lineBytes = width * 3;
    uint8_t* scanline = (uint8_t*)heap_caps_malloc(lineBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!scanline) {
        return false;
    }

    // Allocate output JPEG buffer in internal DRAM (32 KB easily holds QQVGA at quality 85)
    size_t outCap = (width <= 320) ? 32768 : 131072;
    uint8_t* outBuf = (uint8_t*)heap_caps_malloc(outCap, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!outBuf) {
        outBuf = (uint8_t*)ps_malloc(outCap);
        if (!outBuf) {
            outCap = 65536;
            outBuf = (uint8_t*)heap_caps_malloc(outCap, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (!outBuf) {
                free(scanline);
                return false;
            }
        }
    }

    safe_jpge::memory_stream memStream(outBuf, outCap);
    safe_jpge::params p;
    p.m_quality = quality;
    p.m_subsampling = safe_jpge::H2V2;

    safe_jpge::jpeg_encoder* encoder = new (std::nothrow) safe_jpge::jpeg_encoder();
    if (!encoder) {
        free(scanline);
        free(outBuf);
        return false;
    }
    if (!encoder->init(&memStream, width, height, 3, p)) {
        delete encoder;
        free(scanline);
        free(outBuf);
        return false;
    }

    for (int y = 0; y < height; y++) {
        const uint8_t* row = yuv422 + (y * width * 2);
        // Fast YUV422 (Y0, Cb, Y1, Cr) -> RGB888 in internal DRAM scanline
        int o = 0;
        for (int x = 0; x < width * 2; x += 4) {
            int32_t y0 = row[x + 0];
            int32_t u  = (int32_t)row[x + 1] - 128;
            int32_t y1 = row[x + 2];
            int32_t v  = (int32_t)row[x + 3] - 128;

            int32_t r_diff = (359 * v + 128) >> 8;
            int32_t g_diff = (88 * u + 183 * v + 128) >> 8;
            int32_t b_diff = (454 * u + 128) >> 8;

            scanline[o++] = safe_jpge::clamp_u8(y0 + r_diff);
            scanline[o++] = safe_jpge::clamp_u8(y0 - g_diff);
            scanline[o++] = safe_jpge::clamp_u8(y0 + b_diff);

            scanline[o++] = safe_jpge::clamp_u8(y1 + r_diff);
            scanline[o++] = safe_jpge::clamp_u8(y1 - g_diff);
            scanline[o++] = safe_jpge::clamp_u8(y1 + b_diff);
        }

        if (!encoder->process_scanline(scanline)) {
            delete encoder;
            free(scanline);
            free(outBuf);
            return false;
        }
    }

    // End of image flush
    if (!encoder->process_scanline(nullptr)) {
        delete encoder;
        free(scanline);
        free(outBuf);
        return false;
    }

    delete encoder;
    free(scanline);

    *outJpg = outBuf;
    *outLen = memStream.get_size();
    return (*outLen > 0);
}

// ──────────────────────────────────────────────────────────────────────────────
//  Public API: encodeRGB565ToJpeg
// ──────────────────────────────────────────────────────────────────────────────
bool encodeRGB565ToJpeg(const uint8_t* rgb565, int width, int height, int quality, uint8_t** outJpg, size_t* outLen) {
    if (!rgb565 || width <= 0 || height <= 0 || !outJpg || !outLen) return false;
    *outJpg = nullptr;
    *outLen = 0;

    // Allocate intermediate scanline buffer in INTERNAL DRAM (320 * 3 = 960 bytes)
    size_t lineBytes = width * 3;
    uint8_t* scanline = (uint8_t*)heap_caps_malloc(lineBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!scanline) {
        return false;
    }

    // Allocate output JPEG buffer in external PSRAM (128 KB easily holds VGA 640x480 JPEG at quality 75)
    size_t outCap = 131072;
    uint8_t* outBuf = (uint8_t*)ps_malloc(outCap);
    if (!outBuf) {
        // Fallback: try 80 KB internal DRAM
        outCap = 81920;
        outBuf = (uint8_t*)heap_caps_malloc(outCap, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!outBuf) {
            outCap = 65536;
            outBuf = (uint8_t*)heap_caps_malloc(outCap, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (!outBuf) {
                free(scanline);
                return false;
            }
        }
    }

    safe_jpge::memory_stream memStream(outBuf, outCap);
    safe_jpge::params p;
    p.m_quality = quality;
    p.m_subsampling = safe_jpge::H2V2;

    safe_jpge::jpeg_encoder* encoder = new (std::nothrow) safe_jpge::jpeg_encoder();
    if (!encoder) {
        free(scanline);
        free(outBuf);
        return false;
    }
    if (!encoder->init(&memStream, width, height, 3, p)) {
        delete encoder;
        free(scanline);
        free(outBuf);
        return false;
    }

    for (int y = 0; y < height; y++) {
        const uint8_t* row = rgb565 + (y * width * 2);
        // Fast RGB565 (big endian from DVP bus) -> RGB888 in internal DRAM scanline
        int o = 0;
        for (int x = 0; x < width * 2; x += 2) {
            scanline[o++] = row[x] & 0xF8;
            scanline[o++] = (uint8_t)(((row[x] & 0x07) << 5) | ((row[x+1] & 0xE0) >> 3));
            scanline[o++] = (uint8_t)((row[x+1] & 0x1F) << 3);
        }

        if (!encoder->process_scanline(scanline)) {
            delete encoder;
            free(scanline);
            free(outBuf);
            return false;
        }
    }

    // End of image flush
    if (!encoder->process_scanline(nullptr)) {
        delete encoder;
        free(scanline);
        free(outBuf);
        return false;
    }

    delete encoder;
    free(scanline);

    *outJpg = outBuf;
    *outLen = memStream.get_size();
    return (*outLen > 0);
}
