#include "toolset.h"

//-V:bytepp:112
//-V:bitpp:112

namespace ts
{

    // YUV to RGB conversion constants.
    // Y contribution to R,G,B.  Scale and bias.
    // TODO(fbarchard): Consider moving constants into a common header.
#define YG 18997 /* round(1.164 * 64 * 256 * 256 / 257) */
#define YGB 1160 /* 1.164 * 64 * 16 - adjusted for even error distribution */

    // U and V contributions to R,G,B.
#define UB -128 /* -min(128, round(2.018 * 64)) */
#define UG 25 /* -round(-0.391 * 64) */
#define VG 52 /* -round(-0.813 * 64) */
#define VR -102 /* -round(1.596 * 64) */

    // Bias values to subtract 16 from Y and 128 from U and V.
#define BB (UB * 128            - YGB)
#define BG (UG * 128 + VG * 128 - YGB)
#define BR            (VR * 128 - YGB)

    // C reference code that mimics the YUV assembly.
    static __inline void YuvPixel(uint8 y, uint8 u, uint8 v,
                                  uint8* b, uint8* g, uint8* r) {
        uint32 y1 = (uint32)(y * 0x0101 * YG) >> 16;
        *b = CLAMP<uint8>((int32)(BB - (u * UB) + y1) >> 6);
        *g = CLAMP<uint8>((int32)(BG - (v * VG + u * UG) + y1) >> 6);
        *r = CLAMP<uint8>((int32)(BR - (v * VR) + y1) >> 6);
    }

#define RGB_TO_YUV(t)                                                                       \
    ( (0.257*(float)(t>>16)) + (0.504*(float)(t>>8&0xff)) + (0.098*(float)(t&0xff)) + 16),  \
    (-(0.148*(float)(t>>16)) - (0.291*(float)(t>>8&0xff)) + (0.439*(float)(t&0xff)) + 128), \
    ( (0.439*(float)(t>>16)) - (0.368*(float)(t>>8&0xff)) - (0.071*(float)(t&0xff)) + 128)

static INLINE uint8 RGB_Y( TSCOLOR c )
{
    //return (uint8)( lround( (0.257*(float)RED(c)) + (0.504*(float)GREEN(c)) + (0.098*(float)BLUE(c)) + 16) );
    return (uint8)((RED(c) * 16843 + GREEN(c) * 33030 + BLUE(c) * 6423 + 1048576) >> 16);
}

static INLINE uint8 RGB_U(TSCOLOR c)
{
    //return (uint8)( lround(-(0.148*(float)RED(c)) - (0.291*(float)GREEN(c)) + (0.439*(float)BLUE(c)) + 128) );
    return (uint8)((8388608 - RED(c) * 9699 - GREEN(c) * 19071 + BLUE(c) * 28770) >> 16);
}

static INLINE uint8 RGB_V(TSCOLOR c)
{
    //return (uint8)( 0xff & lround((0.439*(float)RED(c)) - (0.368*(float)GREEN(c)) - (0.071*(float)BLUE(c)) + 128) );
    return (uint8)((RED(c) * 28770 - GREEN(c) * 24117 - BLUE(c) * 4653 + 8388608) >> 16);
}

TSCOLOR INLINE coloravg( TSCOLOR c1, TSCOLOR c2, TSCOLOR c3, TSCOLOR c4 )
{
    //return ARGB<uint>((RED(c1) + RED(c2) + RED(c3) + RED(c4)) / 4, (GREEN(c1) + GREEN(c2) + GREEN(c3) + GREEN(c4)) / 4, (BLUE(c1) + BLUE(c2) + BLUE(c3) + BLUE(c4)) / 4);

    _asm
    {
        mov eax, c1
        mov ecx, c2
        mov ebx, eax
        mov edx, ecx
        and eax, 0x00FF00FF
        and ebx, 0x0000FF00

        and ecx, 0x00FF00FF
        and edx, 0x0000FF00
        add eax, ecx
        add ebx, edx

        mov ecx, c3
        mov edx, ecx
        and ecx, 0x00FF00FF
        and edx, 0x0000FF00
        add eax, ecx
        add ebx, edx

        mov ecx, c4
        mov edx, ecx
        and ecx, 0x00FF00FF
        and edx, 0x0000FF00
        add eax, ecx
        add ebx, edx

        shr eax, 2
        shr ebx, 2
        and eax, 0x00FF00FF
        and ebx, 0x0000FF00
        or  eax, ebx

    }
}

void TSCALL img_helper_rgb2yuv(uint8 *dst, const imgdesc_s &src_info, const uint8 *sou, yuv_fmt_e yuvfmt)
{
    switch (yuvfmt)
    {
    case ts::YFORMAT_I420:
        {
            uint8 *dst_u = dst + src_info.sz.x * src_info.sz.y;
            uint8 *dst_v = dst_u + src_info.sz.x * src_info.sz.y / 4;

            for( int y = 0; y < src_info.sz.y; y+=2, dst += src_info.sz.x, sou += src_info.pitch * 2 )
            {
                const TSCOLOR *clr0 = (const TSCOLOR *)sou;
                const TSCOLOR *clr1 = (const TSCOLOR *)(sou + src_info.pitch);
                for (int x = 0; x < src_info.sz.x; x+=2, dst += 2, clr0 += 2, clr1 += 2, ++dst_u, ++dst_v )
                {
                    TSCOLOR c1 = clr0[0];
                    TSCOLOR c2 = clr0[1];
                    TSCOLOR c3 = clr1[0];
                    TSCOLOR c4 = clr1[1];

                    dst[0] = RGB_Y( c1 );
                    dst[1] = RGB_Y( c2 );
                    dst[0 + src_info.sz.x] = RGB_Y(c3);
                    dst[1 + src_info.sz.x] = RGB_Y(c4);

                    TSCOLOR cc = coloravg( c1, c2, c3, c4 );

                    *dst_u = RGB_U( cc );
                    *dst_v = RGB_V( cc );
                }
            }
            
        }
        break;
    }
}

void TSCALL img_helper_yuv2rgb(uint8 *des, const imgdesc_s &des_info, const uint8 *sou, yuv_fmt_e yuvfmt)
{
    switch (yuvfmt)
    {
    case ts::YFORMAT_YUY2:
        {
            for(int y = 0; y<des_info.sz.y; ++y, des += des_info.pitch)
            {
                TSCOLOR *dst = (TSCOLOR *)des;
                TSCOLOR *dste = dst + des_info.sz.x;
                for(;dst < dste;sou += 4, dst += 2)
                {
                    uint32 YUYV = *(uint32 *)sou;

                    int32 Y0 = (uint32)(( YUYV & 0xff ) * 0x0101 * YG) >> 16;
                    int32 Y1 = (uint32)(( (YUYV>>16) & 0xff ) * 0x0101 * YG) >> 16;

                    uint8 U = (YUYV >> 8) & 0xff;
                    uint8 V = YUYV >> 24;

                    int prer = BR - (V * VR);
                    int preg = BG - (V * VG + U * UG);
                    int preb = BB - (U * UB);

                    dst[0] = ts::ARGB<int>((prer + Y0) >> 6, (preg + Y0) >> 6, (preb + Y0) >> 6);
                    dst[1] = ts::ARGB<int>((prer + Y1) >> 6, (preg + Y1) >> 6, (preb + Y1) >> 6);

                }
            }

        }
        break;
    case ts::YFORMAT_I420:
        {
            const uint8 *Y_plane = sou;
            int sz = des_info.sz.x * des_info.sz.y;
            const uint8 *U_plane = sou + sz;
            const uint8 *V_plane = U_plane + sz / 4;

            for (int y = 0; y < des_info.sz.y; y+=2, des += des_info.pitch * 2, Y_plane += des_info.sz.x)
            {
                TSCOLOR *dst0 = (TSCOLOR *)des;
                TSCOLOR *dst1 = (TSCOLOR *)(des + des_info.pitch);
                TSCOLOR *dste = dst0 + des_info.sz.x;
                for (; dst0 < dste; ++U_plane, ++V_plane, dst0 += 2, dst1 += 2, Y_plane += 2)
                {

                    int Y0 = (int)(Y_plane[0] * 0x0101 * YG) >> 16;
                    int Y1 = (int)(Y_plane[1] * 0x0101 * YG) >> 16;
                    int Y2 = (int)(Y_plane[0 + des_info.sz.x] * 0x0101 * YG) >> 16;
                    int Y3 = (int)(Y_plane[1 + des_info.sz.x] * 0x0101 * YG) >> 16;

                    uint8 U = *U_plane;
                    uint8 V = *V_plane;

                    int prer = BR - (V * VR);
                    int preg = BG - (V * VG + U * UG);
                    int preb = BB - (U * UB);

                    dst0[0] = ts::ARGB<int>((prer+Y0) >> 6, (preg+Y0) >> 6, (preb+Y0) >> 6);
                    dst0[1] = ts::ARGB<int>((prer+Y1) >> 6, (preg+Y1) >> 6, (preb+Y1) >> 6);

                    dst1[0] = ts::ARGB<int>((prer+Y2) >> 6, (preg+Y2) >> 6, (preb+Y2) >> 6);
                    dst1[1] = ts::ARGB<int>((prer+Y3) >> 6, (preg+Y3) >> 6, (preb+Y3) >> 6);

               }
            }
        }
        break;
    case ts::YFORMAT_I420x2:
        {
            ts::ivec2 yuv_size = des_info.sz * 2;
            const uint8 *Y_plane = sou;
            int sz = yuv_size.x * yuv_size.y;
            const uint8 *U_plane = sou + sz;
            const uint8 *V_plane = U_plane + sz / 4;

            for (int y = 0; y < des_info.sz.y; ++y, des += des_info.pitch, Y_plane += yuv_size.x)
            {
                TSCOLOR *dst = (TSCOLOR *)des;
                TSCOLOR *dste = dst + des_info.sz.x;
                for (; dst < dste; ++U_plane, ++V_plane, ++dst, Y_plane += 2)
                {

                    int Y = ((int)Y_plane[0] + Y_plane[1] + Y_plane[0 + yuv_size.x] + Y_plane[1 + yuv_size.x]) / 4;
                    int32 Y0 = (uint32)(Y * 0x0101 * YG) >> 16;

                    uint8 U = *U_plane;
                    uint8 V = *V_plane;

                    int prer = BR - (V * VR);
                    int preg = BG - (V * VG + U * UG);
                    int preb = BB - (U * UB);

                    *dst = ts::ARGB<int>((prer + Y0) >> 6, (preg + Y0) >> 6, (preb + Y0) >> 6);
                }
            }
        }
        break;
    }
}

void TSCALL img_helper_premultiply(uint8 *des, const imgdesc_s &des_info)
{
    int desnl = des_info.pitch - des_info.sz.x * 4;

    for (int y = 0; y < des_info.sz.y; ++y, des += desnl)
    {
        for (int x = 0; x < des_info.sz.x; ++x, des += 4)
        {
            TSCOLOR ocolor;
            TSCOLOR color = *(TSCOLOR *)des;
            uint8 alpha = ALPHA(color);
            if (alpha == 255)
            {
                continue;
            }
            else if (alpha == 0)
            {
                ocolor = 0;
            }
            else
            {
                ocolor = PREMULTIPLY(color);
            }

            *(TSCOLOR *)des = ocolor;
        }
    }

}

void TSCALL img_helper_fill(uint8 *des, const imgdesc_s &des_info, TSCOLOR color)
{
    int desnl = des_info.pitch - des_info.sz.x*des_info.bytepp();
    int desnp = des_info.bytepp();

    switch (des_info.bytepp())
    {
        case 1:
            for (int y = 0; y < des_info.sz.y; y++, des += desnl)
                for (int x = 0; x < des_info.sz.x; x++, des += desnp)
                    *(uint8 *)des = (uint8)color;
            break;
        case 2:
            for (int y = 0; y < des_info.sz.y; y++, des += desnl)
                for (int x = 0; x < des_info.sz.x; x++, des += desnp)
                    *(uint16 *)des = (uint16)color;
            break;
        case 3:
            for (int y = 0; y < des_info.sz.y; y++, des += desnl) {
                for (int x = 0; x < des_info.sz.x; x++, des += desnp) {
                    *(uint16 *)des = (uint16)color;
                    *(uint8 *)(des + 2) = (uint8)(color >> 16);
                }
            }
            break;
        case 4:
            for (int y = 0; y < des_info.sz.y; y++, des += desnl)
                for (int x = 0; x < des_info.sz.x; x++, des += desnp)
                    *(uint32 *)des = color;
            break;
    }
}

void TSCALL img_helper_copy(uint8 *des, const uint8 *sou, const imgdesc_s &des_info, const imgdesc_s &sou_info)
{
    ASSERT(sou_info.sz == des_info.sz);

    if (des_info.bytepp() == sou_info.bytepp())
    {
        aint len = sou_info.sz.x * des_info.bytepp();
        if (len == des_info.pitch && des_info.pitch == sou_info.pitch)
        {
            memcpy(des, sou, len * sou_info.sz.y);
        }
        else
        {
            for (int y = 0; y < sou_info.sz.y; ++y, des += des_info.pitch, sou += sou_info.pitch)
                memcpy(des, sou, len);
        }
    }
    else
    {
        uint32 alpha = (sou_info.bytepp() == 3) ? 0xFF000000 : 0;

        for (int y = 0; y < sou_info.sz.y; ++y, des += des_info.pitch, sou += sou_info.pitch)
        {
            uint8 *des1 = des;
            const uint8 *sou1 = sou;
            for (int i = 0; i < (sou_info.sz.x - 1); ++i, sou1 += sou_info.bytepp(), des1 += des_info.bytepp())
            {
                uint32 color = *((uint32 *)sou1) | alpha;
                *((uint32 *)des1) = color;
            }
            int c = tmin(sou_info.bytepp(), des_info.bytepp());
            while (c--)
                *des1++ = *sou1++;

            if (des_info.bytepp() == 4 && c == 3)
                *des1 = 0xFF;
        }
    }


}

extern "C" void _cdecl asm_shrink2x(
		void *dst,
		const void *src,
		unsigned long width,
		unsigned long height,
		unsigned long srcpitch,
		unsigned long dstcorrectpitch);


void TSCALL img_helper_shrink_2x(uint8 *des, const uint8 *sou, const imgdesc_s &des_info, const imgdesc_s &sou_info)
{
    ASSERT(des_info.sz == (sou_info.sz / 2));
    ivec2 newsz = sou_info.sz / 2;

    aint desnl = des_info.pitch - des_info.sz.x*des_info.bytepp();
    aint sounl = sou_info.pitch - sou_info.sz.x*sou_info.bytepp();

    if (sou_info.bytepp() == 1)
    {
        for (int y = 0; y < sou_info.sz.y; y += 2)
        {
            for (int x = 0; x < sou_info.sz.x; x += 2, ++des)
            {
                aint b0 = *(sou + x + 0);

                b0 += *(sou + x + 1);

                b0 += *(sou + x + 0 + sou_info.pitch);

                b0 += *(sou + x + 1 + sou_info.pitch);

                *(des + 0) = (uint8)(b0 / 4);
            }

            sou += sou_info.pitch * 2;
        }
    }
    else if (sou_info.bytepp() == 2)
    {
        for (aint y = 0; y < newsz.y; y++, sou += sou_info.pitch + sounl, des += desnl)
        {
            for (aint x = 0; x < newsz.x; x++, des += 3, sou += 3 + 3)
            {

                aint b0 = *(sou + 0);
                aint b1 = *(sou + 1);
                aint b2 = *(sou + 2);

                b0 += *(sou + 3);
                b1 += *(sou + 4);
                b2 += *(sou + 5);

                b0 += *(sou + 0 + sou_info.pitch);
                b1 += *(sou + 1 + sou_info.pitch);
                b2 += *(sou + 2 + sou_info.pitch);

                b0 += *(sou + 3 + sou_info.pitch);
                b1 += *(sou + 4 + sou_info.pitch);
                b2 += *(sou + 5 + sou_info.pitch);

                *(des + 0) = (uint8)(b0 >> 2);
                *(des + 1) = (uint8)(b1 >> 2);
                *(des + 2) = (uint8)(b2 >> 2);
            }
        }
    }
    else if (sou_info.bytepp() == 4)
        asm_shrink2x(des, sou, des_info.sz.x, des_info.sz.y, sou_info.pitch, desnl);
}

void TSCALL img_helper_copy_components(uint8* des, const uint8* sou, const imgdesc_s &des_info, const imgdesc_s &sou_info, int num_comps)
{
    aint dpitch = des_info.pitch;
    aint spitch = sou_info.pitch;
    aint dszy = des_info.sz.y;
    for (dpitch -= des_info.sz.x*des_info.bytepp(), spitch -= des_info.sz.x*sou_info.bytepp(); dszy > 0; --dszy, des += dpitch, sou += spitch)
        for (int x = des_info.sz.x; x > 0; --x, des += des_info.bytepp(), sou += sou_info.bytepp())
            for (int i = 0; i < num_comps; ++i)
                des[i] = sou[i];
}


void img_helper_merge_with_alpha(uint8 *dst, const uint8 *basesrc, const uint8 *src, const imgdesc_s &des_info, const imgdesc_s &base_info, const imgdesc_s &sou_info, int oalphao)
{
    ASSERT(sou_info.bytepp() == 4);

    if (des_info.bytepp() == 3)
    {
        for (int y = 0; y < des_info.sz.y; y++, dst += des_info.pitch, basesrc += base_info.pitch, src += sou_info.pitch)
        {
            uint8 *des = dst;
            const uint8 *desbase = basesrc;
            const uint32 *sou = (uint32 *)src;
            int bytepp = des_info.bytepp();
            int byteppbase = base_info.bytepp();
            for (int x = 0; x < des_info.sz.x; x++, des += bytepp, desbase += byteppbase, ++sou)
            {
                uint32 color = *sou;
                uint8 alpha = uint8(color >> 24);
                uint32 ocolor = 0;
                if (alpha == 0)
                {
                    if (dst == basesrc)
                        continue;
                } else if (alpha == 255)
                {
                    ocolor = color;
                }
                else
                {
                    ocolor = uint32(*(uint16 *)desbase) | (uint32(*((uint8 *)desbase + 2)) << 16);

                    uint8 R = as_byte(color >> 16);
                    uint8 G = as_byte(color >> 8);
                    uint8 B = as_byte(color);

                    uint8 oR = as_byte(ocolor >> 16);
                    uint8 oG = as_byte(ocolor >> 8);
                    uint8 oB = as_byte(ocolor);


                    float A = float(double(alpha) * (1.0 / 255.0));
                    float nA = 1.0f - A;

#define CCGOOD( C, oC ) CLAMP<uint8>( float(C) * A + float(oC) * nA )

                    ocolor = CCGOOD(B, oB) | (CCGOOD(G, oG) << 8) | (CCGOOD(R, oR) << 16);
#undef CCGOOD
                }

                *des = as_byte(ocolor);
                *(des + 1) = as_byte(ocolor >> 8);
                *(des + 2) = as_byte(ocolor >> 16);
            }
        }
    }
    else
    {

        if (oalphao < 0)
        {
            for (int y = 0; y < des_info.sz.y; y++, dst += des_info.pitch, basesrc += base_info.pitch, src += sou_info.pitch)
            {
                uint8 *des = dst;
                const uint8 *desbase = basesrc;
                const uint32 *sou = (uint32 *)src;
                int bytepp = des_info.bytepp();
                int byteppbase = base_info.bytepp();
                for (int x = 0; x < des_info.sz.x; x++, des += bytepp, desbase += byteppbase, ++sou)
                {
                    uint32 ocolor = 0;
                    uint32 color = *sou;
                    uint8 alpha = uint8(color >> 24);
                    if (alpha == 0)
                    {
                        if (dst == basesrc)
                            continue;
                    } else if (alpha == 255)
                    {
                        ocolor = color;
                    } else
                    {

                        uint8 R = as_byte(color >> 16);
                        uint8 G = as_byte(color >> 8);
                        uint8 B = as_byte(color);

                        uint32 ocolor_cur = byteppbase == 4 ? *(uint32 *)desbase : (uint32(*(uint16 *)desbase) | (uint32(*(desbase + 2)) << 16) | 0xFF000000);
                        uint8 oalpha = as_byte(ocolor_cur >> 24);
                        uint8 oR = as_byte(ocolor_cur >> 16);
                        uint8 oG = as_byte(ocolor_cur >> 8);
                        uint8 oB = as_byte(ocolor_cur);

                        float A = float(alpha) / 255.0f;
                        float nA = 1.0f - A;

#define CCGOOD( C, oC ) CLAMP<uint8>( float(C) * A + float(oC) * nA )
                        uint oiA = (uint)lround(oalpha + float(255 - oalpha) * A);
                        ocolor = CCGOOD(B, oB) | (CCGOOD(G, oG) << 8) | (CCGOOD(R, oR) << 16) | (CLAMP<uint8>(oiA) << 24);
#undef CCGOOD

                    }

                    *(uint32 *)des = ocolor;
                }
            }
        }
        else
        {
            for (int y = 0; y < des_info.sz.y; y++, dst += des_info.pitch, basesrc += base_info.pitch, src += sou_info.pitch)
            {
                uint8 *des = dst;
                const uint8 *desbase = basesrc;
                const uint32 *sou = (uint32 *)src;
                int bytepp = des_info.bytepp();
                int byteppbase = base_info.bytepp();
                for (int x = 0; x < des_info.sz.x; x++, des += bytepp, desbase += byteppbase, ++sou)
                {
                    uint32 ocolor = 0;
                    uint32 icolor = *sou;
                    uint8 ialpha = uint8(icolor >> 24);
                    if (ialpha == 0)
                    {
                        if (dst == basesrc)
                            continue;
                    } else if (ialpha == 255)
                    {
                        ocolor = icolor;
                    }
                    else
                    {

                        uint8 R = as_byte(icolor >> 16);
                        uint8 G = as_byte(icolor >> 8);
                        uint8 B = as_byte(icolor);

                        uint32 ocolor_cur = byteppbase == 4 ? *(uint32 *)desbase : (uint32(*(uint16 *)desbase) | (uint32(*(desbase + 2)) << 16));
                        uint8 oR = as_byte(ocolor_cur >> 16);
                        uint8 oG = as_byte(ocolor_cur >> 8);
                        uint8 oB = as_byte(ocolor_cur);

                        float A = float(ialpha) / 255.0f;
                        float nA = 1.0f - A;


                        int oiB = lround(float(B) * A + float(oB) * nA);
                        int oiG = lround(float(G) * A + float(oG) * nA);
                        int oiR = lround(float(R) * A + float(oR) * nA);

                        int oiA = lround(oalphao + float(255 - oalphao) * A);

                        ocolor = ((oiB > 255) ? 255 : oiB) | (((oiG > 255) ? 255 : oiG) << 8) | (((oiR > 255) ? 255 : oiR) << 16) | (((oiA > 255) ? 255 : oiA) << 24);

                    }

                    *(uint32 *)des = ocolor;
                }

            }
        }
    }

}


void    bmpcore_normal_s::before_modify(bitmap_c *me)
{
    if (m_core->m_ref == 1) return;

    bitmap_c b( *me );

    if (b.info().bitpp == 8) me->create_grayscale(b.info().sz);
    else if (b.info().bitpp == 15) me->create_15(b.info().sz);
    else if (b.info().bitpp == 16) me->create_16(b.info().sz);
    else if (b.info().bitpp == 24) me->create_RGB(b.info().sz);
    if (b.info().bitpp == 32) me->create_RGBA(b.info().sz);

    me->copy(ivec2(0), b.info().sz, b.extbody(), ivec2(0));
}

bool bmpcore_normal_s::operator==(const bmpcore_normal_s & bm) const
{
    if (m_core == bm.m_core) return true;

    if (m_core->m_info != bm.m_core->m_info) return false;
    aint ln = m_core->m_info.sz.x * m_core->m_info.bytepp();
    if ( m_core->m_info.pitch == ln && bm.m_core->m_info.pitch == ln )
    {
        int sz = m_core->m_info.pitch * m_core->m_info.sz.y;
        return 0 == memcmp(m_core + 1, bm.m_core + 1, sz);
    }
    
    const uint8 *d1 = (const uint8 *)m_core + 1;
    const uint8 *d2 = (const uint8 *)bm.m_core + 1;
    aint cnt = m_core->m_info.sz.y;
    for(aint i=0;i<cnt;++i)
    {
        if (0 != memcmp(d1, d2, ln))
            return false;
        d1 += m_core->m_info.pitch;
        d2 += bm.m_core->m_info.pitch;
    }
    return true;
}

bool bmpcore_exbody_s::operator==(const bmpcore_exbody_s & bm) const
{
    if (m_body == bm.m_body) return m_info == bm.m_info;
    if (m_info != bm.m_info) return false;

    aint ln = m_info.sz.x * m_info.bytepp();
    if (m_info.pitch == ln && bm.m_info.pitch == ln)
    {
        int sz = m_info.pitch * m_info.sz.y;
        return 0 == memcmp(m_body, bm.m_body, sz);
    }

    const uint8 *d1 = (const uint8 *)m_body;
    const uint8 *d2 = (const uint8 *)bm.m_body;
    aint cnt = m_info.sz.y;
    for (aint i = 0; i < cnt; ++i)
    {
        if (0 != memcmp(d1, d2, ln))
            return false;
        d1 += m_info.pitch;
        d2 += bm.m_info.pitch;
    }
    return true;
}


#pragma warning (push)
#pragma warning (disable : 4731)

template <typename CORE> void bitmap_t<CORE>::convert_24to32(bitmap_c &imgout) const
{
    if (info().bitpp != 24) return;

	if (info().pitch * info().sz.y % 12 != 0) // for small images 1x1 or 2x2
	{
		imgout.create_RGBA(info().sz);
		imgout.copy(ts::ivec2(0),info().sz,extbody(),ts::ivec2(0));
		return;
	}

    int cnt = info().pitch * info().sz.y / 12;
    void *src = body();

    imgout.create( info().sz, 32 );

    void *dst = imgout.body();

    _asm
    {
        push ebp
        mov esi, src;
        mov edi, dst
        mov ebp, cnt

loop_1112:
        mov eax, [esi]
        mov ebx, [esi + 4]
        mov ecx, [esi + 8]

        //  R  G  B   R  G  B   R  G  B   R  G  B
        //  == == ==  == == ==  == == ==  == == ==
        //  C3 C2 C1  C0 B3 B2  B1 B0 A3  A2 A1 A0

        mov edx, eax
        or  edx, 0xFF000000
        mov [edi], edx

        mov ax, bx              // � eax : BxRG
        rol eax, 8              // � eax : xRGB
        or  eax, 0xFF000000
        mov [edi+4], eax

        mov bl, cl              // GBxR
        ror ebx, 16             // xRGB
        or  ebx, 0xFF000000
        mov [edi+8], ebx

        shr ecx, 8              // 
        or  ecx, 0xFF000000
        mov [edi+12], ecx

        add esi, 12
        add edi, 16

        dec ebp
        jnz loop_1112;


        pop ebp
    }
}
#pragma warning (pop)

template<typename CORE> void bitmap_t<CORE>::convert_from_yuv( const ivec2 & pdes, const ivec2 & size, const uint8 *src, yuv_fmt_e fmt )
{
    ASSERT( info().sz >>= (pdes + size) );
    ASSERT( info().bytepp() == 4 );
    img_helper_yuv2rgb(body(pdes), info( irect(0, size) ), src, fmt);
}

template<typename CORE> void bitmap_t<CORE>::convert_to_yuv( const ivec2 & pdes, const ivec2 & size, buf_c &b, yuv_fmt_e fmt )
{
    ASSERT(info().sz >>= (pdes + size));
    ASSERT(info().bytepp() == 4);

    aint bsz = 0;
    switch (fmt)
    {
    case ts::YFORMAT_YUY2:
        break;
    case ts::YFORMAT_I420:
        bsz = size.x * size.y;
        bsz += bsz/2;
        break;
    case ts::YFORMAT_I420x2:
        break;
    }
    if (bsz == 0)
        return;
    b.set_size(bsz, false);

    img_helper_rgb2yuv(b.data(), info(irect(0, size)), body(pdes), fmt);
}

template<typename CORE> void bitmap_t<CORE>::shrink_2x_to(const ivec2 &lt_source, const ivec2 &sz_source, const bmpcore_exbody_s &eb_target) const
{
    if(info().bytepp()==2 || (sz_source /2) != eb_target.info().sz || info().bytepp() != eb_target.info().bytepp()) return;

    uint8 *dst = eb_target();
    const uint8 *src = body(lt_source);

    img_helper_shrink_2x(dst, src, eb_target.info(), info( irect(0, sz_source) ));
}


#if 0

template <typename CORE> void bitmap_t<CORE>::convert_32to16(bitmap_c &imgout) const
{
    if (info().bitpp != 32) return;

    int cnt = info().pitch * info().sz.y / info().bytepp();
    uint32 *src = (uint32 *)body();
    uint16  *dst;
    int newp = m_core->m_pitch >> 1;
    if (m_core->m_ref > 1)
    {
        int sz = m_core->m_size.x * 2 * m_core->m_size.y;
        m_core->ref_dec();
        create_normal( sz );
    }

    dst = (uint16 *)body();


    while (cnt-- > 0)
    {

        uint32 s = *(src);
        *dst = uint16( ((s >> 8) & 0xF800) |
                     ((s >> 5) & 0x07E0) |
                     ((s >> 3) & 0x001F) );

        ++src;
        ++dst;
    }
    m_core->m_bitPP = 16;
    m_core->m_bytePP = 2;
    m_core->m_pitch = newp;

}

template <typename CORE> void bitmap_t<CORE>::convert_32to24(bitmap_c &imgout) const
{
	if (info().bitpp != 32) return;

	int cnt = m_core->m_pitch * m_core->m_size.y / m_core->m_bytePP;
	uint32 *src = (uint32 *)body();
	int newp = m_core->m_pitch * 3 / 4;
	if (m_core->m_ref > 1)
	{
		int sz = m_core->m_size.x * 3 * m_core->m_size.y;
		m_core->ref_dec();
		create_normal( sz );
	}

	uint8 *dst = body();

	while (cnt--)
		*((uint32*)dst) = *src++, dst += 3;

	m_core->m_bitPP = 24;
	m_core->m_bytePP = 3;
	m_core->m_pitch = newp;
}

template<typename CORE> void bitmap_t<CORE>::crop(const ivec2 & left_up,const ivec2 & size)
{
    int nx = size.x;
    int ny = size.y;
    
    int op = info().pitch;
    int obytepp = info().bytepp();
    int obitpp = info().bitpp;

    const uint8 *sou = body() + info().pitch * left_up.y + obytepp * left_up.x;
    if (m_core->m_ref > 1)
    {
        int sz = nx * obytepp * ny;
        m_core->ref_dec();
        create_normal( sz );

        m_core->m_bitPP = obitpp;
        m_core->m_bytePP = obytepp;
    }
    uint8  *des = body();

    m_core->m_size.x = nx;
    m_core->m_size.y = ny;
    m_core->m_pitch = obytepp * nx;
    copy(des,sou,nx,ny,m_core->m_pitch,obytepp,op,obytepp);
}

template<typename CORE> void bitmap_t<CORE>::crop_to_square()
{
    if (info().sz.x > info().sz.y)
    {
        int d = info().sz.x - info().sz.y;
        int d0 = d/2;
        int d1 = d-d0;
        crop( ivec2(d0,0), ivec2(info().sz.x-d0-d1,info().sz.y) );

    } else if (info().sz.x < info().sz.y)
    {
        int d = info().sz.y - info().sz.x;
        int d0 = d/2;
        int d1 = d-d0;
        crop( ivec2(0, d0), ivec2(info().sz.x,info().sz.y-d0-d1) );
    }
}

template<typename CORE> void bitmap_t<CORE>::make_larger(int factor, bitmap_t<CORE> &out) const
{
	ivec2 newsz = size() * factor;

    if(out.info().sz.x != newx || out.info().sz.y != newy || out.info().bytepp() != info().bytepp() || out.m_core->m_ref > 1)
    {
        if(info().bytepp()==1) out.create_grayscale(newx,newy);
        else if(info().bytepp()==2) out.create_16(newx,newy);
        else if(info().bytepp()==3) out.create_RGB(newx,newy);
		else if(info().bytepp()==4) out.create_RGBA(newx,newy);
	}

	uint8 * des = out.body();;
	const uint8 * sou = body();

    int add = info().bytepp()*factor;
    int addl = info().pitch*factor;

    if(info().bytepp()==1)
    {
        for(int y=0;y<info().sz.y;++y)
        {
		    for(int x=0;x<info().sz.x;++x,des+=add)
            {
                memset(des,*((uint8 *)sou + x), factor);
		    }

            for (int k = 0; k<(factor-1); ++k)
            {
                memcpy(des,des - addl, addl);
                des += addl;
            }

            sou += info().pitch;
	    }
    } else
    if(info().bytepp()==2)
    {
        for(int y=0;y<info().sz.y;++y)
        {
		    for(int x=0;x<info().sz.x;++x,des+=add)
            {
                uint16 w = *(uint16 *)(sou + x * 2);
                for (int k = 0; k<factor; ++k)
                {
                    *(((uint16 *)des) + k) = w;
                }
		    }			
            for (int k = 0; k<(factor-1); ++k)
            {
                memcpy(des,des - addl, addl);
                des += addl;
            }

            sou += info().pitch;
	    }
    } else
    if(info().bytepp()==3)
    {
        for(int y=0;y<info().sz.y;++y)
        {
		    for(int x=0;x<info().sz.x;++x,des+=add)
            {
                uint16 w = *(uint16 *)(sou + x * 3);
                uint8 b = *(uint8 *)(sou + x * 3 + 2);
                for (int k = 0; k<factor; ++k)
                {
                    *((uint16 *)(des + k*3)) = w;
                    *((uint8 *)(des + k*3 + 2)) = b;
                }
		    }			
            for (int k = 0; k<(factor-1); ++k)
            {
                memcpy(des,des - addl, addl);
                des += addl;
            }

            sou += info().pitch;
	    }
    } else
    if(info().bytepp()==4)
    {
        for(int y=0;y<info().sz.y;++y)
        {
		    for(int x=0;x<info().sz.x;++x,des+=add)
            {
                uint32 dw = *(uint32 *)(sou + x * 4);
                for (int k = 0; k<factor; ++k)
                {
                    *(((uint32 *)des) + k) = dw;
                }
            }
            for (int k = 0; k<(factor-1); ++k)
            {
                memcpy(des,des - addl, addl);
                des += addl;
            }

            sou += info().pitch;
        }
    }
}

#endif

template<typename CORE> irect bitmap_t<CORE>::calc_visible_rect() const
{
    irect r;
    r.lt = info().sz - 1;
    r.rb.x = 0;
    r.rb.y = 0;

    uint32 * ptr=(uint32 *)body();

    for (int j = 0; j<info().sz.y; ++j)
    {
        for (int i = 0; i<info().sz.x; ++i)
        {
            uint32 ap = (0xFF000000 & (*(ptr + i)));
            bool t_empty_found = (ap == 0x00000000);
            //bool t_solid_found = (ap == 0xFF000000);

            if (!t_empty_found)
            {
				if (i < r.lt.x) r.lt.x = i;
				if (i > r.rb.x) r.rb.x = i;
				if (j < r.lt.y) r.lt.y = j;
				if (j > r.rb.y) r.rb.y = j;
            }

        }
        ptr = (uint32 *)(((uint8 *)ptr) + info().pitch);
    }

    return r;
}

template<typename CORE> uint32 bitmap_t<CORE>::get_area_type(const ivec2 & p,const ivec2 & size) const
{
    if (info().bytepp()!=4) return IMAGE_AREA_SOLID;
	uint32 * ptr=(uint32 *)((uint8 *)body()+4*p.x+info().pitch*p.y);

    uint32 at = 0;

    for (int j = 0; j<size.y; ++j)
    {
        for (int i = 0; i<size.x; ++i)
        {
            uint32 ap = (0xFF000000 & (*(ptr + i)));
            bool t_empty_found = (ap == 0x00000000);
            bool t_solid_found = (ap == 0xFF000000);

            if ( !t_empty_found && !t_solid_found  ) at |=IMAGE_AREA_SEMITRANSPARENT;
            if (t_empty_found) at |= IMAGE_AREA_TRANSPARENT;
            if (t_solid_found) at |= IMAGE_AREA_SOLID;

            if (at == IMAGE_AREA_ALLTYPE) return at;
        }
        ptr = (uint32 *)(((uint8 *)ptr) + info().pitch);
    }
    return at;
}

template<typename CORE> void bitmap_t<CORE>::tile(const ivec2 & pdes,const ivec2 & desize, const bmpcore_exbody_s & bmsou,const ivec2 & spsou, const ivec2 & szsou)
{
    copy(pdes, szsou, bmsou, spsou);

    // filling by x

    int cx = szsou.x;
    while (cx < desize.x)
    {
        int w = tmin((desize.x-cx), cx);
        copy( ivec2(pdes.x + cx, pdes.y), ivec2(w,szsou.y), extbody(), pdes);
        cx += w;
    }

    // filling by y

    int cy = szsou.y;
    while (cy < desize.y)
    {
        int h = tmin((desize.y-cy), cy);
        copy( ivec2(pdes.x, pdes.y + cy), ivec2(desize.x,h), extbody(), pdes);
        cy += h;
    }
}

template<typename CORE> void bitmap_t<CORE>::copy_components(bitmap_c &imageout, int num_comps, int dst_first_comp, int src_first_comp) const
{
	ASSERT(imageout.info().sz == info().sz && dst_first_comp+num_comps <= imageout.info().bytepp() && src_first_comp+num_comps <= info().bytepp());
	imageout.before_modify();
	img_helper_copy_components(imageout.body() + dst_first_comp, body() + src_first_comp, imageout.info(), info(), num_comps);
}

template<typename CORE> void bitmap_t<CORE>::copy(const ivec2 &pdes, const ivec2 &size, const bmpcore_exbody_s &bmsou, const ivec2 &spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+size.x)>bmsou.info().sz.x || (spsou.y+size.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	const uint8 * sou=bmsou()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;

    if (bmsou.info().bytepp() == info().bytepp())
    {
        int len = size.x * info().bytepp();
	    for(int y=0;y<size.y;y++,des+=info().pitch,sou+=bmsou.info().pitch)
        {
            memcpy(des, sou, len);
	    }
    } else
        // TODO : fix this brunch for non 3 or 4 bytePP images
    {
        uint32 alpha = bmsou.info().bytepp()==3?0xFF000000:0;
	    for(int y=0;y<size.y;y++,des+=info().pitch,sou+=bmsou.info().pitch)
        {
            uint8 *des1 = des;
            const uint8 *sou1 = sou;
            for (int i = 0; i<(size.x-1); ++i, sou1 += bmsou.info().bytepp(), des1 += info().bytepp())
            {
                uint32 color = *((uint32 *)sou1) | alpha;
                *((uint32 *)des1) = color;
            }
            int c = tmin(bmsou.info().bytepp(), info().bytepp());
            while (c--)
            {
                *des1++ = *sou1++;
            }
            if (alpha) *des1 = 0xFF;
        }
    }
}

template<typename CORE> void bitmap_t<CORE>::fill_alpha(uint8 a)
{
    ASSERT( info().bytepp() == 4 );
    before_modify();

    if ( info().pitch == 4 * info().sz.x )
    {
        uint8 * o = body() + 3;
        int cnt = info().sz.x * info().sz.y;
        for (; cnt > 0; --cnt, o += 4)
            *o = a;
    } else
        fill_alpha(ts::ivec2(0), info().sz, a);
}

template<typename CORE> void bitmap_t<CORE>::fill_alpha(const ivec2 & pdes, const ivec2 & size, uint8 a)
{
    ASSERT( info().bytepp() == 4 );
    if (pdes.x < 0 || pdes.y < 0) return;
    if ((pdes.x + size.x) > info().sz.x || (pdes.y + size.y) > info().sz.y) return;

    before_modify();

    uint8 * des = body() + info().bytepp()*pdes.x + info().pitch*pdes.y + 3;
    int desnl = info().pitch - size.x*info().bytepp();
    int desnp = info().bytepp();

    for (int y = 0; y < size.y; y++, des += desnl) {
        for (int x = 0; x < size.x; x++, des += desnp) {
            *(uint8 *)des = a;
        }
    }
}

template<typename CORE> bool bitmap_t<CORE>::has_alpha() const
{
    if (info().bytepp() != 4) return false;

    if (info().pitch == 4 * info().sz.x)
    {
        const uint8 * o = body() + 3;
        int cnt = info().sz.x * info().sz.y;
        for (; cnt > 0; --cnt, o += 4)
            if (*o < 255) return true;
        return false;
    }
    return has_alpha(ts::ivec2(0), info().sz);

}

template<typename CORE> bool bitmap_t<CORE>::has_alpha(const ivec2 & pdes, const ivec2 & size) const
{
    if (info().bytepp() != 4) return false;
    if (pdes.x < 0 || pdes.y < 0) return false;
    if ((pdes.x + size.x) > info().sz.x || (pdes.y + size.y) > info().sz.y) return false;

    const uint8 * des = body() + info().bytepp()*pdes.x + info().pitch*pdes.y + 3;
    int desnl = info().pitch - size.x*info().bytepp();
    int desnp = info().bytepp();

    for (int y = 0; y < size.y; y++, des += desnl)
        for (int x = 0; x < size.x; x++, des += desnp)
            if (*des < 255) return true;

    return false;
}

template<typename CORE> void bitmap_t<CORE>::detect_alpha_channel( const bitmap_t<CORE> & bmsou )
{
    class ADFILTERFILTER
    {
    public:

        bool begin( const bitmap_t<CORE> & tgt, const bitmap_t<CORE> & src )
        {
            return src.info().bitpp == 32 && tgt.info().bitpp == 32;
        }

        void point( uint8 * me, const FMATRIX &m )
        {
            // me = black
            // from = white

            const uint8 * from = m[1][1];

            if ( me[0] == from[0] && me[1] == from[1] && me[2] == from[2] )
            {
                me[3] = 0xFF;
            } else if ( me[0] == 0 && me[1] == 0 && me[2] == 0 && from[0] == 255 && from[1] == 255 && from[2] == 255 )
            {
                me[3] = 0x0;

            } else 
            {
                int a0 = 255 - from[0] + me[0];
                int a1 = 255 - from[1] + me[1];
                int a2 = 255 - from[2] + me[2];

                int a = tmax( a0, a1, a2 );

                me[0] = as_byte( me[0] * 255 / a );
                me[1] = as_byte( me[1] * 255 / a );
                me[2] = as_byte( me[2] * 255 / a );
                me[3] = as_byte( a );

            }

        }
    };
    
    ADFILTERFILTER xx;
    bmsou.copy_with_filter(*this,xx);
}

template<typename CORE> void bitmap_t<CORE>::fill(TSCOLOR color)
{
    before_modify();
    img_helper_fill( body(), info(), color );
}

template<typename CORE> void bitmap_t<CORE>::fill(const ivec2 & pdes,const ivec2 & size, TSCOLOR color)
{
	if(pdes.x<0 || pdes.y<0 || (pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y)
    {
        ivec2 pos(pdes);
        ivec2 sz(size);
        if (pos.x < 0) { sz.x += pos.x; pos.x = 0; }
        if (pos.y < 0) { sz.y += pos.y; pos.y = 0; }
        if ((pos.x+sz.x)>info().sz.x) sz.x = info().sz.x - pos.x;
        if ((pos.y+sz.y)>info().sz.y) sz.y = info().sz.y - pos.y;
        if (sz >> 0) fill(pos,sz,color);
        return;
    }
    if (!(size >> 0)) return;

    before_modify();
    img_helper_fill( body()+info().bytepp()*pdes.x+info().pitch*pdes.y, imgdesc_s(info(), size), color );
}

template<typename CORE> void bitmap_t<CORE>::overfill(const ivec2 & pdes,const ivec2 & size, TSCOLOR color)
{
    if (pdes.x<0 || pdes.y<0 || (pdes.x + size.x)>info().sz.x || (pdes.y + size.y)>info().sz.y)
    {
        ivec2 pos(pdes);
        ivec2 sz(size);
        if (pos.x < 0) { sz.x += pos.x; pos.x = 0; }
        if (pos.y < 0) { sz.y += pos.y; pos.y = 0; }
        if ((pos.x + sz.x) > info().sz.x) sz.x = info().sz.x - pos.x;
        if ((pos.y + sz.y) > info().sz.y) sz.y = info().sz.y - pos.y;
        if (sz >> 0) overfill(pos, sz, color);
        return;
    }
    if (!ASSERT(size >> 0)) return;

    before_modify();

    // formula is: (1-src.alpha) * dst + src.color
    // dont check 0 alpha due it can be (1 * desc + src) op

    uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
    int desnl=info().pitch-size.x*info().bytepp();
    int desnp=info().bytepp();

    if(info().bytepp()==1) {
        // unsupported
    } else if(info().bytepp()==2) {
        // unsupported
    } else if(info().bytepp()==3) {

        double a = ((double)ALPHA(color) * 1.0 / 255.0);
        double not_a = 1.0 - a;

        for(int y=0;y<size.y;y++,des+=desnl) {
            for(int x=0;x<size.x;x++,des+=desnp) {

                auint oiB = lround(float(des[0]) * not_a) + BLUE(color);
                auint oiG = lround(float(des[1]) * not_a) + GREEN(color);
                auint oiR = lround(float(des[2]) * not_a) + RED(color);

                *(uint16 *)des = CLAMP<uint8>(oiB) | (CLAMP<uint8>(oiG) << 8);
                *(uint8 *)(des+2) = (uint8)CLAMP<uint8>(oiR);

            }			
        }
    } else if(info().bytepp()==4) {

        double a = ((double)ALPHA(color) * 1.0 / 255.0);
        double not_a = 1.0 - a;

        for(int y=0;y<size.y;y++,des+=desnl) {
            for(int x=0;x<size.x;x++,des+=desnp) {

                TSCOLOR c = *(TSCOLOR *)des;

                auint oiB = lround(float(BLUE(c)) * not_a) + BLUE(color);
                auint oiG = lround(float(GREEN(c)) * not_a) + GREEN(color);
                auint oiR = lround(float(RED(c)) * not_a) + RED(color);
                auint oiA = lround(float(ALPHA(c)) * not_a) + ALPHA(color);

                c = CLAMP<uint8>(oiB) | (CLAMP<uint8>(oiG) << 8) | (CLAMP<uint8>(oiR) << 16) | (CLAMP<uint8>(oiA) << 24);

                *(TSCOLOR *)des = c;
            }			
        }
    }
}

template<typename CORE> void bitmap_t<CORE>::premultiply()
{
    if(info().bytepp()!=4) return;
    before_modify();
    img_helper_premultiply( body(), info() );
}

template<typename CORE> void bitmap_t<CORE>::premultiply( const irect &rect )
{
    if (info().bytepp() != 4) return;
    before_modify();
    img_helper_premultiply( body(rect.lt), info(rect) );
}


#if 0

template<typename CORE> void bitmap_t<CORE>::rotate_90(bitmap_c& outimage, const ivec2 & pdes,const ivec2 & sizesou,const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+sizesou.x)>bmsou.info().sz.x || (spsou.y+sizesou.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+sizesou.y)>info().sz.x || (pdes.y+sizesou.x)>info().sz.y) return;

