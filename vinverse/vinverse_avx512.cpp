#include "vinverse.h"
#include "VCL2/vectorclass.h"

void vertical_blur3_avx512_8(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const uint8_t* srcp = reinterpret_cast<const uint8_t*>(srcp_);
    uint8_t* __restrict dstp = reinterpret_cast<uint8_t*>(dstp_);

    int mod64_width = (width + 63) & ~63;

    auto two = Vec32us(2);

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcpp = y == 0 ? srcp + src_pitch : srcp - src_pitch;
        const uint8_t* srcpn = y == height - 1 ? srcp - src_pitch : srcp + src_pitch;

        for (int x = 0; x < mod64_width; x += 64)
        {
            auto p = Vec64uc().load(srcpp + x);
            auto c = Vec64uc().load(srcp + x);
            auto n = Vec64uc().load(srcpn + x);

            auto p_lo = extend_low(p);
            auto p_hi = extend_high(p);
            auto c_lo = extend_low(c);
            auto c_hi = extend_high(c);
            auto n_lo = extend_low(n);
            auto n_hi = extend_high(n);

            auto acc_lo = c_lo + p_lo;
            auto acc_hi = c_hi + p_hi;

            acc_lo = acc_lo + c_lo;
            acc_hi = acc_hi + c_hi;

            acc_lo = acc_lo + n_lo;
            acc_hi = acc_hi + n_hi;

            acc_lo = acc_lo + two;
            acc_hi = acc_hi + two;

            acc_lo = acc_lo >> 2;
            acc_hi = acc_hi >> 2;

            auto dst = compress_saturated(acc_lo, acc_hi);
            dst.store(dstp + x);
        }

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

void vertical_blur5_avx512_8(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const uint8_t* srcp = reinterpret_cast<const uint8_t*>(srcp_);
    uint8_t* __restrict dstp = reinterpret_cast<uint8_t*>(dstp_);

    int mod64_width = (width + 63) & ~63;

    auto six = Vec32us(6);
    auto eight = Vec32us(8);

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcppp = y < 2 ? srcp + src_pitch * 2 : srcp - src_pitch * 2;
        const uint8_t* srcpp = y == 0 ? srcp + src_pitch : srcp - src_pitch;
        const uint8_t* srcpn = y == height - 1 ? srcp - src_pitch : srcp + src_pitch;
        const uint8_t* srcpnn = y > height - 3 ? srcp - src_pitch * 2 : srcp + src_pitch * 2;

        for (int x = 0; x < mod64_width; x += 64)
        {
            auto p2 = Vec64uc().load(srcppp + x);
            auto p1 = Vec64uc().load(srcpp + x);
            auto c = Vec64uc().load(srcp + x);
            auto n1 = Vec64uc().load(srcpn + x);
            auto n2 = Vec64uc().load(srcpnn + x);

            auto p2_lo = extend_low(p2);
            auto p2_hi = extend_high(p2);
            auto p1_lo = extend_low(p1);
            auto p1_hi = extend_high(p1);
            auto c_lo = extend_low(c);
            auto c_hi = extend_high(c);
            auto n1_lo = extend_low(n1);
            auto n1_hi = extend_high(n1);
            auto n2_lo = extend_low(n2);
            auto n2_hi = extend_high(n2);

            auto acc_lo = c_lo * six;
            auto acc_hi = c_hi * six;

            auto t_lo = p1_lo + n1_lo;
            auto t_hi = p1_hi + n1_hi;

            acc_lo = acc_lo + n2_lo;
            acc_hi = acc_hi + n2_hi;

            t_lo = t_lo << 2;
            t_hi = t_hi << 2;

            t_lo = t_lo + p2_lo;
            t_hi = t_hi + p2_hi;

            acc_lo = acc_lo + eight;
            acc_hi = acc_hi + eight;

            acc_lo = acc_lo + t_lo;
            acc_hi = acc_hi + t_hi;

            acc_lo = acc_lo >> 4;
            acc_hi = acc_hi >> 4;

            auto dst = compress_saturated(acc_lo, acc_hi);
            dst.store(dstp + x);
        }

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

static void mt_makediff_avx512_8(void* __restrict dstp_, const void* c1p_, const void* c2p_, int dst_pitch, int c1_pitch, int c2_pitch, int width, int height) noexcept
{
    const uint8_t* c1p = reinterpret_cast<const uint8_t*>(c1p_);
    const uint8_t* c2p = reinterpret_cast<const uint8_t*>(c2p_);
    uint8_t* __restrict dstp = reinterpret_cast<uint8_t*>(dstp_);

    int mod64_width = (width + 63) & ~63;

    auto v128 = Vec64uc(0x80808080);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < mod64_width; x += 64)
        {
            auto c1 = Vec64uc().load(c1p + x);
            auto c2 = Vec64uc().load(c2p + x);

            c1 = c1 - v128;
            c2 = c2 - v128;

            auto diff = c1 - c2;
            diff = diff + v128;
            diff.store(dstp + x);
        }

        dstp += dst_pitch;
        c1p += c1_pitch;
        c2p += c2_pitch;
    }
}

