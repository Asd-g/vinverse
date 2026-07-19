#pragma once

#include <cstdint>

enum class VinverseMode
{
    Vinverse,
    Vinverse2
};

template<typename T>
void copyPlane(void* __restrict dstp_, const int dstStride, const void* srcp_, const int srcStride, const int width,
    const int height) noexcept;

template <typename T>
void vertical_blur3_c(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept;
template <typename T>
void vertical_blur5_c(void* __restrict dstp_, const void* srcp_, int dst_pitch, int src_pitch, int width, int height) noexcept;
template <typename T, int p, int h>
void vertical_sbr_c(void* __restrict dstp_, void* __restrict tempp_, const void* srcp_, int dst_pitch, int temp_pitch, int src_pitch,
    int width, int height) noexcept;

#ifdef HAS_SSE2
void vertical_blur3_sse2_8(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_blur5_sse2_8(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_sbr_sse2_8(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch,
    int width, int height) noexcept;

void vertical_blur3_sse2_16(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_blur5_sse2_16(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template <int h, uint32_t u>
void vertical_sbr_sse2_16(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch,
    int width, int height) noexcept;

void vertical_blur3_avx2_8(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_blur5_avx2_8(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_sbr_avx2_8(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch,
    int width, int height) noexcept;

void vertical_blur3_avx2_16(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_blur5_avx2_16(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template <int h, uint32_t u>
void vertical_sbr_avx2_16(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch,
    int width, int height) noexcept;

void vertical_blur3_avx512_8(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_blur5_avx512_8(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_sbr_avx512_8(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch,
    int width, int height) noexcept;

void vertical_blur3_avx512_16(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
void vertical_blur5_avx512_16(void* __restrict dstp, const void* srcp, int dst_pitch, int src_pitch, int width, int height) noexcept;
template <int h, uint32_t u>
void vertical_sbr_avx512_16(void* __restrict dstp, void* __restrict tempp, const void* srcp, int dst_pitch, int temp_pitch, int src_pitch,
    int width, int height) noexcept;
#endif