	if(bmsou.info().bytepp()!=info().bytepp()) return;
	if(bmsou.info().bitpp!=info().bitpp) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-sizesou.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	int soull=bmsou.info().pitch;
	int sounp=bmsou.info().bytepp();

	sou=sou+(sizesou.x-1)*sounp;

	for(int y=0;y<sizesou.x;y++,des+=desnl,sou=sou-soull*sizesou.y-sounp) {
		for(int x=0;x<sizesou.y;x++,des+=desnp,sou+=soull) {
			switch(info().bytepp()) {
				case 1: *(uint8 *)des=*(uint8 *)sou; break;
				case 2: *(uint16 *)des=*(uint16 *)sou; break;
				case 3: *(uint16 *)des=*(uint16 *)sou; *(uint8 *)(des+2)=*(uint8 *)(sou+2); break;
				case 4: *(uint32 *)des=*(uint32 *)sou;break;
			}
		}			
	}
}

template<typename CORE> void bitmap_t<CORE>::rotate_180(bitmap_c& outimage, const ivec2 & pdes,const ivec2 & sizesou, const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+sizesou.x)>bmsou.info().sz.x || (spsou.y+sizesou.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+sizesou.x)>info().sz.x || (pdes.y+sizesou.y)>info().sz.y) return;

	if(bmsou.info().bytepp()!=info().bytepp()) return;
	if(bmsou.info().bitpp!=info().bitpp) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-sizesou.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	int sounl=bmsou.info().pitch-sizesou.x*bmsou.info().bytepp();
	int soull=bmsou.info().pitch;
	int sounp=bmsou.info().bytepp();

	sou=sou+(sizesou.x-1)*sounp+(sizesou.y-1)*soull;

	for(int y=0;y<sizesou.y;y++,des+=desnl,sou=sou-sounl) {
		for(int x=0;x<sizesou.x;x++,des+=desnp,sou-=sounp) {
			switch(info().bytepp()) {
				case 1: *(uint8 *)des=*(uint8 *)sou; break;
				case 2: *(uint16 *)des=*(uint16 *)sou; break;
				case 3: *(uint16 *)des=*(uint16 *)sou; *(uint8 *)(des+2)=*(uint8 *)(sou+2); break;
				case 4: *(uint32 *)des=*(uint32 *)sou;break;
			}
		}			
	}
}