void vertical_sbr_avx512_8(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept
{
    vertical_blur3_avx512_8(tempp_, srcp_, temp_pitch, src_pitch, width, height); //temp = rg11
    mt_makediff_avx512_8(dstp_, srcp_, tempp_, dst_pitch, src_pitch, temp_pitch, width, height); //dst = rg11D
    vertical_blur3_avx512_8(tempp_, dstp_, temp_pitch, dst_pitch, width, height); //temp = rg11D.vblur()

    const uint8_t* srcp = reinterpret_cast<const uint8_t*>(srcp_);
    uint8_t* __restrict tempp = reinterpret_cast<uint8_t*>(tempp_);
    uint8_t* __restrict dstp = reinterpret_cast<uint8_t*>(dstp_);

    int mod64_width = (width + 63) & ~63;

    Vec32s zero = zero_si512();
    auto v128 = Vec32us(128);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < mod64_width; x += 64)
        {
            auto dst_lo = extend_low(Vec64uc().load(dstp + x));
            auto temp_lo = extend_low(Vec64uc().load(tempp + x));
            auto src_lo = extend_low(Vec64uc().load(srcp + x));

            auto t_lo = dst_lo - temp_lo;
            auto t2_lo = dst_lo - v128;

            auto nochange_mask_lo = Vec32s(t_lo * t2_lo) < zero;

            auto t_mask_lo = abs(t_lo) < abs(t2_lo);
            auto desired_lo = src_lo - t_lo;
            auto otherwise_lo = (src_lo - dst_lo) + v128;
            auto result_lo = select(nochange_mask_lo, src_lo, select(t_mask_lo, desired_lo, otherwise_lo));
            //
            auto dst_hi = extend_high(Vec64uc().load(dstp + x));
            auto temp_hi = extend_high(Vec64uc().load(tempp + x));
            auto src_hi = extend_high(Vec64uc().load(srcp + x));

            auto t_hi = dst_hi - temp_hi;
            auto t2_hi = dst_hi - v128;

            auto nochange_mask_hi = Vec32s(t_hi * t2_hi) < zero;

            auto t_mask_hi = abs(t_hi) < abs(t2_hi);
            auto desired_hi = src_hi - t_hi;
            auto otherwise_hi = (src_hi - dst_hi) + v128;
            auto result_hi = select(nochange_mask_hi, src_hi, select(t_mask_hi, desired_hi, otherwise_hi));
            // 
            auto result = compress_saturated(result_lo, result_hi);
            result.store(dstp + x);
        }

        dstp += dst_pitch;
        srcp += src_pitch;
        tempp += temp_pitch;
    }
}

