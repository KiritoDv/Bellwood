#include "PreRender.h"

#include "libultra/libultra.h"
#include "gfxalloc.h"
#include "sys_ucode.h"
#include "libforest/gbi_extensions.h"

static Gfx* gfx_SetUpCFB(Gfx* gfx, void* ptr, u32 width, u32 heiht) {

    gDPPipeSync(gfx + 0);
    gDPSetColorImage(gfx + 1, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, ptr);
    gDPSetScissor(gfx + 2, G_SC_NON_INTERLACE, 0, 0, width, heiht);

    return gfx + 3;
}

extern void PreRender_setup_savebuf(PreRender* render, u32 width, u32 height, void* fbuf, void* zbuf, void* cvg) {
    render->width_bak = width;
    render->height_bak = height;

    render->framebuffer_bak = fbuf;
    render->cvg_bak = cvg;

    render->zbuffer_bak = zbuf;

    render->ulx_bak = 0;
    render->uly_bak = 0;
    render->lrx_bak = width - 1;
    render->lry_bak = height - 1;
}

extern void PreRender_init(PreRender* render) {
    bzero(render, sizeof(PreRender));
}

extern void PreRender_setup_renderbuf(PreRender* render, u32 width, u32 height, void* fbuf, void* zbuf) {
    render->width = width;
    render->height = height;

    render->framebuffer = fbuf;
    render->zbuffer = zbuf;

    render->ulx = 0;
    render->uly = 0;
    render->lrx = width - 1;
    render->lry = height - 1;
}

extern void PreRender_cleanup(PreRender* render) {
    // nothing
}

