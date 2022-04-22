#include "vinverse.h"
#include "VCL2/vectorclass.h"

void vertical_blur3_sse2_8(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const uint8_t* srcp = reinterpret_cast<const uint8_t*>(srcp_);
    uint8_t* __restrict dstp = reinterpret_cast<uint8_t*>(dstp_);

    int mod16_width = (width + 15) & ~15;

    auto two = Vec8us(2);

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcpp = y == 0 ? srcp + src_pitch : srcp - src_pitch;
        const uint8_t* srcpn = y == height - 1 ? srcp - src_pitch : srcp + src_pitch;

        for (int x = 0; x < mod16_width; x += 16)
        {
            auto p = Vec16uc().load(srcpp + x);
            auto c = Vec16uc().load(srcp + x);
            auto n = Vec16uc().load(srcpn + x);

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

void vertical_blur5_sse2_8(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const uint8_t* srcp = reinterpret_cast<const uint8_t*>(srcp_);
    uint8_t* __restrict dstp = reinterpret_cast<uint8_t*>(dstp_);

    int mod16_width = (width + 15) & ~15;

    auto six = Vec8us(6);
    auto eight = Vec8us(8);

    for (int y = 0; y < height; ++y)
    {
        const uint8_t* srcppp = y < 2 ? srcp + src_pitch * 2 : srcp - src_pitch * 2;
        const uint8_t* srcpp = y == 0 ? srcp + src_pitch : srcp - src_pitch;
        const uint8_t* srcpn = y == height - 1 ? srcp - src_pitch : srcp + src_pitch;
        const uint8_t* srcpnn = y > height - 3 ? srcp - src_pitch * 2 : srcp + src_pitch * 2;

        for (int x = 0; x < mod16_width; x += 16)
        {
            auto p2 = Vec16uc().load(srcppp + x);
            auto p1 = Vec16uc().load(srcpp + x);
            auto c = Vec16uc().load(srcp + x);
            auto n1 = Vec16uc().load(srcpn + x);
            auto n2 = Vec16uc().load(srcpnn + x);

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

static void mt_makediff_sse2_8(void* __restrict dstp_, const void* c1p_, const void* c2p_, int dst_pitch, int c1_pitch, int c2_pitch, int width, int height) noexcept
{
    const uint8_t* c1p = reinterpret_cast<const uint8_t*>(c1p_);
    const uint8_t* c2p = reinterpret_cast<const uint8_t*>(c2p_);
    uint8_t* __restrict dstp = reinterpret_cast<uint8_t*>(dstp_);

    int mod16_width = (width + 15) & ~15;

    auto v128 = Vec16uc(0x80808080);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < mod16_width; x += 16)
        {
            auto c1 = Vec16uc().load(c1p + x);
            auto c2 = Vec16uc().load(c2p + x);

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

void vertical_sbr_sse2_8(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept
{
    vertical_blur3_sse2_8(tempp_, srcp_, temp_pitch, src_pitch, width, height); //temp = rg11
    mt_makediff_sse2_8(dstp_, srcp_, tempp_, dst_pitch, src_pitch, temp_pitch, width, height); //dst = rg11D
    vertical_blur3_sse2_8(tempp_, dstp_, temp_pitch, dst_pitch, width, height); //temp = rg11D.vblur()

    const uint8_t* srcp = reinterpret_cast<const uint8_t*>(srcp_);
    uint8_t* __restrict tempp = reinterpret_cast<uint8_t*>(tempp_);
    uint8_t* __restrict dstp = reinterpret_cast<uint8_t*>(dstp_);

    int mod8_width = (width + 7) & ~7;

    Vec8us zero = zero_si128();
    auto v128 = Vec8us(128);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < mod8_width; x += 8)
        {
            auto dst = extend_low(Vec16uc().loadl(dstp + x));
            auto temp = extend_low(Vec16uc().loadl(tempp + x));
            auto src = extend_low(Vec16uc().loadl(srcp + x));

            auto t = dst - temp;
            auto t2 = dst - v128;

            auto nochange_mask = Vec8s(t * t2) < zero;

            auto t_mask = abs(t) < abs(t2);
            auto desired = src - t;
            auto otherwise = (src - dst) + v128;
            auto result = select(nochange_mask, src, select(t_mask, desired, otherwise));

            result = compress_saturated(result, zero);
            result.storel(dstp + x);
        }

        dstp += dst_pitch;
        srcp += src_pitch;
        tempp += temp_pitch;
    }
}

template <int c_>
void vertical_blur3_sse2_16(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const uint16_t* srcp = reinterpret_cast<const uint16_t*>(srcp_);
    uint16_t* __restrict dstp = reinterpret_cast<uint16_t*>(dstp_);

    int mod16_width = (width + 15) & ~15;

    auto two = Vec4ui(c_);

    for (int y = 0; y < height; ++y)
    {
        const uint16_t* srcpp = y == 0 ? srcp + src_pitch : srcp - src_pitch;
        const uint16_t* srcpn = y == height - 1 ? srcp - src_pitch : srcp + src_pitch;

        for (int x = 0; x < mod16_width; x += 8)
        {
            auto p = Vec8us().load(srcpp + x);
            auto c = Vec8us().load(srcp + x);
            auto n = Vec8us().load(srcpn + x);

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

template void vertical_blur3_sse2_16<8>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_blur3_sse2_16<32>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_blur3_sse2_16<128>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_blur3_sse2_16<512>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;

template <int c_>
void vertical_blur5_sse2_16(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const uint16_t* srcp = reinterpret_cast<const uint16_t*>(srcp_);
    uint16_t* __restrict dstp = reinterpret_cast<uint16_t*>(dstp_);

    int mod16_width = (width + 15) & ~15;

    auto six = Vec4ui(6);
    auto eight = Vec4ui(c_);

    for (int y = 0; y < height; ++y)
    {
        const uint16_t* srcppp = y < 2 ? srcp + src_pitch * 2 : srcp - src_pitch * 2;
        const uint16_t* srcpp = y == 0 ? srcp + src_pitch : srcp - src_pitch;
        const uint16_t* srcpn = y == height - 1 ? srcp - src_pitch : srcp + src_pitch;
        const uint16_t* srcpnn = y > height - 3 ? srcp - src_pitch * 2 : srcp + src_pitch * 2;

        for (int x = 0; x < mod16_width; x += 8)
        {
            auto p2 = Vec8us().load(srcppp + x);
            auto p1 = Vec8us().load(srcpp + x);
            auto c = Vec8us().load(srcp + x);
            auto n1 = Vec8us().load(srcpn + x);
            auto n2 = Vec8us().load(srcpnn + x);

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

template void vertical_blur5_sse2_16<32>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_blur5_sse2_16<128>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_blur5_sse2_16<512>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_blur5_sse2_16<2048>(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;

template <uint32_t u>
static void mt_makediff_sse2_16(void* __restrict dstp_, const void* c1p_, const void* c2p_, int dst_pitch, int c1_pitch, int c2_pitch, int width, int height) noexcept
{
    const uint16_t* c1p = reinterpret_cast<const uint16_t*>(c1p_);
    const uint16_t* c2p = reinterpret_cast<const uint16_t*>(c2p_);
    uint16_t* __restrict dstp = reinterpret_cast<uint16_t*>(dstp_);

    int mod16_width = (width + 15) & ~15;

    auto v128 = Vec8us(u);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < mod16_width; x += 8)
        {
            auto c1 = Vec8us().load(c1p + x);
            auto c2 = Vec8us().load(c2p + x);

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
void vertical_sbr_sse2_16(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept
{
    vertical_blur3_sse2_16<c>(tempp_, srcp_, temp_pitch, src_pitch, width, height); //temp = rg11
    mt_makediff_sse2_16<u>(dstp_, srcp_, tempp_, dst_pitch, src_pitch, temp_pitch, width, height); //dst = rg11D
    vertical_blur3_sse2_16<c>(tempp_, dstp_, temp_pitch, dst_pitch, width, height); //temp = rg11D.vblur()

    const uint16_t* srcp = reinterpret_cast<const uint16_t*>(srcp_);
    uint16_t* __restrict tempp = reinterpret_cast<uint16_t*>(tempp_);
    uint16_t* __restrict dstp = reinterpret_cast<uint16_t*>(dstp_);

    int mod8_width = (width + 7) & ~7;

    Vec4i zero = zero_si128();
    auto v128 = Vec4ui(h);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < mod8_width; x += 4)
        {
            auto dst = extend_low(Vec8us().load(dstp + x));
            auto temp = extend_low(Vec8us().load(tempp + x));
            auto src = extend_low(Vec8us().load(srcp + x));

            auto t = dst - temp;
            auto t2 = dst - v128;

            auto nochange_mask = Vec4i(t * t2) < zero;

            auto t_mask = abs(t) < abs(t2);
            auto desired = src - t;
            auto otherwise = (src - dst) + v128;
            auto result = select(nochange_mask, src, select(t_mask, desired, otherwise));

            result = compress_saturated(result, zero);
            result.storel(dstp + x);
        }

        dstp += dst_pitch;
        srcp += src_pitch;
        tempp += temp_pitch;
    }
}

template void vertical_sbr_sse2_16<8, 512, 0x200200>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_sse2_16<32, 2048, 0x800800>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_sse2_16<128, 8192, 0x20002000>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_sse2_16<512, 32768, 0x80008000>(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;

template <typename T, VinverseMode mode, bool eclip, bool thresh>
void Vinverse<T, mode, eclip, thresh>::finalize_plane_sse2(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept
{
    const T* srcp = reinterpret_cast<const T*>(srcp_);
    const T* pb3 = reinterpret_cast<const T*>(pb3_);
    const T* pb6 = reinterpret_cast<const T*>(pb6_);
    T* __restrict dstp = reinterpret_cast<T*>(dstp_);

    int mod8_width = (width + 7) & ~7;

    if constexpr (std::is_same_v<T, uint8_t>)
    {
        auto zero = zero_si128();
        auto sstr_vector = Vec4f(sstr_);
        auto scl_vector = Vec4f(scl_);
        auto amnt_vector = Vec8s(amnt_);

        if constexpr (eclip)
        {
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < mod8_width; x += 8)
                {
                    auto b3 = extend_low(Vec16uc().loadl(pb3 + x));
                    auto b6 = extend_low(Vec16uc().loadl(pb6 + x));
                    auto src = extend_low(Vec16uc().loadl(srcp + x));

                    auto d1i = Vec8s(src - b3);

                    if constexpr (thresh)
                    {
                        auto ch = compress_saturated(abs(d1i) >= Vec8s(thr_), zero);

                        int64_t check;
                        ch.storel(&check);

                        if (!check)
                            Vec16uc().loadl(srcp + x).storel(dstp + x);
                        else
                        {
                            auto d2i = Vec8s(src - b6);

                            auto d1_lo = to_float(extend_low(d1i));
                            auto d1_hi = to_float(extend_high(d1i));

                            auto d2_lo = to_float(extend_low(d2i));
                            auto d2_hi = to_float(extend_high(d2i));

                            auto da_mask_lo = abs(d1_lo) < abs(d2_lo);
                            auto da_mask_hi = abs(d1_hi) < abs(d2_hi);

                            auto da_lo = select(da_mask_lo, d1_lo, d2_lo);
                            auto da_hi = select(da_mask_hi, d1_hi, d2_hi);

                            auto desired_lo = da_lo * scl_vector;
                            auto desired_hi = da_hi * scl_vector;

                            auto fin_mask_lo = (d1_lo * d2_lo) < 0.0f;
                            auto fin_mask_hi = (d1_hi * d2_hi) < 0.0f;

                            auto add_lo = truncatei(select(fin_mask_lo, desired_lo, da_lo));
                            auto add_hi = truncatei(select(fin_mask_hi, desired_hi, da_hi));

                            auto add = compress_saturated(add_lo, add_hi);
                            auto df = b6 + Vec8us(add);

                            auto minm = src - amnt_vector;
                            auto maxf = src + amnt_vector;

                            df = max(df, minm);
                            df = min(df, maxf);

                            select(Vec16cb(ch), compress_saturated_s2u(df, zero), Vec16uc().loadl(srcp + x)).storel(dstp + x);
                        }
                    }
                    else
                    {
                        auto d2i = Vec8s(src - b6);

                        auto d1_lo = to_float(extend_low(d1i));
                        auto d1_hi = to_float(extend_high(d1i));

                        auto d2_lo = to_float(extend_low(d2i));
                        auto d2_hi = to_float(extend_high(d2i));

                        auto da_mask_lo = abs(d1_lo) < abs(d2_lo);
                        auto da_mask_hi = abs(d1_hi) < abs(d2_hi);

                        auto da_lo = select(da_mask_lo, d1_lo, d2_lo);
                        auto da_hi = select(da_mask_hi, d1_hi, d2_hi);

                        auto desired_lo = da_lo * scl_vector;
                        auto desired_hi = da_hi * scl_vector;

                        auto fin_mask_lo = (d1_lo * d2_lo) < 0.0f;
                        auto fin_mask_hi = (d1_hi * d2_hi) < 0.0f;

                        auto add_lo = truncatei(select(fin_mask_lo, desired_lo, da_lo));
                        auto add_hi = truncatei(select(fin_mask_hi, desired_hi, da_hi));

                        auto add = compress_saturated(add_lo, add_hi);
                        auto df = b6 + Vec8us(add);

                        auto minm = src - amnt_vector;
                        auto maxf = src + amnt_vector;

                        df = max(df, minm);
                        df = min(df, maxf);

                        auto result = compress_saturated_s2u(df, zero);
                        result.storel(dstp + x);
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
                for (int x = 0; x < mod8_width; x += 8)
                {
                    auto b3 = extend_low(Vec16uc().loadl(pb3 + x));
                    auto b6 = extend_low(Vec16uc().loadl(pb6 + x));
                    auto src = extend_low(Vec16uc().loadl(srcp + x));

                    auto d1i = Vec8s(src - b3);

                    if constexpr (thresh)
                    {
                        auto ch = compress_saturated(abs(d1i) >= Vec8s(thr_), zero);

                        int64_t check;
                        ch.storel(&check);

                        if (!check)
                            Vec16uc().loadl(srcp + x).storel(dstp + x);
                        else
                        {
                            auto d2i = Vec8s(b3 - b6);

                            auto d1_lo = to_float(extend_low(d1i));
                            auto d1_hi = to_float(extend_high(d1i));

                            auto d2_lo = to_float(extend_low(d2i));
                            auto d2_hi = to_float(extend_high(d2i));

                            auto t_lo = d2_lo * sstr_vector;
                            auto t_hi = d2_hi * sstr_vector;

                            auto da_mask_lo = abs(d1_lo) < abs(t_lo);
                            auto da_mask_hi = abs(d1_hi) < abs(t_hi);

                            auto da_lo = select(da_mask_lo, d1_lo, t_lo);
                            auto da_hi = select(da_mask_hi, d1_hi, t_hi);

                            auto desired_lo = da_lo * scl_vector;
                            auto desired_hi = da_hi * scl_vector;

                            auto fin_mask_lo = (d1_lo * t_lo) < 0.0f;
                            auto fin_mask_hi = (d1_hi * t_hi) < 0.0f;

                            auto add_lo = truncatei(select(fin_mask_lo, desired_lo, da_lo));
                            auto add_hi = truncatei(select(fin_mask_hi, desired_hi, da_hi));

                            auto add = compress_saturated(add_lo, add_hi);
                            auto df = b3 + Vec8us(add);

                            auto minm = src - amnt_vector;
                            auto maxf = src + amnt_vector;

                            df = max(df, minm);
                            df = min(df, maxf);

                            select(Vec16cb(ch), compress_saturated(df, zero), Vec16uc().loadl(srcp + x)).storel(dstp + x);
                        }
                    }
                    else
                    {
                        auto d2i = Vec8s(b3 - b6);

                        auto d1_lo = to_float(extend_low(d1i));
                        auto d1_hi = to_float(extend_high(d1i));

                        auto d2_lo = to_float(extend_low(d2i));
                        auto d2_hi = to_float(extend_high(d2i));

                        auto t_lo = d2_lo * sstr_vector;
                        auto t_hi = d2_hi * sstr_vector;

                        auto da_mask_lo = abs(d1_lo) < abs(t_lo);
                        auto da_mask_hi = abs(d1_hi) < abs(t_hi);

                        auto da_lo = select(da_mask_lo, d1_lo, t_lo);
                        auto da_hi = select(da_mask_hi, d1_hi, t_hi);

                        auto desired_lo = da_lo * scl_vector;
                        auto desired_hi = da_hi * scl_vector;

                        auto fin_mask_lo = (d1_lo * t_lo) < 0.0f;
                        auto fin_mask_hi = (d1_hi * t_hi) < 0.0f;

                        auto add_lo = truncatei(select(fin_mask_lo, desired_lo, da_lo));
                        auto add_hi = truncatei(select(fin_mask_hi, desired_hi, da_hi));

                        auto add = compress_saturated(add_lo, add_hi);
                        auto df = b3 + Vec8us(add);

                        auto minm = src - amnt_vector;
                        auto maxf = src + amnt_vector;

                        df = max(df, minm);
                        df = min(df, maxf);

                        auto result = compress_saturated(df, zero);
                        result.storel(dstp + x);
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
        auto zero = zero_si128();
        auto sstr_vector = Vec4f(sstr_);
        auto scl_vector = Vec4f(scl_);
        auto amnt_vector = Vec4i(amnt_);

        if constexpr (eclip)
        {
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < mod8_width; x += 4)
                {
                    auto b3 = Vec4ui().load_4us(pb3 + x);
                    auto b6 = Vec4ui().load_4us(pb6 + x);
                    auto src = Vec4ui().load_4us(srcp + x);

                    auto d1i = Vec4i(src - b3);

                    if constexpr (thresh)
                    {
                        auto ch = compress_saturated(abs(d1i) >= Vec4i(thr_), zero);

                        int64_t check;
                        ch.storel(&check);

                        if (!check)
                            Vec8us().load(srcp + x).storel(dstp + x);
                        else
                        {
                            auto d2i = Vec4i(src - b6);

                            auto d1 = to_float((d1i));
                            auto d2 = to_float((d2i));

                            auto da_mask = abs(d1) < abs(d2);
                            auto da = select(da_mask, d1, d2);

                            auto desired = da * scl_vector;
                            auto fin_mask = (d1 * d2) < 0.0f;

                            auto add = truncatei(select(fin_mask, desired, da));
                            auto df = b6 + Vec4i(add);

                            auto minm = src - amnt_vector;
                            auto maxf = src + amnt_vector;

                            df = max(df, minm);
                            df = min(df, maxf);

                            select(Vec8sb(ch), compress_saturated_s2u(df, zero), Vec8us().load(srcp + x)).storel(dstp + x);
                        }
                    }
                    else
                    {
                        auto d2i = Vec4i(src - b6);

                        auto d1 = to_float((d1i));
                        auto d2 = to_float((d2i));

                        auto da_mask = abs(d1) < abs(d2);
                        auto da = select(da_mask, d1, d2);

                        auto desired = da * scl_vector;
                        auto fin_mask = (d1 * d2) < 0.0f;

                        auto add = truncatei(select(fin_mask, desired, da));
                        auto df = b6 + Vec4i(add);

                        auto minm = src - amnt_vector;
                        auto maxf = src + amnt_vector;

                        df = max(df, minm);
                        df = min(df, maxf);

                        auto result = compress_saturated_s2u(df, zero);
                        result.storel(dstp + x);
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
                for (int x = 0; x < mod8_width; x += 4)
                {
                    auto b3 = Vec4ui().load_4us(pb3 + x);
                    auto b6 = Vec4ui().load_4us(pb6 + x);
                    auto src = Vec4ui().load_4us(srcp + x);

                    auto d1i = Vec4i(src - b3);

                    if constexpr (thresh)
                    {
                        auto ch = compress_saturated(abs(d1i) >= Vec4i(thr_), zero);

                        int64_t check;
                        ch.storel(&check);

                        if (!check)
                            Vec8us().load(srcp + x).storel(dstp + x);
                        else
                        {
                            auto d2i = Vec4i(b3 - b6);

                            auto d1 = to_float((d1i));
                            auto d2 = to_float((d2i));

                            auto t = d2 * sstr_vector;
                            auto da_mask = abs(d1) < abs(t);
                            auto da = select(da_mask, d1, t);

                            auto desired = da * scl_vector;
                            auto fin_mask = (d1 * t) < 0.0f;

                            auto add = truncatei(select(fin_mask, desired, da));
                            auto df = b3 + Vec4i(add);

                            auto minm = src - amnt_vector;
                            auto maxf = src + amnt_vector;

                            df = max(df, minm);
                            df = min(df, maxf);

                            select(Vec8sb(ch), compress_saturated_s2u(df, zero), Vec8us().load(srcp + x)).storel(dstp + x);
                        }
                    }
                    else
                    {
                        auto d2i = Vec4i(b3 - b6);

                        auto d1 = to_float((d1i));
                        auto d2 = to_float((d2i));

                        auto t = d2 * sstr_vector;
                        auto da_mask = abs(d1) < abs(t);
                        auto da = select(da_mask, d1, t);

                        auto desired = da * scl_vector;
                        auto fin_mask = (d1 * t) < 0.0f;

                        auto add = truncatei(select(fin_mask, desired, da));
                        auto df = b3 + Vec4i(add);

                        auto minm = src - amnt_vector;
                        auto maxf = src + amnt_vector;

                        df = max(df, minm);
                        df = min(df, maxf);

                        auto result = compress_saturated_s2u(df, zero);
                        result.storel(dstp + x);
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

template void Vinverse<uint8_t, VinverseMode::Vinverse, true, true>::finalize_plane_sse2
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint8_t, VinverseMode::Vinverse, false, true>::finalize_plane_sse2
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint8_t, VinverseMode::Vinverse, true, false>::finalize_plane_sse2
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint8_t, VinverseMode::Vinverse, false, false>::finalize_plane_sse2
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;

template void Vinverse<uint8_t, VinverseMode::Vinverse2, false, false>::finalize_plane_sse2
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;

template void Vinverse<uint16_t, VinverseMode::Vinverse, true, true>::finalize_plane_sse2
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint16_t, VinverseMode::Vinverse, false, true>::finalize_plane_sse2
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint16_t, VinverseMode::Vinverse, true, false>::finalize_plane_sse2
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint16_t, VinverseMode::Vinverse, false, false>::finalize_plane_sse2
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;

template void Vinverse<uint16_t, VinverseMode::Vinverse2, false, false>::finalize_plane_sse2
(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
