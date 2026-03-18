#include <memory>
#include <string>
#include <vector>

#include "vinverse_vs.h"

template <typename T, VinverseMode mode, bool eclip, bool thresh>
const VSFrame* VS_CC vinverse_get_frame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx,
    VSCore* core, const VSAPI* vsapi)
{
    auto d = static_cast<Vinverse<T, mode, eclip, thresh>*>(instanceData);

    if (activationReason == arInitial)
    {
        vsapi->requestFrameFilter(n, d->child_, frameCtx);
        if constexpr (eclip)
            vsapi->requestFrameFilter(n, d->clip2_, frameCtx);
    }
    else if (activationReason == arAllFramesReady)
    {
        const VSFrame* src = vsapi->getFrameFilter(n, d->child_, frameCtx);
        const VSFrame* clip2 = eclip ? vsapi->getFrameFilter(n, d->clip2_, frameCtx) : nullptr;

        const VSVideoFormat* fmt = vsapi->getVideoFrameFormat(src);
        const VSFrame* planeSrc[3] = { nullptr, nullptr, nullptr };
        int planes[3] = { 0, 1, 2 };

        if (d->uv_ < 3 && fmt->numPlanes > 1)
        {
            planeSrc[1] = src;
            planeSrc[2] = src;
        }

        VSFrame* dst = vsapi->newVideoFrame2(fmt, vsapi->getFrameWidth(src, 0), vsapi->getFrameHeight(src, 0), planeSrc, planes, src, core);

        for (int plane = 0; plane < fmt->numPlanes; ++plane)
        {
            if (planeSrc[plane] != nullptr)
                continue;

            const int width = vsapi->getFrameWidth(src, plane);
            const int height = vsapi->getFrameHeight(src, plane);
            const int src_pitch = static_cast<int>(vsapi->getStride(src, plane) / sizeof(T));
            const int dst_pitch = static_cast<int>(vsapi->getStride(dst, plane) / sizeof(T));

            size_t pbuf_size = height * d->pb_pitch;
            auto deleter = [](T* ptr) { vsh::vsh_aligned_free(ptr); };
            std::unique_ptr<T[], decltype(deleter)> buffer(
                static_cast<T*>(vsh::vsh_aligned_malloc(pbuf_size * 2 * sizeof(T), 64)), deleter);

            if (!buffer) {
                vsapi->setFilterError("Vinverse: memory allocation failed", frameCtx);
                vsapi->freeFrame(src);
                if constexpr (eclip)
                    vsapi->freeFrame(clip2);

                vsapi->freeFrame(dst);
                return nullptr;
            }

            T* blur3_buf = buffer.get();
            T* blur6_buf = blur3_buf + pbuf_size;

            const uint8_t* srcp = vsapi->getReadPtr(src, plane);
            uint8_t* dstp = vsapi->getWritePtr(dst, plane);

            if constexpr (mode == VinverseMode::Vinverse)
            {
                d->do_blur3(blur3_buf, srcp, d->pb_pitch, src_pitch, width, height);
                if constexpr (!eclip)
                    d->do_blur5(blur6_buf, blur3_buf, d->pb_pitch, d->pb_pitch, width, height);
            }
            else {
                if (plane == 0)
                    d->do_sbr(blur3_buf, blur6_buf, srcp, d->pb_pitch, d->pb_pitch, src_pitch, width, height);
                else
                    vsh::bitblt(blur3_buf, d->pb_pitch * sizeof(T), srcp, src_pitch * sizeof(T), width * sizeof(T), height);

                d->do_blur3(blur6_buf, blur3_buf, d->pb_pitch, d->pb_pitch, width, height);
            }

            if constexpr (eclip)
            {
                const uint8_t* cp2p = vsapi->getReadPtr(clip2, plane);
                int cp2_pitch = static_cast<int>(vsapi->getStride(clip2, plane) / sizeof(T));
                (d->do_fin_plane)(dstp, srcp, blur3_buf, cp2p, src_pitch, dst_pitch, d->pb_pitch, cp2_pitch, width, height);
            }
            else
                (d->do_fin_plane)(dstp, srcp, blur3_buf, blur6_buf, src_pitch, dst_pitch, d->pb_pitch, 0, width, height);
        }

        vsapi->freeFrame(src);
        if constexpr (eclip)
            vsapi->freeFrame(clip2);

        return dst;
    }
    return nullptr;
}

template <typename T, VinverseMode mode, bool eclip, bool thresh>
void VS_CC vinverse_free(void* instanceData, VSCore* core, const VSAPI* vsapi)
{
    auto d = static_cast<Vinverse<T, mode, eclip, thresh>*>(instanceData);
    vsapi->freeNode(d->child_);
    if constexpr (eclip)
        vsapi->freeNode(d->clip2_);

    delete d;
}

