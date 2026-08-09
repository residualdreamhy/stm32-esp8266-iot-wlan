# -*- coding: utf-8 -*-
"""
生成 OLED 8x16 ASCII 字库 (0x20~0x7E, 共 95 字符)。
取模规则必须与 oled.c 的 OLED_ShowChar 完全一致：
  - 每个字符 16 字节，byte[0..7]=上半部分(8 行)逐列，byte[8..15]=下半部分逐列
  - 每个字节是一列 8 像素，bit7=该列最上方像素，bit0=最下方
关键点：所有字符按同一套“字身框(em box)”缩放，字形大小一致、比例自然、
        占满 16 行；用超采样最大池化保留细笔画，避免裁切/比例失调造成的乱码。
用法：需要 Pillow。python gen_font.py  ->  输出 oled_font.h
"""
import re
from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "C:/Windows/Fonts/cour.ttf"   # 等宽字体，保证每个字符形态稳定
FONTSIZE  = 64                            # 渲染分辨率，越大越清晰
CELL_W, CELL_H = 8, 16                    # OLED 字模尺寸
SS = 4                                    # 超采样系数：下采样时保留细笔画


font = ImageFont.truetype(FONT_PATH, FONTSIZE)

# 等宽字体的“字身框”：所有字符共用同一套缩放比例，保证字形大小一致、比例自然。
# 基线在 y=asc，字形占据 [0, asc+desc) 整框；宽取等宽 advance（取 M 近似，等宽下为常数）。
_ASC, _DESC = font.getmetrics()
_EMH = _ASC + _DESC
_EMW = int(font.getlength("M")) + 1


def render_glyph(ch):
    """把字符渲染成 16x8 的 0/1 点阵(grid[y][x])，y∈[0,16) 行，x∈[0,8) 列。"""
    # 用大画布 + 居中锚点渲染，避免裁切
    canvas = Image.new("L", (FONTSIZE * 2, FONTSIZE * 2), 0)
    d = ImageDraw.Draw(canvas)
    d.text((FONTSIZE, FONTSIZE), ch, fill=255, font=font, anchor="mm")
    bb = canvas.getbbox()
    if bb is None:
        return [[0] * CELL_W for _ in range(CELL_H)]
    g = canvas.crop(bb)
    gw, gh = g.size
    # 尽量占满整列宽度(8px)以提升可读性；若高度溢出则改占满 16 行
    scale = CELL_W / gw
    if gh * scale > CELL_H:
        scale = CELL_H / gh
    nw, nh = max(1, round(gw * scale)), max(1, round(gh * scale))
    g = g.resize((nw, nh), Image.LANCZOS)
    # 放入超采样网格并水平居中
    GW, GH = CELL_W * SS, CELL_H * SS
    big = Image.new("L", (GW, GH), 0)
    big.paste(g, ((GW - nw * SS) // 2, (GH - nh * SS) // 2))
    px = big.load()
    grid = [[0] * CELL_W for _ in range(CELL_H)]
    for y in range(CELL_H):
        for x in range(CELL_W):
            on = any(px[x * SS + sx, y * SS + sy] > 128
                     for sy in range(SS) for sx in range(SS))
            grid[y][x] = 1 if on else 0
    return grid


def grid_to_bytes(grid):
    """转为 16 字节：byte[0..7] 上半、byte[8..15] 下半，bit7=顶。"""
    out = []
    for half in (0, 1):
        for c in range(CELL_W):
            b = 0
            for r in range(8):
                row = half * 8 + r
                if grid[row][c]:
                    b |= (1 << (7 - r))
            out.append(b)
    return out


lines = []
lines.append("/* 本文件由 gen_font.py 自动生成：Courier New 渲染的 8x16 ASCII 字库 (32~126) */")
lines.append("#ifndef __OLED_FONT_H")
lines.append("#define __OLED_FONT_H")
lines.append("")
lines.append("/* OLED_F8x16[ch - ' '][16]：列优先，byte[0..7]上半、byte[8..15]下半，bit7 为顶 */")
lines.append("const unsigned char OLED_F8x16[95][16] = {")

for code in range(0x20, 0x7F):
    ch = chr(code)
    grid = render_glyph(ch)
    bs = grid_to_bytes(grid)
    hexs = ", ".join("0x%02X" % b for b in bs)
    disp = ch if ch.isprintable() and ch != "'" else ("\\'" if ch == "'" else " ")
    lines.append("    { %s }, /* %d '%s' */" % (hexs, code, disp))

lines.append("};")
lines.append("")
lines.append("#endif")
lines.append("")

with open("oled_font.h", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))

print("oled_font.h 生成完成，共 %d 个字符" % (0x7F - 0x20))
