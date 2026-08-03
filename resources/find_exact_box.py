from PIL import Image

img = Image.open(r'C:\Users\Jun\.gemini\antigravity\brain\77905a4f-9187-437b-99ef-68f1e0179c1e\lightcell_icon_glow_1785135114071.jpg')
w, h = img.size

def is_icon(color):
    r, g, b = color[:3]
    return (g - r > 12) or (g - b > 12) or (g > 100)

left = next(x for x in range(w) if is_icon(img.getpixel((x, h//2))))
right = next(x for x in range(w-1, -1, -1) if is_icon(img.getpixel((x, h//2))))
top = next(y for y in range(h) if is_icon(img.getpixel((w//2, y))))
bottom = next(y for y in range(h-1, -1, -1) if is_icon(img.getpixel((w//2, y))))

print(f"Detected Icon Bounds: X({left} ~ {right}), Y({top} ~ {bottom})")
size_x = right - left
size_y = bottom - top
print(f"Size X: {size_x}, Size Y: {size_y}")

# Make a square box centered around the midpoint
mid_x = (left + right) // 2
mid_y = (top + bottom) // 2
half_size = max(size_x, size_y) // 2

sq_left = mid_x - half_size
sq_right = mid_x + half_size
sq_top = mid_y - half_size
sq_bottom = mid_y + half_size

print(f"Square Box: ({sq_left}, {sq_top}, {sq_right}, {sq_bottom})")