template<typename CORE> void bitmap_t<CORE>::rotate_270(bitmap_c& outimage, const ivec2 & pdes,const ivec2 & sizesou, const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+sizesou.x)>bmsou.info().sz.x || (spsou.y+sizesou.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+sizesou.y)>info().sz.x || (pdes.y+sizesou.x)>info().sz.y) return;

	if(bmsou.info().bytepp()!=info().bytepp()) return;
	if(bmsou.info().bitpp!=info().bitpp) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-sizesou.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	//int sounl=bmsou.info().pitch-sizesou.x*bmsou.info().bytepp();
	int soull=bmsou.info().pitch;
	int sounp=bmsou.info().bytepp();

	sou=sou+(sizesou.y-1)*soull;


	for(int y=0;y<sizesou.x;y++,des+=desnl,sou=sou+soull*sizesou.y+sounp) {
		for(int x=0;x<sizesou.y;x++,des+=desnp,sou-=soull) {
			switch(info().bytepp()) {
				case 1: *(uint8 *)des=*(uint8 *)sou; break;
				case 2: *(uint16 *)des=*(uint16 *)sou; break;
				case 3: *(uint16 *)des=*(uint16 *)sou; *(uint8 *)(des+2)=*(uint8 *)(sou+2); break;
				case 4: *(uint32 *)des=*(uint32 *)sou;break;
			}
		}			
	}
}