template <typename T, VinverseMode mode, bool eclip, bool thresh>
void create_vinverse(VSMap* out, VSNode* clip, float sstr, int amnt, int uv, float scl, int opt, VSNode* clip2, int thr,
    VSCore* core, const VSAPI* vsapi)
{
    auto d = new Vinverse<T, mode, eclip, thresh>(clip, sstr, amnt, uv, scl, opt, clip2, thr, vsapi);

    VSFilterDependency deps[2] = { {clip, rpStrictSpatial} };
    int num_deps = 1;
    if constexpr (eclip)
    {
        deps[1] = { clip2, rpStrictSpatial };
        num_deps = 2;
    }

    vsapi->createVideoFilter(out, (mode == VinverseMode::Vinverse2) ? "Vinverse2" : "Vinverse", d->vi,
        vinverse_get_frame<T, mode, eclip, thresh>, vinverse_free<T, mode, eclip, thresh>, fmParallel, deps, num_deps, d, core);
}

static void VS_CC vinverse_create(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
    int err;
    VSNode* clip = vsapi->mapGetNode(in, "clip", 0, nullptr);
    VSNode* clip2 = vsapi->mapGetNode(in, "clip2", 0, &err);
    if (err)
        clip2 = nullptr;

    float sstr = vsh::doubleToFloatS(vsapi->mapGetFloat(in, "sstr", 0, &err));
    if (err)
        sstr = 2.7f;

    int amnt = vsapi->mapGetIntSaturated(in, "amnt", 0, &err);
    if (err)
        amnt = -1;

    int uv = vsapi->mapGetIntSaturated(in, "uv", 0, &err);
    if (err)
        uv = 3;

    float scl = vsh::doubleToFloatS(vsapi->mapGetFloat(in, "scl", 0, &err));
    if (err)
        scl = 0.25f;

    int opt = vsapi->mapGetIntSaturated(in, "opt", 0, &err);
    if (err)
        opt = -1;

    int thr = vsapi->mapGetIntSaturated(in, "thr", 0, &err);
    if (err)
        thr = 0;

    VinverseMode mode = (userData != nullptr) ? VinverseMode::Vinverse2 : VinverseMode::Vinverse;

    const VSVideoInfo* vi = vsapi->getVideoInfo(clip);

    if (!vsh::isConstantVideoFormat(vi) || vi->format.colorFamily == cfRGB || vi->format.sampleType != stInteger)
    {
        vsapi->mapSetError(out, "Vinverse: only constant format integer YUV input supported.");
        vsapi->freeNode(clip);
        if (clip2)
            vsapi->freeNode(clip2);

        return;
    }

    if (vi->format.bitsPerSample > 16)
    {
        vsapi->mapSetError(out, "Vinverse: only 8..16-bit input is supported!");
        vsapi->freeNode(clip);
        if (clip2)
            vsapi->freeNode(clip2);

        return;
    }

    const int peak = (1 << vi->format.bitsPerSample) - 1;
    amnt = (amnt == -1) ? peak : amnt;

    if (amnt < 1 || amnt > peak)
    {
        vsapi->mapSetError(out, ("Vinverse: amnt must be greater than 0 and less than or equal to " + std::to_string(peak) + "!").c_str());
        vsapi->freeNode(clip);
        if (clip2)
            vsapi->freeNode(clip2);

        return;
    }

    if (uv < 1 || uv > 3)
    {
        vsapi->mapSetError(out, "Vinverse: uv must be set to 1, 2, or 3!");
        vsapi->freeNode(clip);
        if (clip2)
            vsapi->freeNode(clip2);

        return;
    }

    if (opt < -1 || opt > 3)
    {
        vsapi->mapSetError(out, "Vinverse: opt must be between -1..3.");
        vsapi->freeNode(clip);
        if (clip2)
            vsapi->freeNode(clip2);

        return;
    }


    if (clip2)
    {
        const VSVideoInfo* vi2 = vsapi->getVideoInfo(clip2);
        if (!vsh::isSameVideoFormat(&vi->format, &vi2->format))
        {
            vsapi->mapSetError(out, "Vinverse: clip2's format doesn't match.");
            vsapi->freeNode(clip); vsapi->freeNode(clip2);
            return;
        }
        if (vi->width != vi2->width || vi->height != vi2->height)
        {
            vsapi->mapSetError(out, "Vinverse: input and clip2 must be the same resolution.");
            vsapi->freeNode(clip); vsapi->freeNode(clip2);
            return;
        }
        if (vi->numFrames != vi2->numFrames)
        {
            vsapi->mapSetError(out, "Vinverse: clip2's number of frames doesn't match.");
            vsapi->freeNode(clip); vsapi->freeNode(clip2);
            return;
        }
    }

    if (thr < 0 || thr > peak)
    {
        vsapi->mapSetError(out, ("Vinverse: thr must be between 0.." + std::to_string(peak) + "!").c_str());
        vsapi->freeNode(clip);
        if (clip2)
            vsapi->freeNode(clip2);
        return;
    }

    CPUFlags cpu = get_cpu_flags();
    if (!cpu.avx512f && opt == 3)
    {
        vsapi->mapSetError(out, "Vinverse: opt=3 requires AVX512F."); vsapi->freeNode(clip);
        if (clip2)
            vsapi->freeNode(clip2);
        return;
    }
    if (!cpu.avx2 && opt == 2)
    {
        vsapi->mapSetError(out, "Vinverse: opt=2 requires AVX2."); vsapi->freeNode(clip);
        if (clip2)
            vsapi->freeNode(clip2);
        return;
    }
    if (!cpu.sse2 && opt == 1)
    {
        vsapi->mapSetError(out, "Vinverse: opt=1 requires SSE2."); vsapi->freeNode(clip);
        if (clip2)
            vsapi->freeNode(clip2);
        return;
    }

    bool is_16bit = vi->format.bytesPerSample == 2;
    bool eclip = (clip2 != nullptr);
    bool use_thresh = (thr > 0);

    if (mode == VinverseMode::Vinverse)
    {
        if (eclip)
        {
            if (use_thresh)
            {
                if (is_16bit)
                    create_vinverse<uint16_t, VinverseMode::Vinverse, true, true>(out, clip, sstr, amnt, uv, scl, opt, clip2, thr, core,
                        vsapi);
                else
                    create_vinverse<uint8_t, VinverseMode::Vinverse, true, true>(out, clip, sstr, amnt, uv, scl, opt, clip2, thr, core,
                        vsapi);
            }
            else
            {
                if (is_16bit)
                    create_vinverse<uint16_t, VinverseMode::Vinverse, true, false>(out, clip, sstr, amnt, uv, scl, opt, clip2, thr, core,
                        vsapi);
                else
                    create_vinverse<uint8_t, VinverseMode::Vinverse, true, false>(out, clip, sstr, amnt, uv, scl, opt, clip2, thr, core,
                        vsapi);
            }
        }
        else {
            if (use_thresh)
            {
                if (is_16bit)
                    create_vinverse<uint16_t, VinverseMode::Vinverse, false, true>(out, clip, sstr, amnt, uv, scl, opt, clip2, thr, core,
                        vsapi);
                else
                    create_vinverse<uint8_t, VinverseMode::Vinverse, false, true>(out, clip, sstr, amnt, uv, scl, opt, clip2, thr, core,
                        vsapi);
            }
            else
            {
                if (is_16bit)
                    create_vinverse<uint16_t, VinverseMode::Vinverse, false, false>(out, clip, sstr, amnt, uv, scl, opt, clip2, thr, core,
                        vsapi);
                else
                    create_vinverse<uint8_t, VinverseMode::Vinverse, false, false>(out, clip, sstr, amnt, uv, scl, opt, clip2, thr, core,
                    vsapi);
            }
        }
    }
    else
    {
        if (is_16bit)
            create_vinverse<uint16_t, VinverseMode::Vinverse2, false, false>(out, clip, sstr, amnt, uv, scl, opt, clip2, thr, core, vsapi);
        else
            create_vinverse<uint8_t, VinverseMode::Vinverse2, false, false>(out, clip, sstr, amnt, uv, scl, opt, clip2, thr, core, vsapi);
    }
}

VS_EXTERNAL_API(void) VapourSynthPluginInit2(VSPlugin* plugin, const VSPLUGINAPI* vspapi) {
    vspapi->configPlugin("com.example.vinverse", "vinverse", "Vinverse Plugin", VS_MAKE_VERSION(1, 0), VAPOURSYNTH_API_VERSION, 0, plugin);
    vspapi->registerFunction("vinverse",
        "clip:vnode;sstr:float:opt;amnt:int:opt;uv:int:opt;scl:float:opt;opt:int:opt;clip2:vnode:opt;thr:int:opt;",
        "clip:vnode;", vinverse_create, nullptr, plugin);
    vspapi->registerFunction("vinverse2",
        "clip:vnode;sstr:float:opt;amnt:int:opt;uv:int:opt;scl:float:opt;opt:int:opt;",
        "clip:vnode;", vinverse_create, (void*)1, plugin);
}
