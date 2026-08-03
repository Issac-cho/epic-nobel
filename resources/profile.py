from PIL import Image

img = Image.open(r'C:\Users\Jun\.gemini\antigravity\brain\77905a4f-9187-437b-99ef-68f1e0179c1e\lightcell_icon_glow_1785135114071.jpg')

print("Horizontal profile across Y=512:")
for x in range(0, 1024, 40):
    print(f"X={x:4d}: {img.getpixel((x, 512))}")