#endif

template<typename CORE> void bitmap_t<CORE>::flip_x(const ivec2 & pdes,const ivec2 & size, const bitmap_t<CORE> & bmsou,const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+size.x)>bmsou.info().sz.x || (spsou.y+size.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

	if(bmsou.info().bytepp()!=info().bytepp()) return;
	if(bmsou.info().bitpp!=info().bitpp) return;

    before_modify(); //TODO: multicore optimizations

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	int soull=bmsou.info().pitch;
	int sounp=bmsou.info().bytepp();

	sou=sou+(size.x-1)*sounp;

	for(int y=0;y<size.y;y++,des+=desnl,sou=sou+soull+(size.x)*sounp) {
		for(int x=0;x<size.x;x++,des+=desnp,sou-=sounp) {
			switch(info().bytepp()) {
				case 1: *(uint8 *)des=*(uint8 *)sou; break;
				case 2: *(uint16 *)des=*(uint16 *)sou; break;
				case 3: *(uint16 *)des=*(uint16 *)sou; *(uint8 *)(des+2)=*(uint8 *)(sou+2); break;
				case 4: *(uint32 *)des=*(uint32 *)sou;break;
			}
		}			
	}
}

template<typename CORE> void bitmap_t<CORE>::flip_y()
{
    before_modify(); //TODO: multicore optimizations

    uint8 *l1 = body();
    uint8 *l2 = body() + info().pitch * (info().sz.y-1);
    
    while (l1<l2)
    {
        int cnt = info().pitch;
        uint8 *p0 = l1;
        uint8 *p1 = l2;
        while (cnt >= 4)
        {
            uint32 temp = *(uint32 *)p0;
            *(uint32 *)p0 = *(uint32 *)p1;
            *(uint32 *)p1 = temp;
            p0 += 4;
            p1 += 4;
            cnt -= 4;
        }
        while (cnt > 0)
        {
            BYTE temp = *p0;
            *p0 = *p1;
            *p1 = temp;
            ++p0;
            ++p1;
            --cnt;
        }

        l1+=info().pitch;
        l2-=info().pitch;
    }

}

