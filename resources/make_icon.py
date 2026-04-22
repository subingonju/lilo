"""
Generate a bank-style icon (bank.ico) for the AccountManager app.
Sizes: 256, 128, 64, 48, 32, 16
"""
from PIL import Image, ImageDraw
import math

def draw_bank(size):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    s = size
    pad = s * 0.06
    W = s - pad * 2  # usable width
    cx = s / 2

    # Colors
    bg_color    = (29, 78, 216)   # #1D4ED8 blue
    light_color = (255, 255, 255) # white

    # --- Background circle ---
    d.ellipse([pad, pad, s - pad, s - pad], fill=bg_color)

    # --- Roof / pediment (triangle) ---
    roof_top    = s * 0.12
    roof_bot    = s * 0.38
    roof_left   = cx - W * 0.44
    roof_right  = cx + W * 0.44
    d.polygon([cx, roof_top, roof_left, roof_bot, roof_right, roof_bot],
              fill=light_color)

    # --- Entablature (horizontal bar under roof) ---
    ent_top = roof_bot
    ent_bot = s * 0.44
    d.rectangle([cx - W * 0.44, ent_top, cx + W * 0.44, ent_bot], fill=light_color)

    # --- Columns ---
    col_top  = ent_bot
    col_bot  = s * 0.72
    n_cols   = 4
    col_w    = W * 0.09
    gap      = (W * 0.88 - n_cols * col_w) / (n_cols - 1)
    col_x0   = cx - W * 0.44
    for i in range(n_cols):
        x = col_x0 + i * (col_w + gap)
        d.rectangle([x, col_top, x + col_w, col_bot], fill=light_color)

    # --- Base / steps ---
    base1_top = col_bot
    base1_bot = s * 0.78
    d.rectangle([cx - W * 0.44, base1_top, cx + W * 0.44, base1_bot], fill=light_color)

    base2_top = base1_bot
    base2_bot = s * 0.84
    d.rectangle([cx - W * 0.50, base2_top, cx + W * 0.50, base2_bot], fill=light_color)

    return img

sizes = [256, 128, 64, 48, 32, 16]
images = [draw_bank(sz) for sz in sizes]

out = r"C:\veda_Qt_study\project\lilo\resources\bank.ico"
images[0].save(out, format="ICO", sizes=[(s, s) for s in sizes],
               append_images=images[1:])
print(f"Saved {out}")
