#include <algorithm>

#include "plugin.h"

template<typename T>
void copyPlane(void* __restrict dstp_, const int dstStride, const void* srcp_, const int srcStride, const int width, const int height) noexcept
{
    const T* srcp = reinterpret_cast<const T*>(srcp_);
    T* __restrict dstp = reinterpret_cast<T*>(dstp_);

    for (int y{ 0 }; y < height; ++y)
    {
        for (int x{ 0 }; x < width; ++x)
            dstp[x] = srcp[x];

        srcp += srcStride;
        dstp += dstStride;
    }
}

template void copyPlane<uint8_t>(void* __restrict dstp_, const int dstStride, const void* srcp_, const int srcStride, const int width,
    const int height) noexcept;
template void copyPlane<uint16_t>(void* __restrict dstp_, const int dstStride, const void* srcp_, const int srcStride, const int width,
    const int height) noexcept;

template <typename T>
void vertical_blur3_c(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const T* srcp = reinterpret_cast<const T*>(srcp_);
    T* __restrict dstp = reinterpret_cast<T*>(dstp_);

    for (int y = 0; y < height; ++y)
    {
        const T* srcpp = y == 0 ? srcp + src_pitch : srcp - src_pitch;
        const T* srcpn = y == height - 1 ? srcp - src_pitch : srcp + src_pitch;

        for (int x = 0; x < width; ++x)
            dstp[x] = (srcpp[x] + (srcp[x] << 1) + srcpn[x] + 2) >> 2;

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

template void vertical_blur3_c<uint8_t>(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width,
    int height) noexcept;
template void vertical_blur3_c<uint16_t>(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width,
    int height) noexcept;


template <typename T>
void vertical_blur5_c(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const T* srcp = reinterpret_cast<const T*>(srcp_);
    T* __restrict dstp = reinterpret_cast<T*>(dstp_);

    for (int y = 0; y < height; ++y)
    {
        const T* srcppp = y < 2 ? srcp + src_pitch * 2 : srcp - src_pitch * 2;
        const T* srcpp = y == 0 ? srcp + src_pitch : srcp - src_pitch;
        const T* srcpn = y == height - 1 ? srcp - src_pitch : srcp + src_pitch;
        const T* srcpnn = y > height - 3 ? srcp - src_pitch * 2 : srcp + src_pitch * 2;

        for (int x = 0; x < width; ++x)
            dstp[x] = (srcppp[x] + ((srcpp[x] + srcpn[x]) << 2) + srcp[x] * 6 + srcpnn[x] + 8) >> 4;

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

template void vertical_blur5_c<uint8_t>(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width,
    int height) noexcept;
template void vertical_blur5_c<uint16_t>(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width,
    int height) noexcept;

template <typename T, int p, int h>
static void mt_makediff_c(void* __restrict dstp_, const void* c1p_, const void* c2p_, int dst_pitch, int c1_pitch, int c2_pitch, int width, int height) noexcept
{
    const T* c1p = reinterpret_cast<const T*>(c1p_);
    const T* c2p = reinterpret_cast<const T*>(c2p_);
    T* __restrict dstp = reinterpret_cast<T*>(dstp_);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
            dstp[x] = std::max(std::min(c1p[x] - c2p[x] + h, p), 0);

        dstp += dst_pitch;
        c1p += c1_pitch;
        c2p += c2_pitch;
    }
}

template <typename T, int p, int h>
void vertical_sbr_c(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept
{
    vertical_blur3_c<T>(tempp_, srcp_, temp_pitch, src_pitch, width, height); //temp = rg11
    mt_makediff_c<T, p, h>(dstp_, srcp_, tempp_, dst_pitch, src_pitch, temp_pitch, width, height); //dst = rg11D
    vertical_blur3_c<T>(tempp_, dstp_, temp_pitch, dst_pitch, width, height); //temp = rg11D.vblur()

    const T* srcp = reinterpret_cast<const T*>(srcp_);
    T* __restrict tempp = reinterpret_cast<T*>(tempp_);
    T* __restrict dstp = reinterpret_cast<T*>(dstp_);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int t = dstp[x] - tempp[x];
            int t2 = dstp[x] - h;
            if (t * t2 < 0)
                dstp[x] = srcp[x];
            else
            {
                if (std::abs(t) < std::abs(t2))
                    dstp[x] = srcp[x] - t;
                else
                    dstp[x] = srcp[x] - dstp[x] + h;
            }
        }
        dstp += dst_pitch;
        srcp += src_pitch;
        tempp += temp_pitch;
    }
}

template void vertical_sbr_c<uint8_t, 255, 128>(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch,
    int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_c<uint8_t, 1023, 512>(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch,
    int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_c<uint8_t, 4095, 2048>(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch,
    int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_c<uint8_t, 16383, 8192>(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch,
    int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_c<uint8_t, 65535, 32768>(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch,
    int temp_pitch, int src_pitch, int width, int height) noexcept;

template void vertical_sbr_c<uint16_t, 255, 128>(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch,
    int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_c<uint16_t, 1023, 512>(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch,
    int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_c<uint16_t, 4095, 2048>(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch,
    int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_c<uint16_t, 16383, 8192>(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch,
    int temp_pitch, int src_pitch, int width, int height) noexcept;
template void vertical_sbr_c<uint16_t, 65535, 32768>(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch,
    int temp_pitch, int src_pitch, int width, int height) noexcept;

template <typename T, VinverseMode mode, bool eclip, bool thresh>
void Vinverse<T, mode, eclip, thresh>::finalize_plane_c(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept
{
    const T* srcp = reinterpret_cast<const T*>(srcp_);
    const T* pb3 = reinterpret_cast<const T*>(pb3_);
    const T* pb6 = reinterpret_cast<const T*>(pb6_);
    T* __restrict dstp = reinterpret_cast<T*>(dstp_);

    if constexpr (eclip)
    {
        constexpr auto peak = std::numeric_limits<T>::max();

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const float d1 = static_cast<float>(srcp[x] - pb3[x]);

                if constexpr (thresh)
                {
                    if (std::abs(d1) < thr_)
                        dstp[x] = srcp[x];
                    else
                    {
                        const float d2 = static_cast<float>(srcp[x] - pb6[x]);

                        const float da = (std::abs(d1) < std::abs(d2)) ? d1 : d2;
                        const float desired = da * scl_;

                        const int add = static_cast<int>(((d1 * d2) < 0.0f) ? desired : da);
                        int df = pb6[x] + add;

                        const int minm = srcp[x] - amnt_;
                        const int maxf = srcp[x] + amnt_;

                        dstp[x] = std::clamp(std::clamp(df, minm, maxf), 0, static_cast<int>(peak));
                    }
                }
                else
                {
                    const float d2 = static_cast<float>(srcp[x] - pb6[x]);

                    const float da = (std::abs(d1) < std::abs(d2)) ? d1 : d2;
                    const float desired = da * scl_;

                    const int add = static_cast<int>(((d1 * d2) < 0.0f) ? desired : da);
                    int df = pb6[x] + add;

                    const int minm = srcp[x] - amnt_;
                    const int maxf = srcp[x] + amnt_;

                    dstp[x] = std::clamp(std::clamp(df, minm, maxf), 0, static_cast<int>(peak));
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
            for (int x = 0; x < width; ++x)
            {
                const float d1 = static_cast<float>(srcp[x] - pb3[x]);

                if constexpr (thresh)
                {
                    if (std::abs(d1) < thr_)
                        dstp[x] = srcp[x];
                    else
                    {
                        const int d2 = pb3[x] - pb6[x];
                        const float t = d2 * sstr_;

                        const float da = (std::abs(d1) < std::abs(t)) ? d1 : t;
                        const float desired = da * scl_;

                        const int add = static_cast<int>(((d1 * t) < 0.0f) ? desired : da);
                        int df = pb3[x] + add;

                        const int minm = srcp[x] - amnt_;
                        const int maxf = srcp[x] + amnt_;

                        dstp[x] = std::clamp(df, minm, maxf);
                    }
                }
                else
                {
                    const int d2 = pb3[x] - pb6[x];
                    const float t = d2 * sstr_;

                    const float da = (std::abs(d1) < std::abs(t)) ? d1 : t;
                    const float desired = da * scl_;

                    const int add = static_cast<int>(((d1 * t) < 0.0f) ? desired : da);
                    int df = pb3[x] + add;

                    const int minm = srcp[x] - amnt_;
                    const int maxf = srcp[x] + amnt_;

                    dstp[x] = std::clamp(df, minm, maxf);
                }
            }

            srcp += src_pitch;
            pb3 += pb_pitch;
            pb6 += pb_pitch;
            dstp += dst_pitch;
        }
    }
}

template void Vinverse<uint8_t, VinverseMode::Vinverse, true, true>::finalize_plane_c(void* __restrict dstp_, const void* srcp_,
    const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint8_t, VinverseMode::Vinverse, false, true>::finalize_plane_c(void* __restrict dstp_, const void* srcp_,
    const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint8_t, VinverseMode::Vinverse, true, false>::finalize_plane_c(void* __restrict dstp_, const void* srcp_,
    const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint8_t, VinverseMode::Vinverse, false, false>::finalize_plane_c(void* __restrict dstp_, const void* srcp_,
    const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;

template void Vinverse<uint8_t, VinverseMode::Vinverse2, false, false>::finalize_plane_c(void* __restrict dstp_, const void* srcp_,
    const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;

template void Vinverse<uint16_t, VinverseMode::Vinverse, true, true>::finalize_plane_c(void* __restrict dstp_, const void* srcp_,
    const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint16_t, VinverseMode::Vinverse, false, true>::finalize_plane_c(void* __restrict dstp_, const void* srcp_,
    const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint16_t, VinverseMode::Vinverse, true, false>::finalize_plane_c(void* __restrict dstp_, const void* srcp_,
    const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
template void Vinverse<uint16_t, VinverseMode::Vinverse, false, false>::finalize_plane_c(void* __restrict dstp_, const void* srcp_,
    const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;

template void Vinverse<uint16_t, VinverseMode::Vinverse2, false, false>::finalize_plane_c(void* __restrict dstp_, const void* srcp_,
    const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