template<typename CORE> void bitmap_t<CORE>::flip_y(const ivec2 & pdes,const ivec2 & size, const bitmap_t<CORE> & bmsou,const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+size.x)>bmsou.info().sz.x || (spsou.y+size.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

	if(bmsou.info().bytepp()!=info().bytepp()) return;
	if(bmsou.info().bitpp!=info().bitpp) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	int sounl=bmsou.info().pitch-size.x*bmsou.info().bytepp();
	int soull=bmsou.info().pitch;
	int sounp=bmsou.info().bytepp();

	sou=sou+(size.y-1)*soull;



	for(int y=0;y<size.y;y++,des+=desnl,sou=sou+sounl-soull*2) {
		for(int x=0;x<size.x;x++,des+=desnp,sou+=sounp) {
			switch(info().bytepp()) {
				case 1: *(uint8 *)des=*(uint8 *)sou; break;
				case 2: *(uint16 *)des=*(uint16 *)sou; break;
				case 3: *(uint16 *)des=*(uint16 *)sou; *(uint8 *)(des+2)=*(uint8 *)(sou+2); break;
				case 4: *(uint32 *)des=*(uint32 *)sou;break;
			}
		}			
	}
}

template<typename CORE> void bitmap_t<CORE>::flip_y(const ivec2 & pdes,const ivec2 & size)
{
	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int sounl=info().pitch-size.x*info().bytepp();
	int soull=info().pitch;
	int sounp=info().bytepp();

	sou=sou+(size.y-1)*soull;

	uint32 t;

	for(int y=0;y<(size.y >> 1);y++,des+=desnl,sou=sou+sounl-soull*2) {
		for(int x=0;x<size.x;x++,des+=desnp,sou+=sounp) {
			switch(info().bytepp()) {
				case 1: 
					t=*(uint8 *)des;
					*(uint8 *)des=*(uint8 *)sou; 
					*(uint8 *)sou=BYTE(t);
					break;
				case 2: 
					t=*(uint16 *)des;
					*(uint16 *)des=*(uint16 *)sou; 
					*(uint16 *)sou=uint16(t);
					break;
				case 3: 
					t=*(uint16 *)des;
					*(uint16 *)des=*(uint16 *)sou; 
					*(uint16 *)sou=uint16(t);

					t=*(uint8 *)(des+2);
					*(uint8 *)(des+2)=*(uint8 *)(sou+2); 
					*(uint8 *)(sou+2)=BYTE(t);
					break;
				case 4: 
					t=*(uint32 *)des;
					*(uint32 *)des=*(uint32 *)sou;
					*(uint32 *)sou=t;
					break;
			}
		}			
	}

}

template<typename CORE> void bitmap_t<CORE>::alpha_blend( const ivec2 &p, const bmpcore_exbody_s & img )
{
    if ( info().bytepp() != 4 )
        alpha_blend(p,img,extbody());
    else
    {
        // do photoshop normal mode mixing
        before_modify();

        const uint8 * s = img();

        ts::ivec2 px = p;
        ts::ivec2 isz = img.info().sz;

        if (px.x < 0) { s -= px.x * sizeof(TSCOLOR); isz.x += px.x; px.x = 0; };
        if (px.y < 0) { s -= px.y * img.info().pitch; isz.y += px.y; px.y = 0; };

        uint8 * t = body() + info().bytepp() * px.x + info().pitch * px.y;
        int srcwidth = tmin( isz.x, info().sz.x - px.x );
        int srcheight = tmin( isz.y, info().sz.y - px.y );

        for( int y=0; y<srcheight; ++y, t += info().pitch, s += img.info().pitch )
        {
            TSCOLOR *tgt = (TSCOLOR *)t;
            const TSCOLOR *src = (const TSCOLOR *)s;
            for(int x = 0; x<srcwidth; ++x, ++tgt, ++src)
                *tgt = ALPHABLEND( *tgt, *src );
        }

    }
}

template<typename CORE> void bitmap_t<CORE>::alpha_blend( const ivec2 &p, const bmpcore_exbody_s & img, const bmpcore_exbody_s & base )
{
    ASSERT( img.info().bytepp() == 4 );
    if (info().sz != base.info().sz)
        create_RGBA( base.info().sz );
    else
        before_modify();

    irect baserect( 0, base.info().sz - p );
    baserect.intersect( irect( 0, img.info().sz ) );

    ts::ivec2 imgwh( tmin(img.info().sz, info().sz - p) );
    if (base() != body())
    {
        // we have to copy base part outside of img rectange
        if (p.y > 0) // top
            img_helper_copy( body(), base(), imgdesc_s(info()).set_height(p.y), imgdesc_s(base.info()).set_height(p.y) );

        if (imgwh.y > 0)
        {

            if (p.x > 0) // left
                img_helper_copy(body() + p.y * info().pitch,
                                base() + p.y * base.info().pitch,
                                imgdesc_s(info()).set_width(p.x).set_height(imgwh.y),
                                imgdesc_s(base.info()).set_width(p.x).set_height(imgwh.y));

            int r_space = info().sz.x - p.x - img.info().sz.x;
            if (r_space > 0) // rite
                img_helper_copy(body() + p.y * info().pitch + (info().sz.x-r_space) * info().bytepp(),
                                base() + p.y * base.info().pitch + (info().sz.x-r_space) * base.info().bytepp(),
                                imgdesc_s(info()).set_width(r_space).set_height(imgwh.y),
                                imgdesc_s(base.info()).set_width(r_space).set_height(imgwh.y));
        }
        int b_space = info().sz.y - p.y - img.info().sz.y;
        if (b_space > 0) // bottom
            img_helper_copy(body() + (info().sz.y - b_space) * info().pitch, base() + (info().sz.y - b_space) * base.info().pitch, imgdesc_s(info()).set_height(b_space), imgdesc_s(base.info()).set_height(b_space));
    }

    // alphablend itself
    img_helper_merge_with_alpha(body(p), base() + p.x * base.info().bytepp() + p.y * base.info().pitch, img(), imgdesc_s(info()).set_size(imgwh), imgdesc_s(base.info()).set_size(imgwh), imgdesc_s(img.info()).set_size(imgwh));

}

template<typename CORE> void bitmap_t<CORE>::swap_byte(const ivec2 & pos,const ivec2 & size,int n1,int n2)
{
	if(n1==n2) return;
	if(n1<0 || n1>=info().bytepp()) return;
	if(n2<0 || n2>=info().bytepp()) return;
	if(info().bytepp()<=1) return;

    before_modify();

	uint8 * buf=body()+info().bytepp()*pos.x+info().pitch*pos.y;
	int bufnl=info().pitch-size.x*info().bytepp();
	int bufnp=info().bytepp();

	uint8 zn;

	for(int y=0;y<size.y;y++,buf+=bufnl) {
		for(int x=0;x<size.x;x++,buf+=bufnp) {
			zn=*(buf+n1); *(buf+n1)=*(buf+n2); *(buf+n2)=zn;
		}
	}
}

#if 0
template<typename CORE> void bitmap_t<CORE>::merge_with_alpha(const ivec2 & pdes,const ivec2 & size, const bitmap_c & bmsou,const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+size.x)>bmsou.info().sz.x || (spsou.y+size.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

	if(info().bytepp()!=3 && info().bytepp()!=4) return;
	if(bmsou.info().bytepp()!=4) return;

    before_modify();

    merge_with_alpha( 
        body()+info().bytepp()*pdes.x+info().pitch*pdes.y,
        bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y,
        size.x,
        size.y,
        info().pitch,
        info().bytepp(),
        bmsou.info().pitch
        );
}

template<typename CORE> void bitmap_t<CORE>::merge_with_alpha_PM(const ivec2 & pdes,const ivec2 & size, const bitmap_c & bmsou,const ivec2 & spsou)
{

	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+size.x)>bmsou.info().sz.x || (spsou.y+size.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

	if(bmsou.info().bytepp()!=4) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x*info().bytepp();
	int desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	int sounl=bmsou.info().pitch-size.x*bmsou.info().bytepp();
	int sounp=bmsou.info().bytepp();


	for(int y=0;y<size.y;y++,des+=desnl,sou+=sounl)
    {
        for(int x=0;x<size.x;x++,des+=desnp,sou+=sounp)
        {
			uint32 color=*(uint32 *)sou;
            uint32 ocolor = *des + (*(des+1) << 8) + (*(des+2) << 16);
			uint8 alpha = uint8(color>>24);
            uint8 R = uint8(color>>16);
            uint8 G = uint8(color>>8);
            uint8 B = uint8(color);

			uint8 oalpha = uint8(color>>24);
            uint8 oR = uint8(ocolor>>16);
            uint8 oG = uint8(ocolor>>8);
            uint8 oB = uint8(ocolor);

            if (alpha==0) continue;
			else
            if(alpha==255)
            {
                ocolor = color;
            } else
            {
                float A = alpha / 255.0f;

			    //*des=   uint8((uint32(B)*(uint32(alpha)<<8))>>16)+uint8((uint32(oB)*(uint32(255-alpha)<<8))>>16); // �� ������ ������ ������� =(

                auint oiB = lround(float(B) + float(oB) * (1.0f - A));
                auint oiG = lround(float(G) + float(oG) * (1.0f - A));
                auint oiR = lround(float(R) + float(oR) * (1.0f - A));

                auint oiA = lround(float(255-oalpha) * A);

				ocolor = CLAMP<uint8>(oiB) | (CLAMP<uint8>(oiG) << 8) | (CLAMP<uint8>(oiR) << 16) | (CLAMP<uint8>(oiA) << 24);

                
                /*
                *des=   uint8((uint32(*sou)*(uint32(alpha)<<8))>>16)
						    +uint8((uint32(*des)*(uint32(255-alpha)<<8))>>16); // �� ������ ������ ������� =(
			    sou++; des++;
			    *des=   uint8((uint32(*sou)*(uint32(alpha)<<8))>>16)
						    +uint8((uint32(*des)*(uint32(255-alpha)<<8))>>16); // �� ������ ������ ������� =(
			    sou++; des++;

			    *des+=uint8(((255-*des)*(uint32(alpha)<<8))>>16);
                */
            }
            if (info().bytepp() == 3)
            {
                *des = uint8(ocolor);
                *(des+1) = uint8(ocolor>>8);
                *(des+2) = uint8(ocolor>>16);
            } else
            {

                *(uint32 *)des = ocolor;
            }

		}			
	}

}

