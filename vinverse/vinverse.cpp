#include <algorithm>
#include <emmintrin.h>
#include <string>

#include "vinverse.h"

#ifdef _MSC_VER 
#define WIN32_LEAN_AND_MEAN
#endif

AVS_FORCEINLINE void* aligned_malloc(size_t size, size_t align)
{
    void* result = [&]() {
#ifdef _MSC_VER 
        return _aligned_malloc(size, align);
#else 
        if (posix_memalign(&result, align, size))
            return result = nullptr;
        else
            return result;
#endif
    }();

    return result;
}

AVS_FORCEINLINE void aligned_free(void* ptr)
{
#ifdef _MSC_VER 
    _aligned_free(ptr);
#else 
    free(ptr);
#endif
}

template<typename T>
static void copyPlane(void* __restrict dstp_, const int dstStride, const void* srcp_, const int srcStride, const int width, const int height) noexcept
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

template <typename T, int c>
static void vertical_blur3_c(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
{
    const T* srcp = reinterpret_cast<const T*>(srcp_);
    T* __restrict dstp = reinterpret_cast<T*>(dstp_);

    for (int y = 0; y < height; ++y)
    {
        const T* srcpp = y == 0 ? srcp + src_pitch : srcp - src_pitch;
        const T* srcpn = y == height - 1 ? srcp - src_pitch : srcp + src_pitch;

        for (int x = 0; x < width; ++x)
            dstp[x] = (srcpp[x] + (srcp[x] << 1) + srcpn[x] + c) >> 2;

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

template <typename T, int c>
static void vertical_blur5_c(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept
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
            dstp[x] = (srcppp[x] + ((srcpp[x] + srcpn[x]) << 2) + srcp[x] * 6 + srcpnn[x] + c) >> 4;

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

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

template <typename T, int c, int p, int h>
static void vertical_sbr_c(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept
{
    vertical_blur3_c<T, c>(tempp_, srcp_, temp_pitch, src_pitch, width, height); //temp = rg11
    mt_makediff_c<T, p, h>(dstp_, srcp_, tempp_, dst_pitch, src_pitch, temp_pitch, width, height); //dst = rg11D
    vertical_blur3_c<T, c>(tempp_, dstp_, temp_pitch, dst_pitch, width, height); //temp = rg11D.vblur()

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

template <typename T, VinverseMode mode, bool eclip, bool thresh>
void Vinverse<T, mode, eclip, thresh>::finalize_plane_c(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept
{
    const T* srcp = reinterpret_cast<const T*>(srcp_);
    const T* pb3 = reinterpret_cast<const T*>(pb3_);
    const T* pb6 = reinterpret_cast<const T*>(pb6_);
    T* __restrict dstp = reinterpret_cast<T*>(dstp_);

    if constexpr (eclip)
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
                        const float d2 = static_cast<float>(srcp[x] - pb6[x]);

                        const float da = (std::abs(d1) < std::abs(d2)) ? d1 : d2;
                        const float desired = da * scl_;

                        const int add = static_cast<int>(((d1 * d2) < 0.0f) ? desired : da);
                        int df = pb6[x] + add;

                        const int minm = srcp[x] - amnt_;
                        const int maxf = srcp[x] + amnt_;

                        df = std::max(df, minm);
                        dstp[x] = std::min(df, maxf);
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

                    df = std::max(df, minm);
                    dstp[x] = std::min(df, maxf);
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

                        df = std::max(df, minm);
                        dstp[x] = std::min(df, maxf);
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

                    df = std::max(df, minm);
                    dstp[x] = std::min(df, maxf);
                }
            }

            srcp += src_pitch;
            pb3 += pb_pitch;
            pb6 += pb_pitch;
            dstp += dst_pitch;
        }
    }
}

template <typename T, VinverseMode mode, bool eclip, bool thresh>
Vinverse<T, mode, eclip, thresh>::Vinverse(PClip child, float sstr, int amnt, int uv, float scl, int opt, PClip clip2, int thr, IScriptEnvironment* env)
    : GenericVideoFilter(child), sstr_(sstr), amnt_(amnt), uv_(uv), scl_(scl), opt_(opt), clip2_(clip2), thr_(thr), blur3_buffer(nullptr), blur6_buffer(nullptr)
{
    if (!vi.IsPlanar())
        env->ThrowError("Vinverse: only planar input is supported!");
    if (vi.IsRGB())
        env->ThrowError("Vinverse: only YUV input is supported!");

    const int peak = (1 << vi.BitsPerComponent()) - 1;

    amnt_ = (amnt_ == -1) ? peak : amnt_;
    if (amnt_ < 1 || amnt_ > peak)
        env->ThrowError("Vinverse: amnt must be greater than 0 and less than or equal to %s!", std::to_string(peak).c_str());
    if (uv < 1 || uv > 3)
        env->ThrowError("Vinverse: uv must be set to 1, 2, or 3!");
    if (opt_ < -1 || opt_ > 3)
        env->ThrowError("Vinverse: opt must be between -1..3.");

    avx512 = !!(env->GetCPUFlags() & CPUF_AVX512F);
    avx2 = !!(env->GetCPUFlags() & CPUF_AVX2);
    sse2 = !!(env->GetCPUFlags() & CPUF_SSE2);

    if (!avx512 && opt_ == 3)
        env->ThrowError("Vinverse: opt=3 requires AVX512F.");
    if (!avx2 && opt_ == 2)
        env->ThrowError("Vinverse: opt=2 requires AVX2.");
    if (!sse2 && opt_ == 1)
        env->ThrowError("Vinverse: opt=1 requires SSE2.");

    if constexpr (eclip)
    {
        const VideoInfo& vi_ = clip2_->GetVideoInfo();

        if (!vi.IsSameColorspace(vi_))
            env->ThrowError("Vinverse: clip2's colorspace doesn't match.");
        if (vi.width != vi_.width || vi.height != vi_.height)
            env->ThrowError("Vinverse: input and clip2 must be the same resolution.");
        if (vi.num_frames != vi_.num_frames)
            env->ThrowError("Vinverse: clip2's number of frames doesn't match.");
    }

    if (thr_ < 0 || thr_ > peak)
        env->ThrowError("Vinverse: thr must be between 0..%s!", std::to_string(peak).c_str());

    if ((avx512 && opt_ < 0) || opt_ == 3)
    {
        pb_pitch = (vi.width + 63) & ~63;

        if (sizeof(T) == 1)
        {
            blur3 = vertical_blur3_avx512_8;
            blur5 = vertical_blur5_avx512_8;
            sbr = vertical_sbr_avx512_8;
        }
        else
        {
            switch (vi.BitsPerComponent())
            {
                case 10:
                {
                    blur3 = vertical_blur3_avx512_16<8>;
                    blur5 = vertical_blur5_avx512_16<32>;
                    sbr = vertical_sbr_avx512_16<8, 512, 0x200200>;
                    break;
                }
                case 12:
                {
                    blur3 = vertical_blur3_avx512_16<32>;
                    blur5 = vertical_blur5_avx512_16<128>;
                    sbr = vertical_sbr_avx512_16<32, 2048, 0x800800>;
                    break;
                }
                case 14:
                {
                    blur3 = vertical_blur3_avx512_16<128>;
                    blur5 = vertical_blur5_avx512_16<512>;
                    sbr = vertical_sbr_avx512_16<128, 8192, 0x20002000>;
                    break;
                }
                default:
                {
                    blur3 = vertical_blur3_avx512_16<512>;
                    blur5 = vertical_blur5_avx512_16<2048>;
                    sbr = vertical_sbr_avx512_16<512, 32768, 0x80008000>;
                    break;
                }
            }
        }

        fin_plane = &Vinverse::finalize_plane_avx512;
    }
    else if ((avx2 && opt_ < 0) || opt_ == 2)
    {
        pb_pitch = (vi.width + 31) & ~31;

        if (sizeof(T) == 1)
        {
            blur3 = vertical_blur3_avx2_8;
            blur5 = vertical_blur5_avx2_8;
            sbr = vertical_sbr_avx2_8;
        }
        else
        {
            switch (vi.BitsPerComponent())
            {
                case 10:
                {
                    blur3 = vertical_blur3_avx2_16<8>;
                    blur5 = vertical_blur5_avx2_16<32>;
                    sbr = vertical_sbr_avx2_16<8, 512, 0x200200>;
                    break;
                }
                case 12:
                {
                    blur3 = vertical_blur3_avx2_16<32>;
                    blur5 = vertical_blur5_avx2_16<128>;
                    sbr = vertical_sbr_avx2_16<32, 2048, 0x800800>;
                    break;
                }
                case 14:
                {
                    blur3 = vertical_blur3_avx2_16<128>;
                    blur5 = vertical_blur5_avx2_16<512>;
                    sbr = vertical_sbr_avx2_16<128, 8192, 0x20002000>;
                    break;
                }
                default:
                {
                    blur3 = vertical_blur3_avx2_16<512>;
                    blur5 = vertical_blur5_avx2_16<2048>;
                    sbr = vertical_sbr_avx2_16<512, 32768, 0x80008000>;
                    break;
                }
            }
        }

        fin_plane = &Vinverse::finalize_plane_avx2;
    }
    else if ((sse2 && opt_ < 0) || opt_ == 1)
    {
        pb_pitch = (vi.width + 15) & ~15;

        if (sizeof(T) == 1)
        {
            blur3 = vertical_blur3_sse2_8;
            blur5 = vertical_blur5_sse2_8;
            sbr = vertical_sbr_sse2_8;
        }
        else
        {
            switch (vi.BitsPerComponent())
            {
                case 10:
                {
                    blur3 = vertical_blur3_sse2_16<8>;
                    blur5 = vertical_blur5_sse2_16<32>;
                    sbr = vertical_sbr_sse2_16<8, 512, 0x200200>;
                    break;
                }
                case 12:
                {
                    blur3 = vertical_blur3_sse2_16<32>;
                    blur5 = vertical_blur5_sse2_16<128>;
                    sbr = vertical_sbr_sse2_16<32, 2048, 0x800800>;
                    break;
                }
                case 14:
                {
                    blur3 = vertical_blur3_sse2_16<128>;
                    blur5 = vertical_blur5_sse2_16<512>;
                    sbr = vertical_sbr_sse2_16<128, 8192, 0x20002000>;
                    break;
                }
                default:
                {
                    blur3 = vertical_blur3_sse2_16<512>;
                    blur5 = vertical_blur5_sse2_16<2048>;
                    sbr = vertical_sbr_sse2_16<512, 32768, 0x80008000>;
                    break;
                }
            }
        }

        fin_plane = &Vinverse::finalize_plane_sse2;
    }
    else
    {
        pb_pitch = (vi.width + 15) & ~15;

        if (sizeof(T) == 1)
        {
            blur3 = vertical_blur3_c<T, 2>;
            blur5 = vertical_blur5_c<T, 8>;
            sbr = vertical_sbr_c<T, 2, 255, 128>;
        }
        else
        {
            switch (vi.BitsPerComponent())
            {
                case 10:
                {
                    blur3 = vertical_blur3_c<T, 8>;
                    blur5 = vertical_blur5_c<T, 32>;
                    sbr = vertical_sbr_c<T, 8, 1023, 512>;
                    break;
                }
                case 12:
                {
                    blur3 = vertical_blur3_c<T, 32>;
                    blur5 = vertical_blur5_c<T, 128>;
                    sbr = vertical_sbr_c<T, 32, 4095, 2048>;
                    break;
                }
                case 14:
                {
                    blur3 = vertical_blur3_c<T, 128>;
                    blur5 = vertical_blur5_c<T, 512>;
                    sbr = vertical_sbr_c<T, 128, 16383, 8192>;
                    break;
                }
                default:
                {
                    blur3 = vertical_blur3_c<T, 512>;
                    blur5 = vertical_blur5_c<T, 2048>;
                    sbr = vertical_sbr_c<T, 512, 65535, 32768>;
                    break;
                }
            }
        }

        fin_plane = &Vinverse::finalize_plane_c;
    }

    size_t pbuf_size = vi.height * pb_pitch;

    buffer = std::make_unique<T[]>(pbuf_size * 2 * sizeof(T));

    if (buffer == nullptr)
        env->ThrowError("Vinverse:  malloc failure!");

    blur3_buffer = buffer.get();
    blur6_buffer = blur3_buffer + pbuf_size;

    v8 = true;
    try { env->CheckVersion(8); }
    catch (const AvisynthError&) { v8 = false; }
}

template <typename T, VinverseMode mode, bool eclip, bool thresh>
PVideoFrame __stdcall Vinverse<T, mode, eclip, thresh>::GetFrame(int n, IScriptEnvironment* env)
{
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame dst = (v8) ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi);

    int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };

    for (int pid = 0; pid < 3; ++pid)
    {
        int current_plane = planes[pid];
        if (current_plane != PLANAR_Y && (vi.IsY8() || uv_ == 1))
            continue;

        const uint8_t* srcp = src->GetReadPtr(current_plane);
        const int src_pitch = src->GetPitch(current_plane) / sizeof(T);
        const int dst_pitch = dst->GetPitch(current_plane);
        const int height = src->GetHeight(current_plane);
        const int width = src->GetRowSize(current_plane) / sizeof(T);
        uint8_t* dstp = dst->GetWritePtr(current_plane);

        if (current_plane != PLANAR_Y && uv_ == 2)
        {
            env->BitBlt(dstp, dst_pitch, srcp, src->GetPitch(current_plane), src->GetRowSize(current_plane), height);
            continue;
        }

        if constexpr (mode == VinverseMode::Vinverse)
        {
            blur3(blur3_buffer, srcp, pb_pitch, src_pitch, width, height);

            if (!eclip)
                blur5(blur6_buffer, blur3_buffer, pb_pitch, pb_pitch, width, height);
        }
        else
        {
            if (current_plane == PLANAR_Y)
                sbr(blur3_buffer, blur6_buffer, srcp, pb_pitch, pb_pitch, src_pitch, width, height);
            else
                copyPlane<T>(blur3_buffer, pb_pitch, srcp, src_pitch, width, height);
            blur3(blur6_buffer, blur3_buffer, pb_pitch, pb_pitch, width, height);
        }

        if constexpr (eclip)
        {
            PVideoFrame clp2 = clip2_->GetFrame(n, env);

            (this->*fin_plane)(dstp, srcp, blur3_buffer, clp2->GetReadPtr(current_plane), src_pitch,
                dst_pitch / sizeof(T), pb_pitch, clp2->GetPitch(current_plane) / sizeof(T), width, height);
        }
        else
            (this->*fin_plane)(dst->GetWritePtr(current_plane), srcp, blur3_buffer, blur6_buffer, src_pitch, dst_pitch / sizeof(T), pb_pitch, 0, width, height);
    }

    return dst;
}

AVSValue __cdecl Create_Vinverse(AVSValue args, void*, IScriptEnvironment* env)
{
    enum { CLIP, SSTR, AMNT, UV, SCL, OPT, CLIP2, THR };

    PClip clip = args[CLIP].AsClip();
    PClip clip2 = (args[CLIP2].Defined()) ? args[CLIP2].AsClip() : nullptr;
    const int thresh = args[THR].AsInt(0);

    if (clip2)
    {
        if (thresh > 0)
        {
            switch (clip->GetVideoInfo().ComponentSize())
            {
                case 1: return new Vinverse<uint8_t, VinverseMode::Vinverse, true, true>(clip, args[SSTR].AsFloatf(2.7f), args[AMNT].AsInt(-1), args[UV].AsInt(3), args[SCL].AsFloatf(0.25f), args[OPT].AsInt(-1), clip2, thresh, env);
                case 2: return new Vinverse<uint16_t, VinverseMode::Vinverse, true, true>(clip, args[SSTR].AsFloatf(2.7f), args[AMNT].AsInt(-1), args[UV].AsInt(3), args[SCL].AsFloatf(0.25f), args[OPT].AsInt(-1), clip2, thresh, env);
                default: env->ThrowError("Vinverse: only 8..16-bit input is supported!");
            }
        }
        else
        {
            switch (clip->GetVideoInfo().ComponentSize())
            {
                case 1: return new Vinverse<uint8_t, VinverseMode::Vinverse, true, false>(clip, args[SSTR].AsFloatf(2.7f), args[AMNT].AsInt(-1), args[UV].AsInt(3), args[SCL].AsFloatf(0.25f), args[OPT].AsInt(-1), clip2, 0, env);
                case 2: return new Vinverse<uint16_t, VinverseMode::Vinverse, true, false>(clip, args[SSTR].AsFloatf(2.7f), args[AMNT].AsInt(-1), args[UV].AsInt(3), args[SCL].AsFloatf(0.25f), args[OPT].AsInt(-1), clip2, 0, env);
                default: env->ThrowError("Vinverse: only 8..16-bit input is supported!");
            }
        }
    }
    else
    {
        if (thresh > 0)
        {
            switch (clip->GetVideoInfo().ComponentSize())
            {
                case 1: return new Vinverse<uint8_t, VinverseMode::Vinverse, false, true>(clip, args[SSTR].AsFloatf(2.7f), args[AMNT].AsInt(-1), args[UV].AsInt(3), args[SCL].AsFloatf(0.25f), args[OPT].AsInt(-1), clip2, thresh, env);
                case 2: return new Vinverse<uint16_t, VinverseMode::Vinverse, false, true>(clip, args[SSTR].AsFloatf(2.7f), args[AMNT].AsInt(-1), args[UV].AsInt(3), args[SCL].AsFloatf(0.25f), args[OPT].AsInt(-1), clip2, thresh, env);
                default: env->ThrowError("Vinverse: only 8..16-bit input is supported!");
            }
        }
        else
        {
            switch (clip->GetVideoInfo().ComponentSize())
            {
                case 1: return new Vinverse<uint8_t, VinverseMode::Vinverse, false, false>(clip, args[SSTR].AsFloatf(2.7f), args[AMNT].AsInt(-1), args[UV].AsInt(3), args[SCL].AsFloatf(0.25f), args[OPT].AsInt(-1), clip2, 0, env);
                case 2: return new Vinverse<uint16_t, VinverseMode::Vinverse, false, false>(clip, args[SSTR].AsFloatf(2.7f), args[AMNT].AsInt(-1), args[UV].AsInt(3), args[SCL].AsFloatf(0.25f), args[OPT].AsInt(-1), clip2, 0, env);
                default: env->ThrowError("Vinverse: only 8..16-bit input is supported!");
            }
        }
    }
}

AVSValue __cdecl Create_Vinverse2(AVSValue args, void*, IScriptEnvironment* env)
{
    enum { CLIP, SSTR, AMNT, UV, SCL, OPT, CLIP2, THR };

    PClip clip = args[CLIP].AsClip();

    switch (clip->GetVideoInfo().ComponentSize())
    {
        case 1: return new Vinverse<uint8_t, VinverseMode::Vinverse2, false, false>(clip, args[SSTR].AsFloatf(2.7f), args[AMNT].AsInt(-1), args[UV].AsInt(3), args[SCL].AsFloatf(0.25f), args[OPT].AsInt(-1), nullptr, 0, env);
        case 2: return new Vinverse<uint16_t, VinverseMode::Vinverse2, false, false>(clip, args[SSTR].AsFloatf(2.7f), args[AMNT].AsInt(-1), args[UV].AsInt(3), args[SCL].AsFloatf(0.25f), args[OPT].AsInt(-1), nullptr, 0, env);
        default: env->ThrowError("Vinverse: only 8..16-bit input is supported!");
    }
}

const AVS_Linkage* AVS_linkage = nullptr;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment * env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    env->AddFunction("vinverse", "c[sstr]f[amnt]i[uv]i[scl]f[opt]i[clip2]c[thr]i", Create_Vinverse, 0);
    env->AddFunction("vinverse2", "c[sstr]f[amnt]i[uv]i[scl]f[opt]i", Create_Vinverse2, 0);
    return "Doushimashita?";
}
