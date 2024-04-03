/* 
**
** This file is part of Bevara Access Filters.
** 
** This file is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation.
** 
** This file is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License along with this file. If not, see <https://www.gnu.org/licenses/>.
*/

#include <gpac/filters.h>
#include <gpac/avparse.h>

typedef struct
{
	u32 codecid;
	GF_FilterPid *ipid, *opid;
	u32 width, height, pixel_format, in_BPP, out_BPP;
	u32 ofmt;
} GF_PNGDecCtx;

GF_Err convert(char* out, u32 out_format, const char* in, u32 in_format, u32 count);

static GF_Err pngdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	GF_PNGDecCtx *ctx = (GF_PNGDecCtx *)gf_filter_get_udta(filter);

	// disconnect of src pid (not yet supported)
	if (is_remove)
	{
		if (ctx->opid)
		{
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!prop)
		return GF_NOT_SUPPORTED;
	ctx->codecid = prop->value.uint;
	ctx->ipid = pid;

	if (!ctx->opid)
	{
		ctx->opid = gf_filter_pid_new(filter);
	}
	// copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));

	if (!ctx->ofmt) {
        ctx->ofmt = GF_PIXEL_RGB;
    }

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->ofmt));

	if (ctx->codecid == GF_CODECID_PNG)
	{
		gf_filter_set_name(filter, "pngdec:libpng");
	}
	return GF_OK;
}

static GF_Err pngdec_reconfigure_output(GF_Filter *filter, GF_FilterPid *pid)
{
	const GF_PropertyValue *p;
	GF_PNGDecCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->opid != pid) return GF_BAD_PARAM;

	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_PIXFMT);
	if (p) ctx->ofmt = p->value.uint;
	return pngdec_configure_pid(filter, ctx->ipid, GF_FALSE);
}


static GF_Err pngdec_process(GF_Filter *filter)
{
	GF_Err e;
	GF_FilterPacket *pck;
	u8 *data, *output;
	u32 size;
	GF_PNGDecCtx *ctx = (GF_PNGDecCtx *)gf_filter_get_udta(filter);
	Bool need_conversion = GF_FALSE;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck)
	{
		if (gf_filter_pid_is_eos(ctx->ipid))
		{
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	data = (u8 *)gf_filter_pck_get_data(pck, &size);

	GF_FilterPacket *dst_pck;
	u32 out_size = 0;
	u32 in_size = 0;
	u32 w = ctx->width;
	u32 h = ctx->height;
	u32 pf = ctx->ofmt;

	e = gf_img_png_dec(data, size, &ctx->width, &ctx->height, &ctx->pixel_format, NULL, &out_size);

	if (e != GF_BUFFER_TOO_SMALL)
	{
		gf_filter_pid_drop_packet(ctx->ipid);
		return e;
	}
	if ((w != ctx->width) || (h != ctx->height)){
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height));
	} 
	
	if (pf != ctx->pixel_format)
	{
		need_conversion = GF_TRUE;

		switch (ctx->pixel_format)
		{
		case GF_PIXEL_GREYSCALE:
			ctx->in_BPP = 1;
			break;
		case GF_PIXEL_RGB:
			ctx->in_BPP = 3;
			break;
		case GF_PIXEL_RGBA:
			ctx->in_BPP = 4;
			break;
		}
		
		switch (pf)
		{
		case GF_PIXEL_GREYSCALE:
			ctx->out_BPP = 1;
			break;
		case GF_PIXEL_RGB:
			ctx->out_BPP = 3;
			break;
		case GF_PIXEL_RGBA:
			ctx->out_BPP = 4;
			break;
		}
		
		in_size = ctx->in_BPP * ctx->width * ctx->height;
		out_size = ctx->out_BPP * ctx->width * ctx->height;

	
		//gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->pixel_format));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->out_BPP * ctx->width));
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, out_size, &output);
	if (!dst_pck)
		return GF_OUT_OF_MEM;

	if (need_conversion)
	{
		u32 *src = (u32 *)gf_malloc(in_size);
		e = gf_img_png_dec(data, size, &ctx->width, &ctx->height, &ctx->pixel_format, src, &in_size);
		e = convert(output, ctx->ofmt, src, ctx->pixel_format, ctx->width * ctx->height);
		gf_free(src);
	}
	else
	{
		e = gf_img_png_dec(data, size, &ctx->width, &ctx->height, &ctx->pixel_format, output, &out_size);
	}
	

	if (e)
	{
		gf_filter_pck_discard(dst_pck);
	}
	else
	{
		gf_filter_pck_merge_properties(pck, dst_pck);
		gf_filter_pck_set_dependency_flags(dst_pck, 0);
		gf_filter_pck_send(dst_pck);
	}
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static const GF_FilterCapability ImgDecCaps[] =
	{
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
		CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_PNG),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister ImgPngRegister = {
	.name = "pngdec",
	GF_FS_SET_DESCRIPTION("PNG decoder")
		GF_FS_SET_HELP("This filter decodes PNG images.")
			.private_size = sizeof(GF_PNGDecCtx),
	.priority = 1,
	SETCAPS(ImgDecCaps),
	.configure_pid = pngdec_configure_pid,
	.reconfigure_output = pngdec_reconfigure_output,
	.process = pngdec_process,
};

const GF_FilterRegister * EMSCRIPTEN_KEEPALIVE dynCall_pngdec_register(GF_FilterSession *session)
{
	return &ImgPngRegister;
}
