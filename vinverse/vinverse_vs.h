#pragma once

#include <algorithm>

#include "common.h"
#include "VapourSynth4.h"
#include "VSHelper4.h"

#ifdef HAS_SSE2
#if defined(__amd64__) || defined(__i386__) || defined(_M_IX86) || defined(_M_X64)
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif
#endif

template <typename T, VinverseMode mode, bool eclip, bool thresh>
class Vinverse;

template <typename T, VinverseMode mode, bool eclip, bool thresh>
const VSFrame* VS_CC vinverse_get_frame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx,
    VSCore* core, const VSAPI* vsapi);

template <typename T, VinverseMode mode, bool eclip, bool thresh>
void VS_CC vinverse_free(void* instanceData, VSCore* core, const VSAPI* vsapi);

template <typename T, VinverseMode mode, bool eclip, bool thresh>
void create_vinverse(VSMap* out, VSNode* clip, float sstr, int amnt, int uv, float scl, int opt, VSNode* clip2, int thr,
    VSCore* core, const VSAPI* vsapi);

#ifdef HAS_SSE2
struct CPUFlags
{
    bool sse2 = false;
    bool avx2 = false;
    bool avx512f = false;
};

static CPUFlags get_cpu_flags()
{
    CPUFlags flags;
#if defined(__amd64__) || defined(__i386__) || defined(_M_IX86) || defined(_M_X64)
#ifdef _MSC_VER
    int info[4];
    __cpuid(info, 0);
    if (info[0] >= 1)
    {
        __cpuid(info, 1);
        flags.sse2 = (info[3] & ((int)1 << 26)) != 0;
    }
    if (info[0] >= 7)
    {
        __cpuidex(info, 7, 0);
        flags.avx2 = (info[1] & ((int)1 << 5)) != 0;
        flags.avx512f = (info[1] & ((int)1 << 16)) != 0;
    }
#else
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid_max(0, nullptr) >= 1)
    {
        __cpuid_count(1, 0, eax, ebx, ecx, edx);
        flags.sse2 = (edx & (1 << 26)) != 0;
    }
    if (__get_cpuid_max(0, nullptr) >= 7)
    {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
        flags.avx2 = (ebx & (1 << 5)) != 0;
        flags.avx512f = (ebx & (1 << 16)) != 0;
    }
#endif
#endif
    return flags;
}
#endif

