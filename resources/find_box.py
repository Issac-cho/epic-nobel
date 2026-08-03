from PIL import Image

img = Image.open(r'C:\Users\Jun\.gemini\antigravity\brain\77905a4f-9187-437b-99ef-68f1e0179c1e\lightcell_icon_glow_1785135114071.jpg')
w, h = img.size
bg = img.getpixel((0, 0))

def diff(c1, c2):
    return sum(abs(a - b) for a, b in zip(c1, c2))

left = next(x for x in range(w) if diff(img.getpixel((x, h//2)), bg) > 50)
right = next(x for x in range(w-1, -1, -1) if diff(img.getpixel((x, h//2)), bg) > 50)
top = next(y for y in range(h) if diff(img.getpixel((w//2, y)), bg) > 50)
bottom = next(y for y in range(h-1, -1, -1) if diff(img.getpixel((w//2, y)), bg) > 50)

print(f"Box: ({left}, {top}) to ({right}, {bottom}) | Width: {right-left}, Height: {bottom-top}")