template<typename CORE> void bitmap_t<CORE>::merge_grayscale_with_alpha(const ivec2 & pdes,const ivec2 & size, const bitmap_t<CORE> * data_src,const ivec2 & data_src_point, const bitmap_t<CORE> * alpha_src, const ivec2 & alpha_src_point)
{
    uint8 * dsou = nullptr;
    int dsounl = 0;

	if(data_src == nullptr)
    {
    } else
    {
        ASSERT(data_src->info().bitpp == 8);
	    dsou = data_src->body() + data_src_point.x + data_src->info().pitch*data_src_point.y;
	    dsounl=data_src->info().pitch-size.x;
    }
    ASSERT(alpha_src != nullptr && info().bitpp == 8);
    if (alpha_src->info().bitpp != 32) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x;

	const uint8 * asou=alpha_src->body() + alpha_src_point.x * 4 + alpha_src->info().pitch*alpha_src_point.y + 3;
	int asounl=alpha_src->info().pitch-size.x*4;

    if (dsou == nullptr)
    {
	    for(int y=0;y<size.y;y++,des+=desnl,asou+=asounl)
        {
		    for(int x=0;x<size.x;x++,++des, asou+=4)
            {
			    uint8 alpha = *asou;
                uint8 odata;

                if (alpha==0) continue;
			    else
                if(alpha==255)
                {
                    odata = 0;
                } else
                {
                    odata = uint8(float(255-alpha)*float(*des)*(1.0/255.0));
                }
                
                *des = odata;

		    }			
	    }
        return;
    }

	for(int y=0;y<size.y;y++,des+=desnl,asou+=asounl, dsou+=dsounl)
    {
		for(int x=0;x<size.x;x++,++des,++dsou, asou+=4)
        {
			uint8 alpha = *asou;
            uint8 odata;

            if (alpha==0) continue;
			else
            if(alpha==255)
            {
                odata = *dsou;
            } else
            {
                odata = uint8((float(255-alpha)*float(*des) + float(alpha)*float(*dsou))*(1.0/255.0));
            }
            
            *des = odata;

		}			
	}

}

template<typename CORE> void bitmap_t<CORE>::merge_grayscale_with_alpha_PM(const ivec2 & pdes,const ivec2 & size, const bitmap_t<CORE> * data_src,const ivec2 & data_src_point, const bitmap_t<CORE> * alpha_src, const ivec2 & alpha_src_point)
{
    uint8 * dsou = nullptr;
    int dsounl = 0;

	if(data_src == nullptr)
    {
    } else
    {
        ASSERT(data_src->info().bitpp == 8);
	    dsou = data_src->body() + data_src_point.x + data_src->info().pitch*data_src_point.y;
	    dsounl=data_src->info().pitch-size.x;
    }
    ASSERT(alpha_src != nullptr && info().bitpp == 8);
    if (alpha_src->info().bitpp != 32) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x;

	const uint8 * asou=alpha_src->body() + alpha_src_point.x * 4 + alpha_src->info().pitch*alpha_src_point.y + 3;
	int asounl=alpha_src->info().pitch-size.x*4;

    if (dsou == nullptr)
    {
	    for(int y=0;y<size.y;y++,des+=desnl,asou+=asounl)
        {
		    for(int x=0;x<size.x;x++,++des, asou+=4)
            {
			    uint8 alpha = *asou;
                uint8 odata;

                if (alpha==0) continue;
			    else
                if(alpha==255)
                {
                    odata = 0;
                } else
                {
                    odata = uint8(float(255-alpha)*float(*des)*(1.0/255.0));
                }
                
                *des = odata;

		    }			
	    }
        return;
    }

	for(int y=0;y<size.y;y++,des+=desnl,asou+=asounl, dsou+=dsounl)
    {
		for(int x=0;x<size.x;x++,++des,++dsou, asou+=4)
        {
            *des = uint8((float(255 - *asou)*float(*des) + 255.0*float(*dsou))*(1.0/255.0));
		}			
	}

}


template<typename CORE> void bitmap_t<CORE>::modulate_grayscale_with_alpha(const ivec2 & pdes,const ivec2 & size, const bitmap_t<CORE> * alpha_src, const ivec2 & alpha_src_point)
{
    ASSERT(alpha_src!=nullptr && info().bitpp == 8);

    if (alpha_src->info().bitpp != 32) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	int desnl=info().pitch-size.x;

	const uint8 * asou=alpha_src->body() + alpha_src_point.x * 4 + alpha_src->info().pitch*alpha_src_point.y + 3;
	int asounl=alpha_src->info().pitch-size.x*4;

	for(int y=0;y<size.y;y++,des+=desnl,asou+=asounl)
    {
		for(int x=0;x<size.x;x++,++des, asou+=4)
        {
			uint8 alpha = *asou;

            if (alpha==255) continue;
			else
            {
                *des = uint8((float(alpha)*float(*des))*(1.0/255.0));
            }

		}			
	}
}

#endif

void TSCALL sharpen_run(bitmap_c &obm, const uint8 *sou, const imgdesc_s &souinfo, int lv);
template<typename CORE> void bitmap_t<CORE>::sharpen(bitmap_c& outimage, int lv) const
{
    outimage.create_RGBA(info().sz);
    sharpen_run(outimage, body(), info(), lv);
}

bool resize3(const bmpcore_exbody_s &extbody, const uint8 *sou, const imgdesc_s &souinfo, resize_filter_e filt_mode);
template<typename CORE> bool bitmap_t<CORE>::resize_to(bitmap_c& obm, const ivec2 & newsize, resize_filter_e filt_mode) const
{
    obm.create_RGBA(newsize);
    return resize3(obm.extbody(), body(), info(), filt_mode);
}

template<typename CORE> bool bitmap_t<CORE>::resize_to(const bmpcore_exbody_s &extbody_, resize_filter_e filt_mode) const
{
    return resize3(extbody_, body(), info(), filt_mode);
}

template<typename CORE> bool bitmap_t<CORE>::resize_from(const bmpcore_exbody_s &extbody_, resize_filter_e filt_mode) const
{
    return resize3(extbody(), extbody_(), extbody_.info(), filt_mode);
}

template<typename CORE> void bitmap_t<CORE>::make_grayscale()
{
    if (info().bytepp() != 4 || info().bitpp != 32) return;
    before_modify();

    uint8 * des=body();
    int desnl=info().pitch-info().sz.x*info().bytepp();
    int desnp=info().bytepp();

    if(info().bytepp()==3)
    {
        for(int y=0;y<info().sz.y;y++,des+=desnl) {
            for(int x=0;x<info().sz.x;x++,des+=desnp)
            {
                uint32 color = *(uint32 *)des;

                int  R = as_byte( color>>16 );
                int  G = as_byte( color>>8 );
                int  B = as_byte( color );

                // 0.3 0.5 0.2
                int X = (R * 350 + G * 450 + B * 200) / 1000;

                uint32 ocolor  = (X) | (X<<8) | (X<<16);


                *(uint16 *)des=(uint16)ocolor;
                *(uint8 *)(des+2)=(uint8)(ocolor>>16);
            }			
        }
    } else
    {
        for(int y=0;y<info().sz.y;y++,des+=desnl) {
            for(int x=0;x<info().sz.x;x++,des+=desnp)
            {
                uint32 color = *(uint32 *)des;

                int  A = as_byte( color>>24 );
                int  R = as_byte( color>>16 );
                int  G = as_byte( color>>8 );
                int  B = as_byte( color );

                // 0.3 0.5 0.2
                int X = (R * 350 + G * 450 + B * 200) / 1000;

                *(uint32 *)des=(X) | (X<<8) | (X<<16) | (A<<24);
            }			
        }
    }
}

#pragma warning (push)
#pragma warning (disable : 4731)

template<typename CORE> void bitmap_t<CORE>::swap_byte(void *target) const
{
    // TODO : move this stuff to asm file

    if (info().bytepp() != 3 && info().bytepp() != 4)
    {
        DEBUG_BREAK(); //ERROR_S(L"Unsupported bitpp to convert");
    }


    const int szx = info().sz.x;
    const int szy = info().sz.y;
    const int bpp = info().bytepp();
    const uint8 *bo = body();

    _asm
    {
        // dest
        mov     edi, target

        // source
        //mov     esi, this
        mov     ecx, szx //[esi + m_Size]
        mov     ebx, bpp // [esi + info().bytepp()]
        mov     eax, szy // [esi + m_Size + 4]
        mul     ecx
        mov     esi, bo//[esi + m_Data]
        mov     ecx, eax

        cmp     ebx, 3
        jz      bpp3

        test    ecx, 3
        jz      skipmicroloop4

microloop4:

        mov     eax, [esi]
        mov     ebx, eax
        and     eax, 0xFF00FF00
        ror     ebx, 16
        add     edi, 4
        and     ebx, 0x00FF00FF
        add     esi, 4
        or      eax, ebx
        mov     [edi - 4], eax

        dec     ecx
        test    ecx, 3
        jnz     microloop4

skipmicroloop4:
        shr     ecx, 2
        jz      end

loop1:
        mov     eax, [esi]
        mov     edx, [esi + 4]

        mov     ebx, eax
        and     eax, 0xFF00FF00
        ror     ebx, 16
        add     edi, 16
        and     ebx, 0x00FF00FF
        or      eax, ebx
        mov     ebx, edx
        and     edx, 0xFF00FF00
        ror     ebx, 16
        add     esi, 16
        and     ebx, 0x00FF00FF
        or      edx, ebx

        mov     [edi - 16], eax
        mov     [edi - 12], edx

        mov     eax, [esi - 8]
        mov     edx, [esi - 4]

        mov     ebx, eax
        and     eax, 0xFF00FF00
        ror     ebx, 16
        and     ebx, 0x00FF00FF
        or      eax, ebx
        mov     ebx, edx
        and     edx, 0xFF00FF00
        ror     ebx, 16
        and     ebx, 0x00FF00FF
        or      edx, ebx

        mov     [edi - 8], eax
        mov     [edi - 4], edx

        dec     ecx
        jnz     loop1

        jmp     end
bpp3:

        push    ecx
        shr     ecx, 2
        jnz     rulezz
        pop     ecx
        jmp     microloop3
rulezz:
        push    ebp
loop2:
        // converts every 4 pixels (3 uint32's)

        mov     eax, [esi]
        mov     edx, [esi + 4]
        mov     ebx, eax
        and     eax, 0x0000FF00     // XXXXG0XX
        ror     ebx, 16
        mov     ebp, ebx
        and     ebx, 0x00FF00FF     // XXR0XXG0
        or      eax, ebx
        mov     ebx, edx
        and     ebx, 0x0000FF00     // XXXXR1XX
        and     ebp, 0x0000FF00     // XXXXR0XX
        shl     ebx, 16
        or      eax, ebx
        mov     ebx, edx            // store B2
        mov     [edi], eax
        and     edx, 0xFF0000FF     // G2XXXXG1
        shr     ebx, 16
        or      edx, ebp
        mov     eax, [esi + 8]
        and     ebx, 0x000000FF
        mov     ebp, eax
        and     eax, 0x00FF0000
        ror     ebp, 16
        or      eax, ebx
        mov     ebx, ebp
        and     ebp, 0x00FF0000     // XXR2XXXX
        or      edx, ebp
        mov     [edi + 4], edx
        and     ebx, 0xFF00FF00
        or      eax, ebx
        mov     [edi + 8], eax

        add     esi, 12
        add     edi, 12

        dec     ecx
        jnz     loop2


        pop     ebp
        pop     ecx
        and     ecx, 3
        jz      end

microloop3:
        mov     eax, [esi]
        mov     ebx, eax
        and     eax, 0xFF00FF00
        ror     ebx, 16
        add     edi, 3
        and     ebx, 0x00FF00FF
        add     esi, 3
        or      eax, ebx
        mov     [edi - 3], eax

        dec     ecx
        jnz     microloop3

end:

    };

}

template<typename CORE> bool bitmap_t<CORE>::load_from_BMPHEADER(const BITMAPINFOHEADER * iH, int buflen)
{
    clear();

	ivec2 sz( iH->biWidth, iH->biHeight );

	switch (iH->biBitCount)
	{
	case 24: create_RGB (sz); break;
	case 32: create_RGBA(sz); break;
	default: return false;
	}

	const uint8 *pixels = ((const uint8*)iH) + iH->biSize;
    const uint8 *epixels = ((const uint8*)iH) + buflen;
    uint8 *tdata = body()+(sz.y-1)*info().pitch;

	int bmppitch = (info().pitch + 3) & ~3;

	for (int y=sz.y; y>0; tdata-=info().pitch, pixels+=bmppitch, y--)
        if ( CHECK((pixels + info().pitch) <= epixels) )
		    memcpy(tdata, pixels, info().pitch);

	return true;
}

