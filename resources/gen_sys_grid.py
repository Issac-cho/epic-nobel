import os
from icoextract import IconExtractor
from PIL import Image, ImageDraw, ImageFont

def make_sheet_page(dll_path, out_path, title, start_idx, end_idx):
    e = IconExtractor(dll_path)
    group_icons = e.list_group_icons()
    
    slice_icons = group_icons[start_idx:end_idx]
    if not slice_icons: return
    
    cols = 10
    rows = (len(slice_icons) + cols - 1) // cols
    cell_w = 90
    cell_h = 100
    
    sheet = Image.new('RGB', (cols * cell_w, rows * cell_h + 40), (250, 250, 250))
    draw = ImageDraw.Draw(sheet)
    
    draw.text((10, 10), f"{title} (Items {start_idx}~{end_idx}) - Index labeled below each icon", fill=(0, 0, 0))
    
    for count, item in enumerate(slice_icons):
        abs_idx = start_idx + count
        res_id = item[0]
        id_str = str(res_id).replace("ResourceID(", "").replace(")", "").replace("'", "").replace('"', '')
        
        temp_ico = f"temp_{start_idx}_{count}.ico"
        try:
            # Correct order: export_icon(filename, num=index)
            e.export_icon(temp_ico, num=abs_idx)
            with Image.open(temp_ico) as ico_img:
                ico_img = ico_img.convert('RGBA')
                ico_img.thumbnail((48, 48), Image.Resampling.LANCZOS)
                
                r = count // cols
                c = count % cols
                x = c * cell_w + (cell_w - ico_img.width) // 2
                y = 40 + r * cell_h + 10
                
                sheet.paste(ico_img, (x, y), ico_img)
                draw.text((c * cell_w + 15, y + 55), f"#{abs_idx}", fill=(0, 0, 0))
                draw.text((c * cell_w + 15, y + 70), f"({id_str})", fill=(100, 100, 100))
        except Exception as ex:
            print(f"Failed at {abs_idx}: {ex}")
        finally:
            if os.path.exists(temp_ico):
                try: os.remove(temp_ico)
                except: pass
                
    sheet.save(out_path)
    print("Saved:", out_path)

out_dir = r"C:\Users\Jun\.gemini\antigravity\brain\77905a4f-9187-437b-99ef-68f1e0179c1e"
make_sheet_page(r"C:\WINDOWS\SystemResources\imageres.dll.mun", os.path.join(out_dir, "sys_imageres_p1.png"), "Windows Modern Icons (imageres)", 0, 120)
make_sheet_page(r"C:\WINDOWS\SystemResources\imageres.dll.mun", os.path.join(out_dir, "sys_imageres_p2.png"), "Windows Modern Icons (imageres)", 120, 240)
make_sheet_page(r"C:\WINDOWS\SystemResources\imageres.dll.mun", os.path.join(out_dir, "sys_imageres_p3.png"), "Windows Modern Icons (imageres)", 240, 370)