template <int c_>
void vertical_blur3_avx512_16(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const uint16_t* srcp = reinterpret_cast<const uint16_t*>(srcp_);
    uint16_t* __restrict dstp = reinterpret_cast<uint16_t*>(dstp_);

    int mod64_width = (width + 63) & ~63;

    auto two = Vec16ui(c_);

    for (int y = 0; y < height; ++y)
    {
        const uint16_t* srcpp = y == 0 ? srcp + src_pitch : srcp - src_pitch;
        const uint16_t* srcpn = y == height - 1 ? srcp - src_pitch : srcp + src_pitch;

        for (int x = 0; x < mod64_width; x += 32)
        {
            auto p = Vec32us().load(srcpp + x);
            auto c = Vec32us().load(srcp + x);
            auto n = Vec32us().load(srcpn + x);

            auto p_lo = extend_low(p);
            auto p_hi = extend_high(p);
            auto c_lo = extend_low(c);
            auto c_hi = extend_high(c);
            auto n_lo = extend_low(n);
            auto n_hi = extend_high(n);

            auto acc_lo = c_lo + p_lo;
            auto acc_hi = c_hi + p_hi;

            acc_lo = acc_lo + c_lo;
            acc_hi = acc_hi + c_hi;

            acc_lo = acc_lo + n_lo;
            acc_hi = acc_hi + n_hi;

            acc_lo = acc_lo + two;
            acc_hi = acc_hi + two;

            acc_lo = acc_lo >> 2;
            acc_hi = acc_hi >> 2;

            auto dst = compress_saturated(acc_lo, acc_hi);
            dst.store(dstp + x);
        }

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

template void vertical_blur3_avx512_16<8>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_blur3_avx512_16<32>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_blur3_avx512_16<128>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_blur3_avx512_16<512>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;

template <int c_>
void vertical_blur5_avx512_16(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const uint16_t* srcp = reinterpret_cast<const uint16_t*>(srcp_);
    uint16_t* __restrict dstp = reinterpret_cast<uint16_t*>(dstp_);

    int mod64_width = (width + 63) & ~63;

    auto six = Vec16ui(6);
    auto eight = Vec16ui(c_);

    for (int y = 0; y < height; ++y)
    {
        const uint16_t* srcppp = y < 2 ? srcp + src_pitch * 2 : srcp - src_pitch * 2;
        const uint16_t* srcpp = y == 0 ? srcp + src_pitch : srcp - src_pitch;
        const uint16_t* srcpn = y == height - 1 ? srcp - src_pitch : srcp + src_pitch;
        const uint16_t* srcpnn = y > height - 3 ? srcp - src_pitch * 2 : srcp + src_pitch * 2;

        for (int x = 0; x < mod64_width; x += 32)
        {
            auto p2 = Vec32us().load(srcppp + x);
            auto p1 = Vec32us().load(srcpp + x);
            auto c = Vec32us().load(srcp + x);
            auto n1 = Vec32us().load(srcpn + x);
            auto n2 = Vec32us().load(srcpnn + x);

            auto p2_lo = extend_low(p2);
            auto p2_hi = extend_high(p2);
            auto p1_lo = extend_low(p1);
            auto p1_hi = extend_high(p1);
            auto c_lo = extend_low(c);
            auto c_hi = extend_high(c);
            auto n1_lo = extend_low(n1);
            auto n1_hi = extend_high(n1);
            auto n2_lo = extend_low(n2);
            auto n2_hi = extend_high(n2);

            auto acc_lo = c_lo * six;
            auto acc_hi = c_hi * six;

            auto t_lo = p1_lo + n1_lo;
            auto t_hi = p1_hi + n1_hi;

            acc_lo = acc_lo + n2_lo;
            acc_hi = acc_hi + n2_hi;

            t_lo = t_lo << 2;
            t_hi = t_hi << 2;

            t_lo = t_lo + p2_lo;
            t_hi = t_hi + p2_hi;

            acc_lo = acc_lo + eight;
            acc_hi = acc_hi + eight;

            acc_lo = acc_lo + t_lo;
            acc_hi = acc_hi + t_hi;

            acc_lo = acc_lo >> 4;
            acc_hi = acc_hi >> 4;

            auto dst = compress_saturated(acc_lo, acc_hi);
            dst.store(dstp + x);
        }

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

template void vertical_blur5_avx512_16<32>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_blur5_avx512_16<128>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_blur5_avx512_16<512>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_blur5_avx512_16<2048>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;

template <uint32_t u>
static void mt_makediff_avx512_16(void* __restrict dstp_, const void* c1p_, const void* c2p_, int dst_pitch, int c1_pitch, int c2_pitch, int width, int height) noexcept
{
    const uint16_t* c1p = reinterpret_cast<const uint16_t*>(c1p_);
    const uint16_t* c2p = reinterpret_cast<const uint16_t*>(c2p_);
    uint16_t* __restrict dstp = reinterpret_cast<uint16_t*>(dstp_);

    int mod64_width = (width + 63) & ~63;

    auto v128 = Vec32us(u);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < mod64_width; x += 32)
        {
            auto c1 = Vec32us().load(c1p + x);
            auto c2 = Vec32us().load(c2p + x);

            c1 = c1 - v128;
            c2 = c2 - v128;

            auto diff = c1 - c2;
            diff = diff + v128;
            diff.store(dstp + x);
        }

        dstp += dst_pitch;
        c1p += c1_pitch;
        c2p += c2_pitch;
    }
}

template <int c, int h, uint32_t u>
void vertical_sbr_avx512_16(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept
{
    vertical_blur3_avx512_16<c>(tempp_, srcp_, temp_pitch, src_pitch, width, height); //temp = rg11
    mt_makediff_avx512_16<u>(dstp_, srcp_, tempp_, dst_pitch, src_pitch, temp_pitch, width, height); //dst = rg11D
    vertical_blur3_avx512_16<c>(tempp_, dstp_, temp_pitch, dst_pitch, width, height); //temp = rg11D.vblur()

    const uint16_t* srcp = reinterpret_cast<const uint16_t*>(srcp_);
    uint16_t* __restrict tempp = reinterpret_cast<uint16_t*>(tempp_);
    uint16_t* __restrict dstp = reinterpret_cast<uint16_t*>(dstp_);

    int mod64_width = (width + 63) & ~63;

    Vec16i zero = zero_si512();
    auto v128 = Vec16ui(h);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < mod64_width; x += 32)
        {
            auto dst_lo = extend_low(Vec32us().load(dstp + x));
            auto temp_lo = extend_low(Vec32us().load(tempp + x));
            auto src_lo = extend_low(Vec32us().load(srcp + x));

            auto t_lo = dst_lo - temp_lo;
            auto t2_lo = dst_lo - v128;

            auto nochange_mask_lo = Vec16i(t_lo * t2_lo) < zero;

            auto t_mask_lo = abs(t_lo) < abs(t2_lo);
            auto desired_lo = src_lo - t_lo;
            auto otherwise_lo = (src_lo - dst_lo) + v128;
            auto result_lo = select(nochange_mask_lo, src_lo, select(t_mask_lo, desired_lo, otherwise_lo));
            //
            auto dst_hi = extend_high(Vec32us().load(dstp + x));
            auto temp_hi = extend_high(Vec32us().load(tempp + x));
            auto src_hi = extend_high(Vec32us().load(srcp + x));

            auto t_hi = dst_hi - temp_hi;
            auto t2_hi = dst_hi - v128;

            auto nochange_mask_hi = Vec16i(t_hi * t2_hi) < zero;

            auto t_mask_hi = abs(t_hi) < abs(t2_hi);
            auto desired_hi = src_hi - t_hi;
            auto otherwise_hi = (src_hi - dst_hi) + v128;
            auto result_hi = select(nochange_mask_hi, src_hi, select(t_mask_hi, desired_hi, otherwise_hi));
            // 
            auto result = compress_saturated(result_lo, result_hi);
            result.store(dstp + x);
        }

        dstp += dst_pitch;
        srcp += src_pitch;
        tempp += temp_pitch;
    }
}

template void vertical_sbr_avx512_16<8, 512, 0x200200>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_avx512_16<32, 2048, 0x800800>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_avx512_16<128, 8192, 0x20002000>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_avx512_16<512, 32768, 0x80008000>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;

template <typename T, VinverseMode mode, bool eclip, bool thresh>
void Vinverse<T, mode, eclip, thresh>::finalize_plane_avx512(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept
{
    const T* srcp = reinterpret_cast<const T*>(srcp_);
    const T* pb3 = reinterpret_cast<const T*>(pb3_);
    const T* pb6 = reinterpret_cast<const T*>(pb6_);
    T* __restrict dstp = reinterpret_cast<T*>(dstp_);

    int mod64_width = (width + 63) & ~63;

    if constexpr (std::is_same_v<T, uint8_t>)
    {
        auto sstr_vector = Vec16f(sstr_);
        auto scl_vector = Vec16f(scl_);
        auto amnt_vector = Vec32s(amnt_);

        if constexpr (eclip)
        {
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < mod64_width; x += 64)
                {
                    auto b3_lo = extend_low(Vec64uc().load(pb3 + x));
                    auto b6_lo = extend_low(Vec64uc().load(pb6 + x));
                    auto src_lo = extend_low(Vec64uc().load(srcp + x));

                    auto d1i_lo = Vec32s(src_lo - b3_lo);

                    if constexpr (thresh)
                    {
                        Vec32us df_l;

                        auto ch_lo = abs(d1i_lo) >= Vec32us(thr_);

                        int64_t check_lo;
                        int64_t check_lo1;
                        int64_t check_hi;
                        int64_t check_hi1;

                        Vec8us(ch_lo.get_low().get_low()).storel(&check_lo);
                        Vec8us(ch_lo.get_low().get_high()).storel(&check_lo1);
                        //
                        Vec8us(ch_lo.get_high().get_low()).storel(&check_hi);
                        Vec8us(ch_lo.get_high().get_high()).storel(&check_hi1);

                        if (!check_lo && !check_lo1 && !check_hi && !check_hi1)
                            df_l = src_lo;
                        else
                        {
                            auto d2i_lo = Vec32s(src_lo - b6_lo);

                            auto d1_lo = to_float(extend_low(d1i_lo));
                            auto d1_lo1 = to_float(extend_high(d1i_lo));

                            auto d2_lo = to_float(extend_low(d2i_lo));
                            auto d2_lo1 = to_float(extend_high(d2i_lo));

                            auto da_mask_lo = abs(d1_lo) < abs(d2_lo);
                            auto da_mask_lo1 = abs(d1_lo1) < abs(d2_lo1);

                            auto da_lo = select(da_mask_lo, d1_lo, d2_lo);
                            auto da_lo1 = select(da_mask_lo1, d1_lo1, d2_lo1);

                            auto desired_lo = da_lo * scl_vector;
                            auto desired_lo1 = da_lo1 * scl_vector;

                            auto fin_mask_lo = (d1_lo * d2_lo) < 0.0f;
                            auto fin_mask_lo1 = (d1_lo1 * d2_lo1) < 0.0f;

                            auto add_lo = truncatei(select(fin_mask_lo, desired_lo, da_lo));
                            auto add_lo1 = truncatei(select(fin_mask_lo1, desired_lo1, da_lo1));

                            auto add_l = compress_saturated(add_lo, add_lo1);
                            df_l = b6_lo + Vec32us(add_l);

                            auto minm_l = src_lo - amnt_vector;
                            auto maxf_l = src_lo + amnt_vector;

                            df_l = max(df_l, minm_l);
                            df_l = select(ch_lo, min(df_l, maxf_l), src_lo);
                        }
                        //
                        auto b3_hi = extend_high(Vec64uc().load(pb3 + x));
                        auto b6_hi = extend_high(Vec64uc().load(pb6 + x));
                        auto src_hi = extend_high(Vec64uc().load(srcp + x));

                        auto d1i_hi = Vec32s(src_hi - b3_hi);

                        Vec32us df_h;

                        auto ch_hi = abs(d1i_hi) >= Vec32us(thr_);

                        int64_t check1_lo;
                        int64_t check1_lo1;
                        int64_t check1_hi;
                        int64_t check1_hi1;

                        Vec8us(ch_hi.get_low().get_low()).storel(&check1_lo);
                        Vec8us(ch_hi.get_low().get_high()).storel(&check1_lo1);
                        //
                        Vec8us(ch_hi.get_high().get_low()).storel(&check1_hi);
                        Vec8us(ch_hi.get_high().get_high()).storel(&check1_hi1);

                        if (!check1_lo && !check1_lo1 && !check1_hi && !check1_hi1)
                            df_h = src_hi;
                        else
                        {
                            auto d2i_hi = Vec32s(src_hi - b6_hi);

                            auto d1_hi = to_float(extend_low(d1i_hi));
                            auto d1_hi1 = to_float(extend_high(d1i_hi));

                            auto d2_hi = to_float(extend_low(d2i_hi));
                            auto d2_hi1 = to_float(extend_high(d2i_hi));

                            auto da_mask_hi = abs(d1_hi) < abs(d2_hi);
                            auto da_mask_hi1 = abs(d1_hi1) < abs(d2_hi1);

                            auto da_hi = select(da_mask_hi, d1_hi, d2_hi);
                            auto da_hi1 = select(da_mask_hi1, d1_hi1, d2_hi1);

                            auto desired_hi = da_hi * scl_vector;
                            auto desired_hi1 = da_hi1 * scl_vector;

                            auto fin_mask_hi = (d1_hi * d2_hi) < 0.0f;
                            auto fin_mask_hi1 = (d1_hi1 * d2_hi1) < 0.0f;

                            auto add_hi = truncatei(select(fin_mask_hi, desired_hi, da_hi));
                            auto add_hi1 = truncatei(select(fin_mask_hi1, desired_hi1, da_hi1));

                            auto add_h = compress_saturated(add_hi, add_hi1);
                            df_h = b6_hi + Vec32us(add_h);

                            auto minm_h = src_hi - amnt_vector;
                            auto maxf_h = src_hi + amnt_vector;

                            df_h = max(df_h, minm_h);
                            df_h = select(ch_hi, min(df_h, maxf_h), src_hi);
                        }
                        //
                        auto result = compress_saturated_s2u(df_l, df_h);
                        result.store(dstp + x);
                    }
                    else
                    {
                        auto d2i_lo = Vec32s(src_lo - b6_lo);

                        auto d1_lo = to_float(extend_low(d1i_lo));
                        auto d1_lo1 = to_float(extend_high(d1i_lo));

                        auto d2_lo = to_float(extend_low(d2i_lo));
                        auto d2_lo1 = to_float(extend_high(d2i_lo));

                        auto da_mask_lo = abs(d1_lo) < abs(d2_lo);
                        auto da_mask_lo1 = abs(d1_lo1) < abs(d2_lo1);

                        auto da_lo = select(da_mask_lo, d1_lo, d2_lo);
                        auto da_lo1 = select(da_mask_lo1, d1_lo1, d2_lo1);

                        auto desired_lo = da_lo * scl_vector;
                        auto desired_lo1 = da_lo1 * scl_vector;

                        auto fin_mask_lo = (d1_lo * d2_lo) < 0.0f;
                        auto fin_mask_lo1 = (d1_lo1 * d2_lo1) < 0.0f;

                        auto add_lo = truncatei(select(fin_mask_lo, desired_lo, da_lo));
                        auto add_lo1 = truncatei(select(fin_mask_lo1, desired_lo1, da_lo1));

                        auto add_l = compress_saturated(add_lo, add_lo1);
                        auto df_l = b6_lo + Vec32us(add_l);

                        auto minm_l = src_lo - amnt_vector;
                        auto maxf_l = src_lo + amnt_vector;

                        df_l = max(df_l, minm_l);
                        df_l = min(df_l, maxf_l);
                        //
                        auto b3_hi = extend_high(Vec64uc().load(pb3 + x));
                        auto b6_hi = extend_high(Vec64uc().load(pb6 + x));
                        auto src_hi = extend_high(Vec64uc().load(srcp + x));

                        auto d1i_hi = Vec32s(src_hi - b3_hi);
                        auto d2i_hi = Vec32s(src_hi - b6_hi);

                        auto d1_hi = to_float(extend_low(d1i_hi));
                        auto d1_hi1 = to_float(extend_high(d1i_hi));

                        auto d2_hi = to_float(extend_low(d2i_hi));
                        auto d2_hi1 = to_float(extend_high(d2i_hi));

                        auto da_mask_hi = abs(d1_hi) < abs(d2_hi);
                        auto da_mask_hi1 = abs(d1_hi1) < abs(d2_hi1);

                        auto da_hi = select(da_mask_hi, d1_hi, d2_hi);
                        auto da_hi1 = select(da_mask_hi1, d1_hi1, d2_hi1);

                        auto desired_hi = da_hi * scl_vector;
                        auto desired_hi1 = da_hi1 * scl_vector;

                        auto fin_mask_hi = (d1_hi * d2_hi) < 0.0f;
                        auto fin_mask_hi1 = (d1_hi1 * d2_hi1) < 0.0f;

                        auto add_hi = truncatei(select(fin_mask_hi, desired_hi, da_hi));
                        auto add_hi1 = truncatei(select(fin_mask_hi1, desired_hi1, da_hi1));

                        auto add_h = compress_saturated(add_hi, add_hi1);
                        auto df_h = b6_hi + Vec32us(add_h);

                        auto minm_h = src_hi - amnt_vector;
                        auto maxf_h = src_hi + amnt_vector;

                        df_h = max(df_h, minm_h);
                        df_h = min(df_h, maxf_h);
                        //
                        auto result = compress_saturated_s2u(df_l, df_h);
                        result.store(dstp + x);
                    }
                }

                srcp += src_pitch;
                pb3 += pb_pitch;
                pb6 += clip2_pitch;
                dstp += dst_pitch;
            }
        }
        else
        {
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < mod64_width; x += 64)
                {
                    auto b3_lo = extend_low(Vec64uc().load(pb3 + x));
                    auto b6_lo = extend_low(Vec64uc().load(pb6 + x));
                    auto src_lo = extend_low(Vec64uc().load(srcp + x));

                    auto d1i_lo = Vec32s(src_lo - b3_lo);

                    if constexpr (thresh)
                    {
                        Vec32us df_l;

                        auto ch_lo = abs(d1i_lo) >= Vec32us(thr_);

                        int64_t check_lo;
                        int64_t check_lo1;
                        int64_t check_hi;
                        int64_t check_hi1;

                        Vec8us(ch_lo.get_low().get_low()).storel(&check_lo);
                        Vec8us(ch_lo.get_low().get_high()).storel(&check_lo1);
                        //
                        Vec8us(ch_lo.get_high().get_low()).storel(&check_hi);
                        Vec8us(ch_lo.get_high().get_high()).storel(&check_hi1);

                        if (!check_lo && !check_lo1 && !check_hi && !check_hi1)
                            df_l = src_lo;
                        else
                        {
                            auto d2i_lo = Vec32s(b3_lo - b6_lo);

                            auto d1_lo = to_float(extend_low(d1i_lo));
                            auto d1_lo1 = to_float(extend_high(d1i_lo));

                            auto d2_lo = to_float(extend_low(d2i_lo));
                            auto d2_lo1 = to_float(extend_high(d2i_lo));

                            auto t_lo = d2_lo * sstr_vector;
                            auto t_lo1 = d2_lo1 * sstr_vector;

                            auto da_mask_lo = abs(d1_lo) < abs(t_lo);
                            auto da_mask_lo1 = abs(d1_lo1) < abs(t_lo1);

                            auto da_lo = select(da_mask_lo, d1_lo, t_lo);
                            auto da_lo1 = select(da_mask_lo1, d1_lo1, t_lo1);

                            auto desired_lo = da_lo * scl_vector;
                            auto desired_lo1 = da_lo1 * scl_vector;

                            auto fin_mask_lo = (d1_lo * t_lo) < 0.0f;
                            auto fin_mask_lo1 = (d1_lo1 * t_lo1) < 0.0f;

                            auto add_lo = truncatei(select(fin_mask_lo, desired_lo, da_lo));
                            auto add_lo1 = truncatei(select(fin_mask_lo1, desired_lo1, da_lo1));

                            auto add_l = compress_saturated(add_lo, add_lo1);
                            df_l = b3_lo + Vec32us(add_l);

                            auto minm_l = src_lo - amnt_vector;
                            auto maxf_l = src_lo + amnt_vector;

                            df_l = max(df_l, minm_l);
                            df_l = select(ch_lo, min(df_l, maxf_l), src_lo);
                        }
                        //
                        auto b3_hi = extend_high(Vec64uc().load(pb3 + x));
                        auto b6_hi = extend_high(Vec64uc().load(pb6 + x));
                        auto src_hi = extend_high(Vec64uc().load(srcp + x));

                        auto d1i_hi = Vec32s(src_hi - b3_hi);

                        Vec32us df_h;

                        auto ch_hi = abs(d1i_hi) >= Vec32us(thr_);

                        int64_t check1_lo;
                        int64_t check1_lo1;
                        int64_t check1_hi;
                        int64_t check1_hi1;

                        Vec8us(ch_hi.get_low().get_low()).storel(&check1_lo);
                        Vec8us(ch_hi.get_low().get_high()).storel(&check1_lo1);
                        //
                        Vec8us(ch_hi.get_high().get_low()).storel(&check1_hi);
                        Vec8us(ch_hi.get_high().get_high()).storel(&check1_hi1);

                        if (!check1_lo && !check1_lo1 && !check1_hi && !check1_hi1)
                            df_h = src_hi;
                        else
                        {
                            auto d2i_hi = Vec32s(b3_hi - b6_hi);

                            auto d1_hi = to_float(extend_low(d1i_hi));
                            auto d1_hi1 = to_float(extend_high(d1i_hi));

                            auto d2_hi = to_float(extend_low(d2i_hi));
                            auto d2_hi1 = to_float(extend_high(d2i_hi));

                            auto t_hi = d2_hi * sstr_vector;
                            auto t_hi1 = d2_hi1 * sstr_vector;

                            auto da_mask_hi = abs(d1_hi) < abs(t_hi);
                            auto da_mask_hi1 = abs(d1_hi1) < abs(t_hi1);

                            auto da_hi = select(da_mask_hi, d1_hi, t_hi);
                            auto da_hi1 = select(da_mask_hi1, d1_hi1, t_hi1);

                            auto desired_hi = da_hi * scl_vector;
                            auto desired_hi1 = da_hi1 * scl_vector;

                            auto fin_mask_hi = (d1_hi * t_hi) < 0.0f;
                            auto fin_mask_hi1 = (d1_hi1 * t_hi1) < 0.0f;

                            auto add_hi = truncatei(select(fin_mask_hi, desired_hi, da_hi));
                            auto add_hi1 = truncatei(select(fin_mask_hi1, desired_hi1, da_hi1));

                            auto add_h = compress_saturated(add_hi, add_hi1);
                            df_h = b3_hi + Vec32us(add_h);

                            auto minm_h = src_hi - amnt_vector;
                            auto maxf_h = src_hi + amnt_vector;

                            df_h = max(df_h, minm_h);
                            df_h = select(ch_hi, min(df_h, maxf_h), src_hi);
                        }
                        //
                        auto result = compress_saturated(df_l, df_h);
                        result.store(dstp + x);
                    }
                    else
                    {
                        auto d2i_lo = Vec32s(b3_lo - b6_lo);

                        auto d1_lo = to_float(extend_low(d1i_lo));
                        auto d1_lo1 = to_float(extend_high(d1i_lo));

                        auto d2_lo = to_float(extend_low(d2i_lo));
                        auto d2_lo1 = to_float(extend_high(d2i_lo));

                        auto t_lo = d2_lo * sstr_vector;
                        auto t_lo1 = d2_lo1 * sstr_vector;

                        auto da_mask_lo = abs(d1_lo) < abs(t_lo);
                        auto da_mask_lo1 = abs(d1_lo1) < abs(t_lo1);

                        auto da_lo = select(da_mask_lo, d1_lo, t_lo);
                        auto da_lo1 = select(da_mask_lo1, d1_lo1, t_lo1);

                        auto desired_lo = da_lo * scl_vector;
                        auto desired_lo1 = da_lo1 * scl_vector;

                        auto fin_mask_lo = (d1_lo * t_lo) < 0.0f;
                        auto fin_mask_lo1 = (d1_lo1 * t_lo1) < 0.0f;

                        auto add_lo = truncatei(select(fin_mask_lo, desired_lo, da_lo));
                        auto add_lo1 = truncatei(select(fin_mask_lo1, desired_lo1, da_lo1));

                        auto add_l = compress_saturated(add_lo, add_lo1);
                        auto df_l = b3_lo + Vec32us(add_l);

                        auto minm_l = src_lo - amnt_vector;
                        auto maxf_l = src_lo + amnt_vector;

                        df_l = max(df_l, minm_l);
                        df_l = min(df_l, maxf_l);
                        //
                        auto b3_hi = extend_high(Vec64uc().load(pb3 + x));
                        auto b6_hi = extend_high(Vec64uc().load(pb6 + x));
                        auto src_hi = extend_high(Vec64uc().load(srcp + x));

                        auto d1i_hi = Vec32s(src_hi - b3_hi);
                        auto d2i_hi = Vec32s(b3_hi - b6_hi);

                        auto d1_hi = to_float(extend_low(d1i_hi));
                        auto d1_hi1 = to_float(extend_high(d1i_hi));

                        auto d2_hi = to_float(extend_low(d2i_hi));
                        auto d2_hi1 = to_float(extend_high(d2i_hi));

                        auto t_hi = d2_hi * sstr_vector;
                        auto t_hi1 = d2_hi1 * sstr_vector;

                        auto da_mask_hi = abs(d1_hi) < abs(t_hi);
                        auto da_mask_hi1 = abs(d1_hi1) < abs(t_hi1);

                        auto da_hi = select(da_mask_hi, d1_hi, t_hi);
                        auto da_hi1 = select(da_mask_hi1, d1_hi1, t_hi1);

                        auto desired_hi = da_hi * scl_vector;
                        auto desired_hi1 = da_hi1 * scl_vector;

                        auto fin_mask_hi = (d1_hi * t_hi) < 0.0f;
                        auto fin_mask_hi1 = (d1_hi1 * t_hi1) < 0.0f;

                        auto add_hi = truncatei(select(fin_mask_hi, desired_hi, da_hi));
                        auto add_hi1 = truncatei(select(fin_mask_hi1, desired_hi1, da_hi1));

                        auto add_h = compress_saturated(add_hi, add_hi1);
                        auto df_h = b3_hi + Vec32us(add_h);

                        auto minm_h = src_hi - amnt_vector;
                        auto maxf_h = src_hi + amnt_vector;

                        df_h = max(df_h, minm_h);
                        df_h = min(df_h, maxf_h);
                        //
                        auto result = compress_saturated(df_l, df_h);
                        result.store(dstp + x);
                    }
                }

                srcp += src_pitch;
                pb3 += pb_pitch;
                pb6 += pb_pitch;
                dstp += dst_pitch;
            }
        }
    }
    else
    {
        auto sstr_vector = Vec16f(sstr_);
        auto scl_vector = Vec16f(scl_);
        auto amnt_vector = Vec16i(amnt_);

        if constexpr (eclip)
        {
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < mod64_width; x += 32)
                {
                    auto b3_lo = extend_low(Vec32us().load(pb3 + x));
                    auto b6_lo = extend_low(Vec32us().load(pb6 + x));
                    auto src_lo = extend_low(Vec32us().load(srcp + x));

                    auto d1i_lo = Vec16i(src_lo - b3_lo);

                    if constexpr (thresh)
                    {
                        Vec16i df_l;

                        auto ch_lo = abs(d1i_lo) >= Vec16i(thr_);

                        int64_t check_lo;
                        int64_t check_lo1;
                        int64_t check_hi;
                        int64_t check_hi1;

                        Vec4i(ch_lo.get_low().get_low()).storel(&check_lo);
                        Vec4i(ch_lo.get_low().get_high()).storel(&check_lo1);
                        //
                        Vec4i(ch_lo.get_high().get_low()).storel(&check_hi);
                        Vec4i(ch_lo.get_high().get_high()).storel(&check_hi1);

                        if (!check_lo && !check_lo1 && !check_hi && !check_hi1)
                            df_l = src_lo;
                        else
                        {
                            auto d2i_lo = Vec16i(src_lo - b6_lo);

                            auto d1_lo = to_float(d1i_lo);
                            auto d2_lo = to_float(d2i_lo);

                            auto da_mask_lo = abs(d1_lo) < abs(d2_lo);
                            auto da_lo = select(da_mask_lo, d1_lo, d2_lo);

                            auto desired_lo = da_lo * scl_vector;
                            auto fin_mask_lo = (d1_lo * d2_lo) < 0.0f;

                            auto add_lo = truncatei(select(fin_mask_lo, desired_lo, da_lo));
                            df_l = b6_lo + Vec16i(add_lo);

                            auto minm_l = src_lo - amnt_vector;
                            auto maxf_l = src_lo + amnt_vector;

                            df_l = max(df_l, minm_l);
                            df_l = select(ch_lo, min(df_l, maxf_l), src_lo);
                        }
                        //
                        auto b3_hi = extend_high(Vec32us().load(pb3 + x));
                        auto b6_hi = extend_high(Vec32us().load(pb6 + x));
                        auto src_hi = extend_high(Vec32us().load(srcp + x));

                        auto d1i_hi = Vec16i(src_hi - b3_hi);
                        auto d2i_hi = Vec16i(src_hi - b6_hi);

                        auto d1_hi = to_float(d1i_hi);

                        Vec16i df_h;

                        auto ch_hi = abs(d1i_hi) >= Vec16i(thr_);

                        int64_t check1_lo;
                        int64_t check1_lo1;
                        int64_t check1_hi;
                        int64_t check1_hi1;

                        Vec4i(ch_hi.get_low().get_low()).storel(&check1_lo);
                        Vec4i(ch_hi.get_low().get_high()).storel(&check1_lo1);
                        //
                        Vec4i(ch_hi.get_high().get_low()).storel(&check1_hi);
                        Vec4i(ch_hi.get_high().get_high()).storel(&check1_hi1);

                        if (!check1_lo && !check1_lo1 && !check1_hi && !check1_hi1)
                            df_h = src_hi;
                        else
                        {
                            auto d2_hi = to_float(d2i_hi);

                            auto da_mask_hi = abs(d1_hi) < abs(d2_hi);
                            auto da_hi = select(da_mask_hi, d1_hi, d2_hi);

                            auto desired_hi = da_hi * scl_vector;
                            auto fin_mask_hi = (d1_hi * d2_hi) < 0.0f;

                            auto add_hi = truncatei(select(fin_mask_hi, desired_hi, da_hi));
                            df_h = b6_hi + Vec16i(add_hi);

                            auto minm_h = src_hi - amnt_vector;
                            auto maxf_h = src_hi + amnt_vector;

                            df_h = max(df_h, minm_h);
                            df_h = select(ch_hi, min(df_h, maxf_h), src_hi);
                        }
                        //
                        auto result = compress_saturated_s2u(df_l, df_h);
                        result.store(dstp + x);
                    }
                    else
                    {
                        auto d2i_lo = Vec16i(src_lo - b6_lo);

                        auto d1_lo = to_float(d1i_lo);
                        auto d2_lo = to_float(d2i_lo);

                        auto da_mask_lo = abs(d1_lo) < abs(d2_lo);
                        auto da_lo = select(da_mask_lo, d1_lo, d2_lo);

                        auto desired_lo = da_lo * scl_vector;
                        auto fin_mask_lo = (d1_lo * d2_lo) < 0.0f;

                        auto add_lo = truncatei(select(fin_mask_lo, desired_lo, da_lo));
                        auto df_l = b6_lo + Vec16i(add_lo);

                        auto minm_l = src_lo - amnt_vector;
                        auto maxf_l = src_lo + amnt_vector;

                        df_l = max(df_l, minm_l);
                        df_l = min(df_l, maxf_l);
                        //
                        auto b3_hi = extend_high(Vec32us().load(pb3 + x));
                        auto b6_hi = extend_high(Vec32us().load(pb6 + x));
                        auto src_hi = extend_high(Vec32us().load(srcp + x));

                        auto d1i_hi = Vec16i(src_hi - b3_hi);
                        auto d2i_hi = Vec16i(src_hi - b6_hi);

                        auto d1_hi = to_float(d1i_hi);
                        auto d2_hi = to_float(d2i_hi);

                        auto da_mask_hi = abs(d1_hi) < abs(d2_hi);
                        auto da_hi = select(da_mask_hi, d1_hi, d2_hi);

                        auto desired_hi = da_hi * scl_vector;
                        auto fin_mask_hi = (d1_hi * d2_hi) < 0.0f;

                        auto add_hi = truncatei(select(fin_mask_hi, desired_hi, da_hi));
                        auto df_h = b6_hi + Vec16i(add_hi);

                        auto minm_h = src_hi - amnt_vector;
                        auto maxf_h = src_hi + amnt_vector;

                        df_h = max(df_h, minm_h);
                        df_h = min(df_h, maxf_h);
                        //
                        auto result = compress_saturated_s2u(df_l, df_h);
                        result.store(dstp + x);
                    }
                }

                srcp += src_pitch;
                pb3 += pb_pitch;
                pb6 += clip2_pitch;
                dstp += dst_pitch;
            }
        }
        else
        {
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < mod64_width; x += 32)
                {
                    auto b3_lo = extend_low(Vec32us().load(pb3 + x));
                    auto b6_lo = extend_low(Vec32us().load(pb6 + x));
                    auto src_lo = extend_low(Vec32us().load(srcp + x));

                    auto d1i_lo = Vec16i(src_lo - b3_lo);

                    if constexpr (thresh)
                    {
                        Vec16i df_l;

                        auto ch_lo = abs(d1i_lo) >= Vec16i(thr_);

                        int64_t check_lo;
                        int64_t check_lo1;
                        int64_t check_hi;
                        int64_t check_hi1;

                        Vec4i(ch_lo.get_low().get_low()).storel(&check_lo);
                        Vec4i(ch_lo.get_low().get_high()).storel(&check_lo1);
                        //
                        Vec4i(ch_lo.get_high().get_low()).storel(&check_hi);
                        Vec4i(ch_lo.get_high().get_high()).storel(&check_hi1);

                        if (!check_lo && !check_lo1 && !check_hi && !check_hi1)
                            df_l = src_lo;
                        else
                        {
                            auto d2i_lo = Vec16i(b3_lo - b6_lo);

                            auto d1_lo = to_float(d1i_lo);
                            auto d2_lo = to_float(d2i_lo);

                            auto t_lo = d2_lo * sstr_vector;
                            auto da_mask_lo = abs(d1_lo) < abs(t_lo);
                            auto da_lo = select(da_mask_lo, d1_lo, t_lo);

                            auto desired_lo = da_lo * scl_vector;
                            auto fin_mask_lo = (d1_lo * t_lo) < 0.0f;

                            auto add_lo = truncatei(select(fin_mask_lo, desired_lo, da_lo));
                            df_l = b3_lo + Vec16i(add_lo);

                            auto minm_l = src_lo - amnt_vector;
                            auto maxf_l = src_lo + amnt_vector;

                            df_l = max(df_l, minm_l);
                            df_l = select(ch_lo, min(df_l, maxf_l), src_lo);
                        }
                        //
                        auto b3_hi = extend_high(Vec32us().load(pb3 + x));
                        auto b6_hi = extend_high(Vec32us().load(pb6 + x));
                        auto src_hi = extend_high(Vec32us().load(srcp + x));

                        auto d1i_hi = Vec16i(src_hi - b3_hi);

                        Vec16i df_h;

                        auto ch_hi = abs(d1i_hi) >= Vec16i(thr_);

                        int64_t check1_lo;
                        int64_t check1_lo1;
                        int64_t check1_hi;
                        int64_t check1_hi1;

                        Vec4i(ch_hi.get_low().get_low()).storel(&check1_lo);
                        Vec4i(ch_hi.get_low().get_high()).storel(&check1_lo1);
                        //
                        Vec4i(ch_hi.get_high().get_low()).storel(&check1_hi);
                        Vec4i(ch_hi.get_high().get_high()).storel(&check1_hi1);

                        if (!check1_lo && !check1_lo1 && !check1_hi && !check1_hi1)
                            df_h = src_hi;
                        else
                        {
                            auto d2i_hi = Vec16i(b3_hi - b6_hi);

                            auto d1_hi = to_float(d1i_hi);
                            auto d2_hi = to_float(d2i_hi);

                            auto t_hi = d2_hi * sstr_vector;
                            auto da_mask_hi = abs(d1_hi) < abs(t_hi);
                            auto da_hi = select(da_mask_hi, d1_hi, t_hi);

                            auto desired_hi = da_hi * scl_vector;
                            auto fin_mask_hi = (d1_hi * t_hi) < 0.0f;

                            auto add_hi = truncatei(select(fin_mask_hi, desired_hi, da_hi));
                            df_h = b3_hi + Vec16i(add_hi);

                            auto minm_h = src_hi - amnt_vector;
                            auto maxf_h = src_hi + amnt_vector;

                            df_h = max(df_h, minm_h);
                            df_h = select(ch_hi, min(df_h, maxf_h), src_hi);
                        }
                        //
                        auto result = compress_saturated_s2u(df_l, df_h);
                        result.store(dstp + x);
                    }
                    else
                    {
                        auto d2i_lo = Vec16i(b3_lo - b6_lo);

                        auto d1_lo = to_float(d1i_lo);
                        auto d2_lo = to_float(d2i_lo);

                        auto t_lo = d2_lo * sstr_vector;
                        auto da_mask_lo = abs(d1_lo) < abs(t_lo);
                        auto da_lo = select(da_mask_lo, d1_lo, t_lo);

                        auto desired_lo = da_lo * scl_vector;
                        auto fin_mask_lo = (d1_lo * t_lo) < 0.0f;

                        auto add_lo = truncatei(select(fin_mask_lo, desired_lo, da_lo));
                        auto df_l = b3_lo + Vec16i(add_lo);

                        auto minm_l = src_lo - amnt_vector;
                        auto maxf_l = src_lo + amnt_vector;

                        df_l = max(df_l, minm_l);
                        df_l = min(df_l, maxf_l);
                        //
                        auto b3_hi = extend_high(Vec32us().load(pb3 + x));
                        auto b6_hi = extend_high(Vec32us().load(pb6 + x));
                        auto src_hi = extend_high(Vec32us().load(srcp + x));

                        auto d1i_hi = Vec16i(src_hi - b3_hi);
                        auto d2i_hi = Vec16i(b3_hi - b6_hi);

                        auto d1_hi = to_float(d1i_hi);
                        auto d2_hi = to_float(d2i_hi);

                        auto t_hi = d2_hi * sstr_vector;
                        auto da_mask_hi = abs(d1_hi) < abs(t_hi);
                        auto da_hi = select(da_mask_hi, d1_hi, t_hi);

                        auto desired_hi = da_hi * scl_vector;
                        auto fin_mask_hi = (d1_hi * t_hi) < 0.0f;

                        auto add_hi = truncatei(select(fin_mask_hi, desired_hi, da_hi));
                        auto df_h = b3_hi + Vec16i(add_hi);

                        auto minm_h = src_hi - amnt_vector;
                        auto maxf_h = src_hi + amnt_vector;

                        df_h = max(df_h, minm_h);
                        df_h = min(df_h, maxf_h);
                        //
                        auto result = compress_saturated_s2u(df_l, df_h);
                        result.store(dstp + x);
                    }
                }

                srcp += src_pitch;
                pb3 += pb_pitch;
                pb6 += pb_pitch;
                dstp += dst_pitch;
            }
        }
    }
}

template void Vinverse<uint8_t, VinverseMode::Vinverse, true, true>::finalize_plane_avx512
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint8_t, VinverseMode::Vinverse, false, true>::finalize_plane_avx512
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint8_t, VinverseMode::Vinverse, true, false>::finalize_plane_avx512
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint8_t, VinverseMode::Vinverse, false, false>::finalize_plane_avx512
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;

template void Vinverse<uint8_t, VinverseMode::Vinverse2, false, false>::finalize_plane_avx512
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;

template void Vinverse<uint16_t, VinverseMode::Vinverse, true, true>::finalize_plane_avx512
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint16_t, VinverseMode::Vinverse, false, true>::finalize_plane_avx512
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint16_t, VinverseMode::Vinverse, true, false>::finalize_plane_avx512
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint16_t, VinverseMode::Vinverse, false, false>::finalize_plane_avx512
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;

template void Vinverse<uint16_t, VinverseMode::Vinverse2, false, false>::finalize_plane_avx512
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