template <typename T, VinverseMode mode, bool eclip, bool thresh>
class Vinverse
{
    friend const VSFrame* VS_CC vinverse_get_frame<T, mode, eclip, thresh>(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi);
    friend void create_vinverse<T, mode, eclip, thresh>(VSMap* out, VSNode* clip, float sstr, int amnt, int uv, float scl, int opt, VSNode* clip2, int thr,
        VSCore* core, const VSAPI* vsapi);
    friend void VS_CC vinverse_free<T, mode, eclip, thresh>(void* instanceData, VSCore* core, const VSAPI* vsapi);

public:
    Vinverse(VSNode* child, float sstr, int amnt, int uv, float scl, int opt, VSNode* clip2, int thr, const VSAPI* vsapi)
        : sstr_(sstr), amnt_(amnt), uv_(uv), scl_(scl), child_(child), clip2_(clip2), thr_(thr)
    {
        vi = vsapi->getVideoInfo(child_);

#ifdef HAS_SSE2
        CPUFlags cpu = get_cpu_flags();

        if ((cpu.avx512f && opt < 0) || opt == 3)
        {
            pb_pitch = (vi->width + 63) & ~63;

            if constexpr (sizeof(T) == 1)
            {
                blur3 = vertical_blur3_avx512_8;
                blur5 = vertical_blur5_avx512_8;
                sbr = vertical_sbr_avx512_8;
            }
            else
            {
                switch (vi->format.bitsPerSample)
                {
                case 10:
                    blur3 = vertical_blur3_avx512_16;
                    blur5 = vertical_blur5_avx512_16;
                    sbr = vertical_sbr_avx512_16<512, 0x200200>;
                    break;
                case 12:
                    blur3 = vertical_blur3_avx512_16;
                    blur5 = vertical_blur5_avx512_16;
                    sbr = vertical_sbr_avx512_16<2048, 0x800800>;
                    break;
                case 14:
                    blur3 = vertical_blur3_avx512_16;
                    blur5 = vertical_blur5_avx512_16;
                    sbr = vertical_sbr_avx512_16<8192, 0x20002000>;
                    break;
                default:
                    blur3 = vertical_blur3_avx512_16;
                    blur5 = vertical_blur5_avx512_16;
                    sbr = vertical_sbr_avx512_16<32768, 0x80008000>;
                    break;
                }
            }
            fin_plane = &Vinverse::finalize_plane_avx512;
        }
        else if ((cpu.avx2 && opt < 0) || opt == 2)
        {
            pb_pitch = (vi->width + 31) & ~31;

            if constexpr (sizeof(T) == 1)
            {
                blur3 = vertical_blur3_avx2_8;
                blur5 = vertical_blur5_avx2_8;
                sbr = vertical_sbr_avx2_8;
            }
            else
            {
                switch (vi->format.bitsPerSample)
                {
                case 10:
                    blur3 = vertical_blur3_avx2_16;
                    blur5 = vertical_blur5_avx2_16;
                    sbr = vertical_sbr_avx2_16<512, 0x200200>;
                    break;
                case 12:
                    blur3 = vertical_blur3_avx2_16;
                    blur5 = vertical_blur5_avx2_16;
                    sbr = vertical_sbr_avx2_16<2048, 0x800800>;
                    break;
                case 14:
                    blur3 = vertical_blur3_avx2_16;
                    blur5 = vertical_blur5_avx2_16;
                    sbr = vertical_sbr_avx2_16<8192, 0x20002000>;
                    break;
                default:
                    blur3 = vertical_blur3_avx2_16;
                    blur5 = vertical_blur5_avx2_16;
                    sbr = vertical_sbr_avx2_16<32768, 0x80008000>;
                    break;
                }
            }
            fin_plane = &Vinverse::finalize_plane_avx2;
        }
        else if ((cpu.sse2 && opt < 0) || opt == 1)
        {
            pb_pitch = (vi->width + 15) & ~15;

            if constexpr (sizeof(T) == 1)
            {
                blur3 = vertical_blur3_sse2_8;
                blur5 = vertical_blur5_sse2_8;
                sbr = vertical_sbr_sse2_8;
            }
            else
            {
                switch (vi->format.bitsPerSample)
                {
                case 10:
                    blur3 = vertical_blur3_sse2_16;
                    blur5 = vertical_blur5_sse2_16;
                    sbr = vertical_sbr_sse2_16<512, 0x200200>;
                    break;
                case 12:
                    blur3 = vertical_blur3_sse2_16;
                    blur5 = vertical_blur5_sse2_16;
                    sbr = vertical_sbr_sse2_16<2048, 0x800800>;
                    break;
                case 14:
                    blur3 = vertical_blur3_sse2_16;
                    blur5 = vertical_blur5_sse2_16;
                    sbr = vertical_sbr_sse2_16<8192, 0x20002000>;
                    break;
                default:
                    blur3 = vertical_blur3_sse2_16;
                    blur5 = vertical_blur5_sse2_16;
                    sbr = vertical_sbr_sse2_16<32768, 0x80008000>;
                    break;
                }
            }
            fin_plane = &Vinverse::finalize_plane_sse2;
        }
        else
#endif
        {
            pb_pitch = (vi->width + 15) & ~15;

            if constexpr (sizeof(T) == 1)
            {
                blur3 = vertical_blur3_c<T>;
                blur5 = vertical_blur5_c<T>;
                sbr = vertical_sbr_c<T, 255, 128>;
            }
            else
            {
                switch (vi->format.bitsPerSample)
                {
                case 10:
                    blur3 = vertical_blur3_c<T>;
                    blur5 = vertical_blur5_c<T>;
                    sbr = vertical_sbr_c<T, 1023, 512>;
                    break;
                case 12:
                    blur3 = vertical_blur3_c<T>;
                    blur5 = vertical_blur5_c<T>;
                    sbr = vertical_sbr_c<T, 4095, 2048>;
                    break;
                case 14:
                    blur3 = vertical_blur3_c<T>;
                    blur5 = vertical_blur5_c<T>;
                    sbr = vertical_sbr_c<T, 16383, 8192>;
                    break;
                default:
                    blur3 = vertical_blur3_c<T>;
                    blur5 = vertical_blur5_c<T>;
                    sbr = vertical_sbr_c<T, 65535, 32768>;
                    break;
                }
            }
            fin_plane = &Vinverse::finalize_plane_c;
        }
    }

    void do_blur3(void* dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) const
    {
        blur3(dstp, srcp, dst_pitch, src_pitch, width, height);
    }
    void do_blur5(void* dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) const
    {
        blur5(dstp, srcp, dst_pitch, src_pitch, width, height);
    }
    void do_sbr(void* dstp, void* tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) const
    {
        sbr(dstp, tempp, srcp, dst_pitch, temp_pitch, src_pitch, width, height);
    }
    void do_fin_plane(void* dstp, const void* srcp, const void* pb3, const void* pb6, int src_pitch, int dst_pitch, int pb_pitch,
        int clip2_pitch, int width, int height)
    {
        (this->*fin_plane)(dstp, srcp, pb3, pb6, src_pitch, dst_pitch, pb_pitch, clip2_pitch, width, height);
    }

private:
    VSNode* child_;
    VSNode* clip2_;
    const VSVideoInfo* vi;
    int uv_;
    int pb_pitch;
    float sstr_;
    int amnt_;
    float scl_;
    int thr_;

    void finalize_plane_c(void* __restrict dstp, const void* srcp, const void* pb3, const void* pb6, int src_pitch, int dst_pitch,
        int pb_pitch, int clip2_pitch, int width, int height) noexcept;
    void finalize_plane_sse2(void* __restrict dstp, const void* srcp, const void* pb3, const void* pb6, int src_pitch, int dst_pitch,
        int pb_pitch, int clip2_pitch, int width, int height) noexcept;
    void finalize_plane_avx2(void* __restrict dstp, const void* srcp, const void* pb3, const void* pb6, int src_pitch, int dst_pitch,
        int pb_pitch, int clip2_pitch, int width, int height) noexcept;
    void finalize_plane_avx512(void* __restrict dstp, const void* srcp, const void* pb3, const void* pb6, int src_pitch, int dst_pitch,
        int pb_pitch, int clip2_pitch, int width, int height) noexcept;

    void(*blur3)(void* dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
    void(*blur5)(void* dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
    void(*sbr)(void* dstp, void* tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
    void(Vinverse::* fin_plane)(void* dstp, const void* srcp, const void* pb3, const void* pb6, int src_pitch, int dst_pitch, int pb_pitch,
        int clip2_pitch, int width, int height) noexcept;
};