template<typename CORE> img_format_e bitmap_t<CORE>::load_from_file(const void * buf, int buflen)
{
	if (buflen < 4) return if_none;

    __try
    {
        img_reader_s reader;
        img_format_e fmt = if_none;
        if (image_read_func r = reader.detect(buf,buflen,fmt))
        {
            switch (reader.bitpp)
            {
            case 8:
                create_grayscale(reader.size);
                if (r(reader,body(),info().pitch)) return fmt;
                break;
            case 15:
                create_15(reader.size);
                if (r(reader, body(), info().pitch)) return fmt;
                break;
            case 16:
                create_16(reader.size);
                if (r(reader, body(), info().pitch)) return fmt;
                break;
            case 24:
                create_RGB(reader.size);
                if (r(reader, body(), info().pitch)) return fmt;
                break;
            case 32:
                create_RGBA(reader.size);
                if (r(reader, body(), info().pitch)) return fmt;
                break;
            }
        }

    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

    clear();
    return if_none;
}
template<typename CORE> img_format_e bitmap_t<CORE>::load_from_file(const buf_c & buf)
{
	return load_from_file( buf.data(), buf.size() );
}
template<typename CORE> img_format_e bitmap_t<CORE>::load_from_file(const wsptr &filename)
{
	if (blob_c b = g_fileop->load(filename))
		return load_from_file( b.data(), b.size() );
	return if_none;
}

template<typename CORE> void bitmap_t<CORE>::render_cursor( const ivec2&pos, buf_c &cache )
{
    struct data_s
    {
        HICON iconcached;
        ts::ivec2 sz, hotspot;
    };

    data_s *d = nullptr;
    if (cache.size() >= sizeof(data_s))
        d = (data_s *)cache.data();

    CURSORINFO ci = { sizeof(CURSORINFO) };

    auto preparedata = [&](HICON icn, HBITMAP hmask, HBITMAP hcolor)
    {
        BITMAP bm;
        
        if (0 == GetObject(hmask, sizeof(BITMAP), &bm))
            return false;

        ts::ivec2 sz( bm.bmWidth, bm.bmHeight );
        if (!hcolor) sz.y /= 2;

        drawable_bitmap_c dbmp;
        dbmp.create(ivec2(sz.x, sz.y * 2));
        dbmp.fill(ivec2(0), sz, ts::ARGB(0, 0, 0));
        dbmp.fill(ivec2(0, sz.y), sz, ts::ARGB(255, 255, 255));
        DrawIconEx(dbmp.DC(), 0, 0, icn, sz.x, sz.y, 0, nullptr, DI_NORMAL);
        DrawIconEx(dbmp.DC(), 0, sz.y, icn, sz.x, sz.y, 0, nullptr, DI_NORMAL);

        dbmp.fill_alpha(255);
        //dbmp.save_as_png(L"bmp.png");

        cache.set_size(sizeof(data_s) + (4 * sz.x * sz.y));
        d = (data_s *)cache.data();
        d->iconcached = ci.hCursor;
        d->sz = sz;

        TSCOLOR *dst = (TSCOLOR *)(d + 1);
        TSCOLOR *src1 = (TSCOLOR *)dbmp.body();
        TSCOLOR *src2 = (TSCOLOR *)( dbmp.body() + dbmp.info().pitch * sz.y );
        int addsrc = (dbmp.info().pitch - sz.x * 4) / 4;

        for(int y = 0; y < sz.y; ++y, src1 += addsrc, src2 += addsrc)
        {
            for(int x=0; x<sz.x; ++x, ++dst, ++src1, ++src2)
            {
                auto detectcolor = [](TSCOLOR c1, TSCOLOR c2)->TSCOLOR
                {
                    if (c1 == c2) return c1;
                    if ( c1 == 0xFF000000u && c2 == 0xFFFFFFFFu ) return 0;
                    if ( c2 == 0xFF000000u && c1 == 0xFFFFFFFFu) return 0x00FFFFFFu; // inversion!

                    // detect premultiplied color

                    int a0 = 255 - RED(c2) + RED(c1);
                    int a1 = 255 - GREEN(c2) + GREEN(c1);
                    int a2 = 255 - BLUE(c2) + BLUE(c1);
                    int a = tmax(a0, a1, a2);

                    return (c1 & 0x00FFFFFFu) | (CLAMP<uint8>(a) << 24);
                };

                *dst = detectcolor(*src1, *src2);
            }
        }
        return true;
    };

    ts::ivec2 drawpos;
    if (GetCursorInfo(&ci) && ci.flags == CURSOR_SHOWING)
    {
        if ( d == nullptr || d->iconcached != ci.hCursor )
        {
            HICON hicon = CopyIcon(ci.hCursor);
            ICONINFO ii;
            if (GetIconInfo(hicon, &ii))
            {
                if (!preparedata(hicon, ii.hbmMask, ii.hbmColor))
                {
                    if (nullptr == d)
                    {
                        if (cache.size() < sizeof(data_s))
                            cache.set_size(sizeof(data_s));
                        d = (data_s *)cache.data();
                        d->iconcached = ci.hCursor;
                    }
                    d->sz = ivec2(0); // no render
                }
                d->hotspot.x = ii.xHotspot;
                d->hotspot.y = ii.yHotspot;

                DeleteObject(ii.hbmColor);
                DeleteObject(ii.hbmMask);
            }
            DestroyIcon(hicon);
        }

        drawpos = ts::ivec2(ci.ptScreenPos.x - d->hotspot.x - pos.x, ci.ptScreenPos.y - d->hotspot.y - pos.y);
    } else
    {
        cache.clear();
        return;
    }

    if ( d == nullptr || 0 == d->sz.x )
        return;

    int yy = d->sz.y;
    if (drawpos.y + yy < 0 || drawpos.y >= info().sz.y) return; // full over top / full below bottom

    int xx = d->sz.x;
    if (drawpos.x + xx < 0 || drawpos.x >= info().sz.x) return;

    byte *my_body = body( drawpos );
    const byte *src_color = (const byte *)(d + 1);
        
    if ( drawpos.y < 0 )
    {
        my_body -= info().pitch * drawpos.y;
        src_color -= d->sz.x * 4 * drawpos.y;
        drawpos.y = 0;
    }
    if ( (info().sz.y - drawpos.y) < yy ) yy = (info().sz.y - drawpos.y);

    for (int y = 0; y < yy; ++y, my_body += info().pitch)
    {
        TSCOLOR *rslt = (TSCOLOR *)my_body;

        for (int x = 0; x < xx; ++x, ++rslt, src_color += 4)
        {
            int drawposx = drawpos.x + x;
            if (drawposx < 0 || drawposx >= info().sz.x) continue;

            TSCOLOR c = *(TSCOLOR *)src_color;
            if ( ALPHA(c) )
                *rslt = ALPHABLEND_PM( *rslt, c );
            else if (0x00FFFFFFu == c)
                *rslt = ts::ARGB<auint>( 255-RED(*rslt), 255-GREEN(*rslt), 255-BLUE(*rslt), ALPHA(*rslt) );
        }
    }
}


template<typename CORE> bool bitmap_t<CORE>::load_from_HBITMAP(HBITMAP hbmp)
{
    BITMAP bm;
    if (0 == GetObject(hbmp, sizeof(BITMAP), &bm)) return false;

    struct {
        struct {
            BITMAPV4HEADER bmiHeader;
        } bmi;
        uint32 pal[256];
    } b;

    HDC dc = GetDC(0);
    memset(&b, 0, sizeof(BITMAPV4HEADER));
    b.bmi.bmiHeader.bV4Size = sizeof(BITMAPINFOHEADER);
    b.bmi.bmiHeader.bV4Width = bm.bmWidth;
    b.bmi.bmiHeader.bV4Height = -bm.bmHeight;
    b.bmi.bmiHeader.bV4Planes = 1;

    if (1 == bm.bmBitsPixel)
    {
        b.bmi.bmiHeader.bV4BitCount = 1;
        b.bmi.bmiHeader.bV4SizeImage = bm.bmWidthBytes * bm.bmHeight;
        byte *data = (byte *)_alloca(bm.bmWidthBytes * bm.bmHeight);
        bool ok = GetDIBits(dc, hbmp, 0, bm.bmHeight, data, (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS) != 0;
        if (ok)
        {
            create_grayscale(ivec2(bm.bmWidth, bm.bmHeight));

            byte *my_body = body();
            for (int y = 0; y < bm.bmHeight; ++y, data += bm.bmWidthBytes, my_body += info().pitch)
            {
                const byte *d2 = (const byte *)data;
                byte *bb = my_body;
                for (int x = 0; x < bm.bmWidth; x += 8, ++d2)
                {
                    ts::uint8 bits = *d2;
                    for( int m = 128, xx = x; m>0; m >>=1, ++xx, ++bb )
                        *bb = (bits & m) ? 255 : 0;
                }
            }

        }

        ReleaseDC(0, dc);
        return ok;
    }




    return true;
}

template<typename CORE> void bitmap_t<CORE>::load_from_HWND(HWND hwnd)
{
    RECT r;
    GetClientRect( hwnd, &r );
    ivec2 sz(r.right-r.left, r.bottom-r.top);

    int lPitch=((4*sz.x-1)/4+1)*4;

    BITMAPINFO bi;
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = sz.x;
    bi.bmiHeader.biHeight = -sz.y;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = 0;
    bi.bmiHeader.biSizeImage = lPitch * sz.y;
    bi.bmiHeader.biXPelsPerMeter = 0;
    bi.bmiHeader.biYPelsPerMeter = 0;
    bi.bmiHeader.biClrImportant = 0;
    bi.bmiHeader.biClrUsed = 0;

    HDC dc = GetDC( hwnd );
    
    uint8 *bits;
    HDC memDC = CreateCompatibleDC(dc);
    HBITMAP memBM = CreateDIBSection(dc,&bi,DIB_RGB_COLORS,(void **)&bits,0,0);
    ReleaseDC(hwnd, dc);

    if (memBM == nullptr)
    {
        return;
    }

    HBITMAP tempBM = (HBITMAP)SelectObject(memDC,memBM);

    BitBlt(memDC,0,0,info().sz.x,info().sz.y,dc,0,0,SRCCOPY);

    if (info().sz != sz || info().bytepp() != 4) create_RGBA( sz );

    img_helper_copy( body(), bits, info(), imgdesc_s( sz, 32, (uint16)lPitch ) );

    SelectObject(memDC,tempBM);
    DeleteObject(memBM);
    DeleteDC(memDC);
}

#ifdef _DEBUG
int dbmpcnt = 0;
#endif // _DEBUG

void drawable_bitmap_c::clear()
{
    if (m_mem_dc)
    {
#ifdef _DEBUG
        --dbmpcnt;
        DMSG("dbmp " << dbmpcnt);
#endif // _DEBUG
        DeleteDC(m_mem_dc); m_mem_dc = nullptr;
    }
    if (m_mem_bitmap) { DeleteObject(m_mem_bitmap); m_mem_bitmap = nullptr; }
    core.m_body = nullptr;
    memset(&core.m_info, 0, sizeof(imgdesc_s));
}


void    drawable_bitmap_c::create(const ivec2 &sz, int monitor)
{
    clear();

#ifdef _DEBUG
    ++dbmpcnt;
    DMSG("dbmp " << dbmpcnt);
    //if (dbmpcnt > 300)
    //    __debugbreak();
#endif // _DEBUG


    core.m_info.sz = sz;
    core.m_info.bitpp = 32;

    DEVMODEW devmode;
    devmode.dmSize = sizeof(DEVMODE);

    if (monitor < 0)
        EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &devmode);
    else
    {
        struct mdata
        {
            DEVMODEW *devm;
            int mi;
            static BOOL CALLBACK calcmc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
            {
                mdata *m = (mdata *)dwData;
                if (m->mi == 0)
                {
                    MONITORINFOEXW minf;
                    minf.cbSize = sizeof(MONITORINFOEXW);
                    GetMonitorInfo(hMonitor, &minf);
                    EnumDisplaySettingsW(minf.szDevice, ENUM_CURRENT_SETTINGS, m->devm);
                    return FALSE;
                }
                --m->mi;
                return TRUE;
            }
        } mm; mm.mi = monitor; mm.devm = &devmode;

        EnumDisplayMonitors(nullptr, nullptr, mdata::calcmc, (LPARAM)&mm);

    }

    devmode.dmBitsPerPel = 32;
    devmode.dmPelsWidth = sz.x;
    devmode.dmPelsHeight = sz.y;

    HDC tdc = CreateDCW(L"DISPLAY", nullptr, nullptr, &devmode);

    m_mem_dc = CreateCompatibleDC(tdc);
    if (m_mem_dc == 0)
        DEBUG_BREAK();;

    //m_memBitmap = CreateCompatibleBitmap(tdc, w, h);

    BITMAPV4HEADER bmi;

    int ll = sz.x * 4;
    core.m_info.pitch = (ll + 3) & (~3);

    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bV4Size = sizeof(bmi);
    bmi.bV4Width = sz.x;
    bmi.bV4Height = -sz.y;
    bmi.bV4Planes = 1;
    bmi.bV4BitCount = 32;
    bmi.bV4V4Compression = BI_RGB;
    bmi.bV4SizeImage = 0;

    {
#ifndef _FINAL
        disable_fp_exceptions_c __x;
#endif
        m_mem_bitmap = CreateDIBSection(tdc, (BITMAPINFO *)&bmi, DIB_RGB_COLORS, (void **)&core.m_body, 0, 0);

    }
    ASSERT(m_mem_bitmap);
    CHECK(SelectObject(m_mem_dc, m_mem_bitmap));
    DeleteDC(tdc);
}

bool    drawable_bitmap_c::is_alphablend(const irect &r) const
{
    if (!m_mem_dc || !m_mem_bitmap) return false;

    struct {
        struct {
            BITMAPV4HEADER bmiHeader;
        } bmi;
        uint32 pal[256];
    } b;

    memset(&b, 0, sizeof(BITMAPV4HEADER));
    b.bmi.bmiHeader.bV4Size = sizeof(BITMAPINFOHEADER);
    if (GetDIBits(m_mem_dc, m_mem_bitmap, 0, 0, nullptr, (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS) == 0) return false;

    if (b.bmi.bmiHeader.bV4BitCount == 32 && core.m_body)
    {
        const uint8 * src = core() + (info().pitch*r.lt.y); //(BYTE *)(m_pDIBits)+info().pitch*uint32(b.bmi.bmiHeader.bV4Height - 1 - r.lt.y);
        src += r.lt.x * 4;

        int hh = tmin(r.height(), b.bmi.bmiHeader.bV4Height - r.lt.y);
        int ll = 4 * tmin(r.width(), b.bmi.bmiHeader.bV4Width - r.lt.x);
        for (int y = 0; y < hh; y++)
        {
            for (int i = 0; i < ll; i += 4)
            {
                uint32 s = *(uint32 *)(src + i);
                if ((s & 0xFF000000) != 0xFF000000) return true;
            }
            src += info().pitch;
        }
    }

    return false;
}

bool    drawable_bitmap_c::create_from_bitmap(const bitmap_c &bmp, bool flipy, bool premultiply, bool detect_alpha_pixels)
{
    return create_from_bitmap(bmp, ivec2(0, 0), bmp.info().sz, flipy, premultiply, detect_alpha_pixels);
}

bool drawable_bitmap_c::create_from_bitmap(const bitmap_c &bmp, const ivec2 &p, const ivec2 &sz, bool flipy, bool premultiply, bool detect_alpha_pixels)
{
    clear();

#ifdef _DEBUG
    ++dbmpcnt;
    DMSG("dbmp " << dbmpcnt);
#endif // _DEBUG


    BITMAPV4HEADER bmi;

    ASSERT((p.x + sz.x) <= bmp.info().sz.x && (p.y + sz.y) <= bmp.info().sz.y);

    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bV4Size = sizeof(bmi);
    bmi.bV4Width = sz.x;
    bmi.bV4Height = flipy ? sz.y : -sz.y;
    bmi.bV4Planes = 1;
    bmi.bV4BitCount = bmp.info().bitpp;
    if (bmp.info().bytepp() >= 3)
    {
        bmi.bV4V4Compression = BI_RGB;
        bmi.bV4SizeImage = 0;
        bmi.bV4AlphaMask = 0xFF000000;
    }
    else
    {
        bmi.bV4V4Compression = BI_BITFIELDS;
        bmi.bV4RedMask = 0x0000f800;
        bmi.bV4GreenMask = 0x000007e0;
        bmi.bV4BlueMask = 0x0000001f;
        bmi.bV4AlphaMask = 0;
        bmi.bV4SizeImage = sz.x * sz.y * bmp.info().bytepp();
    }

    //HDC tdc=GetDC(0);

    DEVMODEW devmode;
    devmode.dmSize = sizeof(DEVMODE);

    EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &devmode);

    devmode.dmBitsPerPel = bmp.info().bitpp;
    devmode.dmPelsWidth = sz.x;
    devmode.dmPelsHeight = sz.y;

    HDC tdc = CreateDCW(L"DISPLAY", nullptr, nullptr, &devmode);

    {
#ifndef _FINAL
        disable_fp_exceptions_c __x;
#endif

        m_mem_bitmap = CreateDIBSection(tdc, (BITMAPINFO *)&bmi, DIB_RGB_COLORS, (void **)&core.m_body, 0, 0);
    }

    bool alphablend = false;

    if (m_mem_bitmap == nullptr) DEBUG_BREAK();;

    //if((bmp->info().bytepp()==3 || bmp->info().bytepp()==4)) bmp->swap_byte(ivec2(0,0),bmp->size(),0,2);

    core.m_info.sz = sz;
    core.m_info.bitpp = bmp.info().bitpp;

    int ll = sz.x * bmp.info().bytepp();
    core.m_info.pitch = (ll + 3) & (~3);
    uint8 * bdes = core(); //+lls*uint32(sz.y-1);
    const uint8 * bsou = bmp.body() + p.x * bmp.info().bytepp() + bmp.info().pitch * p.y;

    if (premultiply && bmp.info().bytepp() == 4)
    {
        for (int y = 0; y < sz.y; ++y)
        {
            for (int x = 0; x < sz.x; ++x)
            {
                uint32 ocolor;
                uint32 color = ((uint32 *)bsou)[x];
                uint8 alpha = uint8(color >> 24);
                if (alpha == 255)
                {
                    ocolor = color;
                }
                else if (alpha == 0)
                {
                    ocolor = 0;
                    alphablend = true;
                }
                else
                {

                    uint8 R = as_byte(color >> 16);
                    uint8 G = as_byte(color >> 8);
                    uint8 B = as_byte(color);

                    float A = float(alpha) / 255.0f;

#define CCGOOD( C ) CLAMP<uint8>( float(C) * A )

                    ocolor = CCGOOD(B) | (CCGOOD(G) << 8) | (CCGOOD(R) << 16) | (alpha << 24);
#undef CCGOOD

                    alphablend = true;
                }

                ((uint32 *)bdes)[x] = ocolor;
            }

            bsou = bsou + bmp.info().pitch;
            bdes = bdes + info().pitch;

        }

    }
    else if (detect_alpha_pixels && bmp.info().bytepp() == 4)
    {
        for (int y = 0; y < sz.y; ++y)
        {
            if (alphablend)
            {
                if (info().pitch == bmp.info().pitch && ll == info().pitch)
                {
                    memcpy(bdes, bsou, ll * (sz.y - y));
                }
                else
                {
                    for (; y < sz.y; ++y)
                    {
                        memcpy(bdes, bsou, ll);
                        bsou = bsou + bmp.info().pitch;
                        bdes = bdes + info().pitch;
                    }
                }

                break;
            }

            for (aint i = 0; i < ll; i += 4)
            {
                uint32 color = *(uint32 *)(bsou + i);
                *(uint32 *)(bdes + i) = color;
                uint8 alpha = uint8(color >> 24);
                if (alpha < 255)
                    alphablend = true;
            }

            bsou = bsou + bmp.info().pitch;
            bdes = bdes + info().pitch;
        }

    }
    else
    {
        if (info().pitch == bmp.info().pitch && ll == info().pitch)
        {
            memcpy(bdes, bsou, ll * sz.y);
        }
        else
        {
            img_helper_copy(bdes, bsou, info(), bmp.info());
        }
    }

    //if((bmp->info().bytepp()==3 || bmp->info().bytepp()==4)) bmp->swap_byte(ivec2(0,0),bmp->size(),0,2);

    m_mem_dc = CreateCompatibleDC(tdc);
    if (m_mem_dc == nullptr) DEBUG_BREAK();;
    if (SelectObject(m_mem_dc, m_mem_bitmap) == nullptr) DEBUG_BREAK();;

    //ReleaseDC(0,tdc);
    DeleteDC(tdc);

    return alphablend;
}

void    drawable_bitmap_c::save_to_bitmap(bitmap_c &bmp, const ivec2 & pos_from_dc)
{
    if (!m_mem_dc || !m_mem_bitmap) return;

    struct {
        struct {
            BITMAPV4HEADER bmiHeader;
        } bmi;
        uint32 pal[256];
    } b;

    memset(&b, 0, sizeof(BITMAPV4HEADER));
    b.bmi.bmiHeader.bV4Size = sizeof(BITMAPINFOHEADER);
    if (GetDIBits(m_mem_dc, m_mem_bitmap, 0, 0, nullptr, (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS) == 0) return;

    if (b.bmi.bmiHeader.bV4BitCount == 32 && core())
    {
        const uint8 * src = core()+info().pitch* (b.bmi.bmiHeader.bV4Height - 1 - pos_from_dc.y);
        src += pos_from_dc.x * 4;
        //bmp->create_RGBA(b.bmi.bmiHeader.bV4Width,b.bmi.bmiHeader.bV4Height);
        uint8 * dst = bmp.body();
        int hh = tmin(bmp.info().sz.y, b.bmi.bmiHeader.bV4Height - pos_from_dc.y);
        auint ll = sizeof(uint32) * tmin(bmp.info().sz.x, b.bmi.bmiHeader.bV4Width - pos_from_dc.x);
        for (int y = 0; y < hh; y++)
        {
            //memcpy(dst,src,ll);

            for (uint i = 0; i < ll; i += 4)
            {
                uint32 s = *(uint32 *)(src + i);
                s |= 0xFF000000;
                *(uint32 *)(dst + i) = s;
            }


            src -= info().pitch;
            dst += bmp.info().pitch;
        }
    }

}

void drawable_bitmap_c::save_to_bitmap(bitmap_c &bmp, bool save16as32)
{
    if (!m_mem_dc || !m_mem_bitmap) return;


    struct {
        struct {
            BITMAPV4HEADER bmiHeader;
        } bmi;
        uint32 pal[256];
    } b;

    memset(&b, 0, sizeof(BITMAPV4HEADER));
    b.bmi.bmiHeader.bV4Size = sizeof(BITMAPINFOHEADER);
    if (GetDIBits(m_mem_dc, m_mem_bitmap, 0, 0, nullptr, (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS) == 0) return;

    if (b.bmi.bmiHeader.bV4BitCount == 32 && core())
    {
        uint32 ll = uint32(b.bmi.bmiHeader.bV4Width * 4);
        const uint8 * src = core()+info().pitch*uint32(b.bmi.bmiHeader.bV4Height - 1);
        bmp.create_RGBA(ref_cast<ivec2>(b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height));
        uint8 * dst = bmp.body();
        for (int y = 0; y < bmp.info().sz.y; y++)
        {
            memcpy(dst, src, ll);
            src -= info().pitch;
            dst += bmp.info().pitch;
        }
        return;
    }



    if (b.bmi.bmiHeader.bV4BitCount != 16 && b.bmi.bmiHeader.bV4BitCount != 24 && b.bmi.bmiHeader.bV4BitCount != 32) return;

    if (save16as32 && b.bmi.bmiHeader.bV4BitCount == 16)
    {
        b.bmi.bmiHeader.bV4BitCount = 32;
    }

    if (b.bmi.bmiHeader.bV4BitCount == 16) bmp.create_16(ref_cast<ivec2>(b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height));
    else if (b.bmi.bmiHeader.bV4BitCount == 24) bmp.create_RGB(ref_cast<ivec2>(b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height));
    else if (b.bmi.bmiHeader.bV4BitCount == 32) bmp.create_RGBA(ref_cast<ivec2>(b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height));
    bmp.fill(0);

    if (GetDIBits(m_mem_dc, m_mem_bitmap, 0, b.bmi.bmiHeader.bV4Height, bmp.body(), (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS) == 0) return;

    bmp.fill_alpha(255);

    //bmp->flip_y();
    //if(b.bmi.bmiHeader.bV4BitCount==24 || b.bmi.bmiHeader.bV4BitCount==32) bmp->swap_byte(ivec2(0,0),bmp->size(),0,2);

}

void drawable_bitmap_c::draw(HDC dc, aint xx, aint yy, int alpha) const
{
    struct {
        struct {
            BITMAPV4HEADER bmiHeader;
        } bmi;
        uint32 pal[256];
    } b;

    memset(&b, 0, sizeof(BITMAPV4HEADER));
    b.bmi.bmiHeader.bV4Size = sizeof(BITMAPINFOHEADER);
    if (GetDIBits(m_mem_dc, m_mem_bitmap, 0, 0, nullptr, (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS) == 0) return;
    if (alpha > 0)
    {
        BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, (uint8)alpha, AC_SRC_ALPHA };
        AlphaBlend(dc, xx, yy, b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height, m_mem_dc, 0, 0, b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height, blendPixelFunction);
    } else if (alpha < 0)
    {
        BitBlt(dc, xx, yy, b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height, m_mem_dc, 0, 0, SRCCOPY);
    }
}

void drawable_bitmap_c::draw(HDC dc, aint xx, aint yy, const irect &r, int alpha) const
{
    if (alpha > 0)
    {
        BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, (uint8)alpha, AC_SRC_ALPHA };
        AlphaBlend(dc, xx, yy, r.width(), r.height(), m_mem_dc, r.lt.x, r.lt.y, r.width(), r.height(), blendPixelFunction);
    } else if (alpha < 0)
    {
        BitBlt(dc, xx, yy, r.width(), r.height(), m_mem_dc, r.lt.x, r.lt.y, SRCCOPY);
    }
}


template class bitmap_t<bmpcore_normal_s>;
template class bitmap_t<bmpcore_exbody_s>;




} // namespace ts

