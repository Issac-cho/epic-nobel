from PIL import Image
img = Image.open(r'C:\Users\Jun\.gemini\antigravity\brain\77905a4f-9187-437b-99ef-68f1e0179c1e\lightcell_icon_glow_1785135114071.jpg')
for y in range(700, 950, 20):
    print(f"Y={y:3d}: {img.getpixel((512, y))}")
for y in range(100, 250, 20):
    print(f"Top Y={y:3d}: {img.getpixel((512, y))}")
