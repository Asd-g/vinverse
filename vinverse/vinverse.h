#pragma once

#include <memory>

#include "avisynth.h"

enum class VinverseMode
{
    Vinverse,
    Vinverse2
};

template <typename T, VinverseMode mode, bool eclip, bool thresh>
class Vinverse : public GenericVideoFilter
{
public:
    Vinverse(PClip child, float sstr, int amnt, int uv, float scl, int opt, PClip clip2, int thr, IScriptEnvironment* env);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

    int __stdcall SetCacheHints(int cachehints, int frame_range) override
    {
        return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
    }

private:
    float sstr_;
    int amnt_;
    int uv_;
    float scl_;
    int opt_;
    PClip clip2_;
    int thr_;

    T* blur3_buffer;
    T* blur6_buffer;

    int pb_pitch;
    std::unique_ptr<T[]> buffer;

    bool avx512;
    bool avx2;
    bool sse2;
    bool v8;

    void finalize_plane_c(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
    void finalize_plane_sse2(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
    void finalize_plane_avx2(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
    void finalize_plane_avx512(void* __restrict dstp_, const void* srcp_, const void* pb3_, const void* pb6_, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;

    void(*blur3)(void* dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
    void(*blur5)(void* dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
    void(*sbr)(void* dstp, void* tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
    void(Vinverse::* fin_plane)(void* dstp, const void* srcp, const void* pb3, const void* pb6, int src_pitch, int dst_pitch, int pb_pitch, int clip2_pitch, int width, int height) noexcept;
};

void vertical_blur3_sse2_8(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_blur5_sse2_8(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_sbr_sse2_8(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;

template <int c>
void vertical_blur3_sse2_16(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template <int c>
void vertical_blur5_sse2_16(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template <int c, int h, uint32_t u>
void vertical_sbr_sse2_16(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;

void vertical_blur3_avx2_8(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_blur5_avx2_8(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_sbr_avx2_8(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template <int c>
void vertical_blur3_avx2_16(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template <int c>
void vertical_blur5_avx2_16(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template <int c, int h, uint32_t u>
void vertical_sbr_avx2_16(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;

void vertical_blur3_avx512_8(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_blur5_avx512_8(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_sbr_avx512_8(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
template <int c>
void vertical_blur3_avx512_16(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template <int c>
void vertical_blur5_avx512_16(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template <int c, int h, uint32_t u>
void vertical_sbr_avx512_16(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch, int width, int height) noexcept;
