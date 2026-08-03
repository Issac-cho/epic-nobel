from PIL import Image, ImageDraw

img = Image.open(r'C:\Users\Jun\.gemini\antigravity\brain\77905a4f-9187-437b-99ef-68f1e0179c1e\lightcell_icon_glow_1785135114071.jpg').convert('RGBA')

# Find exact center boundaries where G-R > 20 along central axes
w, h = img.size
top = next(y for y in range(h) if img.getpixel((w//2, y))[1] - img.getpixel((w//2, y))[0] > 20)
bottom = next(y for y in range(h-1, -1, -1) if img.getpixel((w//2, y))[1] - img.getpixel((w//2, y))[0] > 20)
left = next(x for x in range(w) if img.getpixel((x, h//2))[1] - img.getpixel((x, h//2))[0] > 20)
right = next(x for x in range(w-1, -1, -1) if img.getpixel((x, h//2))[1] - img.getpixel((x, h//2))[0] > 20)

print(f"Exact center edges: X({left} ~ {right}), Y({top} ~ {bottom})")
size = max(right - left, bottom - top) + 10 # Add 10px margin around the rounded square
mid_x = (left + right) // 2
mid_y = (top + bottom) // 2
half = size // 2

box = (mid_x - half, mid_y - half, mid_x + half, mid_y + half)
print("Cropping box:", box)

cropped = img.crop(box)

# Create a smooth rounded rectangle mask to eliminate any outer grey corners from the 3D studio background
mask = Image.new('L', cropped.size, 0)
draw = ImageDraw.Draw(mask)
radius = int(cropped.size[0] * 0.22) # Standard macOS Big Sur / Windows 11 icon corner radius (~22%)
draw.rounded_rectangle((0, 0, cropped.size[0]-1, cropped.size[1]-1), radius=radius, fill=255)

# Apply alpha mask
cropped.putalpha(mask)

cropped.save('resources/lightcell.png')
cropped.save('resources/lightcell.ico', format='ICO', sizes=[(256, 256), (128, 128), (64, 64), (32, 32), (16, 16)])
print("Cropped and saved lightcell.png / lightcell.ico successfully!")
