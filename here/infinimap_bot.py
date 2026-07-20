import os
import time
import random
import base64
from playwright.sync_api import sync_playwright
from PIL import Image

# ==========================================
#               CONFIGURATION
# ==========================================
NUM_TILES_TO_GENERATE = 10   
TILE_WIDTH = 512             
TILE_HEIGHT = 512
OVERLAP_WIDTH = 128          

WORKSPACE_DIR = "map_workspace"
MASTER_MAP_FILE = f"{WORKSPACE_DIR}/master_map.png"

PROMPT_START = "Close-up side-scrolling cross-section, Terraria style pixel art. A small wooden house on a dirt surface. Chunky pixels, sharp edges, 16-bit colors."
PROMPT_CONTINUE = "Close-up side-scrolling cross-section, Terraria style pixel art. Perfectly continue the landscape from the left edge into the empty space. Chunky pixels, sharp colors."

if not os.path.exists(WORKSPACE_DIR):
    os.makedirs(WORKSPACE_DIR)

# ==========================================
#             IMAGE PROCESSING
# ==========================================
def make_crisp_pixel_art(image_path):
    img = Image.open(image_path).convert("RGBA")
    if img.width != TILE_WIDTH:
        img = img.resize((TILE_WIDTH, TILE_HEIGHT), Image.Resampling.LANCZOS)
    small = img.resize((img.width // 4, img.height // 4), Image.Resampling.BILINEAR)
    crisp = small.resize((img.width, img.height), Image.Resampling.NEAREST)
    crisp.save(image_path)

def prepare_next_input(previous_tile_path, next_input_path):
    prev_img = Image.open(previous_tile_path).convert("RGBA")
    edge = prev_img.crop((TILE_WIDTH - OVERLAP_WIDTH, 0, TILE_WIDTH, TILE_HEIGHT))
    new_canvas = Image.new("RGBA", (TILE_WIDTH, TILE_HEIGHT), (0, 0, 0, 0))
    new_canvas.paste(edge, (0, 0))
    new_canvas.save(next_input_path)

def stitch_to_master(master_path, new_tile_path, step):
    make_crisp_pixel_art(new_tile_path)
    new_tile = Image.open(new_tile_path).convert("RGBA")
    if step == 1:
        new_tile.save(master_path)
        return
    master = Image.open(master_path).convert("RGBA")
    new_width = master.width + TILE_WIDTH - OVERLAP_WIDTH
    combined = Image.new("RGBA", (new_width, TILE_HEIGHT))
    combined.paste(master, (0, 0))
    combined.paste(new_tile, (master.width - OVERLAP_WIDTH, 0))
    combined.save(master_path)

# ==========================================
#               UI AUTOMATION
# ==========================================
def find_ai_studio_tab(context):
    for page in context.pages:
        if "aistudio.google.com" in page.url:
            return page
    return None

def get_prompt_box(page):
    selectors = ['div[contenteditable="true"]', 'textarea[placeholder*="Type"]', '.prompt-text-area', 'textarea']
    for s in selectors:
        try:
            el = page.wait_for_selector(s, timeout=3000)
            if el and el.is_visible(): return page.locator(s).first
        except: continue
    return None

def clear_ui_safely(page):
    print("   Clearing previous inputs...")
    box = get_prompt_box(page)
    if box:
        box.focus()
        page.keyboard.press("Control+A")
        page.keyboard.press("Backspace")
        page.wait_for_timeout(500)
        
    # Find any 'X' button on the uploaded image and delete it
    remove_selectors = [
        '[aria-label*="Remove" i]', '[aria-label*="Delete" i]', '[title*="Remove" i]',
        '[mat-tooltip*="Remove" i]', 'mat-icon:has-text("close")', 'mat-icon:has-text("cancel")'
    ]
    for sel in remove_selectors:
        elements = page.locator(sel)
        for i in range(elements.count()):
            el = elements.nth(i)
            if el.is_visible():
                try:
                    el.click(timeout=1000)
                    page.wait_for_timeout(300)
                except: pass

def upload_edge_reference(page, input_img):
    print("   Uploading edge reference...")
    
    # 1. Direct Hidden Input Check
    try:
        inputs = page.locator('input[type="file"]')
        for i in range(inputs.count()):
            try:
                inputs.nth(i).set_input_files(input_img, timeout=500)
                print("   -> Uploaded via hidden input.")
                return
            except: pass
    except: pass

    print("   -> Hunting for the Upload/Add button...")
    
    # 2. Aggressive 2-Step Menu Navigation
    potential_buttons = [
        '[aria-label*="add " i]', '[aria-label*="upload" i]', '[aria-label*="image" i]', 
        '[mat-tooltip*="add" i]', '[mat-tooltip*="image" i]',
        'button:has(svg)', 'div[role="button"]:has(svg)', 'span[role="button"]:has(svg)',
        'button:has(.material-symbols-outlined)', 'button:has(.google-symbols)'
    ]
    
    for selector in potential_buttons:
        elements = page.locator(selector)
        for i in range(elements.count()):
            el = elements.nth(i)
            # Ignore invisible elements or the prompt box itself
            if not el.is_visible() or el.get_attribute("contenteditable") == "true":
                continue
                
            try:
                with page.expect_file_chooser(timeout=800) as fc_info:
                    el.click(force=True)
                fc_info.value.set_files(input_img)
                print("   -> Uploaded via direct button click.")
                return
            except:
                # Button clicked, check if it opened a dropdown menu for "Upload Image"
                menu_opts = page.locator('text=/(Upload|Image|Media|File|Computer|Drive)/i')
                for j in range(menu_opts.count()):
                    opt = menu_opts.nth(j)
                    if opt.is_visible() and opt.get_attribute("contenteditable") != "true":
                        try:
                            with page.expect_file_chooser(timeout=800) as fc_info2:
                                opt.click(force=True)
                            fc_info2.value.set_files(input_img)
                            print("   -> Uploaded via dropdown menu.")
                            return
                        except: pass
                
                # Close the menu if we clicked the wrong button
                page.keyboard.press("Escape")
                page.wait_for_timeout(100)

    # 3. The Synthetic Drag & Drop Bypass
    print("   -> Click methods failed. Initiating Javascript Drag & Drop Bypass...")
    box = get_prompt_box(page)
    if box:
        with open(input_img, "rb") as f:
            b64_data = base64.b64encode(f.read()).decode("utf-8")
        filename = os.path.basename(input_img)
        
        js = """
        ([el, data]) => {
            const byteString = atob(data.b64);
            const ab = new ArrayBuffer(byteString.length);
            const ia = new Uint8Array(ab);
            for (let i = 0; i < byteString.length; i++) {
                ia[i] = byteString.charCodeAt(i);
            }
            const file = new File([ia], data.filename, { type: 'image/png' });
            const dt = new DataTransfer();
            dt.items.add(file);
            
            el.dispatchEvent(new DragEvent('dragenter', { bubbles: true, cancelable: true, dataTransfer: dt }));
            el.dispatchEvent(new DragEvent('dragover', { bubbles: true, cancelable: true, dataTransfer: dt }));
            el.dispatchEvent(new DragEvent('drop', { bubbles: true, cancelable: true, dataTransfer: dt }));
        }
        """
        box.evaluate(js, {"b64": b64_data, "filename": filename})
        print("   -> Uploaded via synthetic Drag & Drop!")
        return

    raise Exception("Fatal Error: Could not upload using Input, Click, or Drag-and-Drop.")

def run_and_wait(page, output_path):
    print("   Waiting for file to sync with Google Cloud...")
    page.wait_for_timeout(5000) 
    
    old_img_src = None
    existing_images = page.locator('img[alt*="Generated"]')
    if existing_images.count() > 0:
        old_img_src = existing_images.last.get_attribute("src")

    print("   Submitting via Web Accessibility Bypass (Keyboard Enter)...")
    run_btn = page.locator('button:has-text("Run"), .run-button, button.primary').first
    
    if run_btn.is_visible():
        run_btn.focus()
        page.wait_for_timeout(500)
        page.keyboard.press("Enter")
    else:
        page.keyboard.press("Control+Enter")

    print("   Waiting for AI to finish drawing (~15-30s)...")
    for _ in range(60): 
        images = page.locator('img[alt*="Generated"]')
        if images.count() > 0:
            new_img_src = images.last.get_attribute("src")
            if new_img_src and new_img_src != old_img_src:
                time.sleep(3) 
                images.last.screenshot(path=output_path)
                return
        page.wait_for_timeout(1000)
    print("   ERROR: Timed out waiting for new image generation.")

# ==========================================
#                MAIN LOOP
# ==========================================
def main():
    with sync_playwright() as p:
        print("Connecting to Chrome...")
        try:
            browser = p.chromium.connect_over_cdp("http://127.0.0.1:9222")
            context = browser.contexts[0]
            
            # Anti-Bot Visibility Spoof
            context.add_init_script("""
                Object.defineProperty(document, 'visibilityState', {get: () => 'visible'});
                Object.defineProperty(document, 'hidden', {get: () => false});
                Object.defineProperty(navigator, 'webdriver', {get: () => undefined});
                window.navigator.chrome = { runtime: {} };
            """)
            
            page = find_ai_studio_tab(context)
            if not page:
                print("Error: AI Studio tab not found.")
                return
            page.bring_to_front() 
        except Exception as e:
            print(f"Connection Error: {e}")
            return

        print("\n[STEP 1] Generating Starting Tile...")
        clear_ui_safely(page)
        
        box = get_prompt_box(page)
        if not box:
            return
        
        box.focus()
        page.wait_for_timeout(500)
        
        for char in PROMPT_START:
            page.keyboard.type(char)
            page.wait_for_timeout(random.randint(20, 60))
        
        current_tile = f"{WORKSPACE_DIR}/tile_1.png"
        run_and_wait(page, current_tile)
        stitch_to_master(MASTER_MAP_FILE, current_tile, 1)

        for i in range(2, NUM_TILES_TO_GENERATE + 1):
            print(f"\n[STEP {i}] Expanding Map...")
            input_img = f"{WORKSPACE_DIR}/input_{i}.png"
            prepare_next_input(current_tile, input_img)
            
            clear_ui_safely(page)
            
            # THE NEW UPLOAD METHOD
            upload_edge_reference(page, input_img)
            
            box = get_prompt_box(page)
            box.focus()
            page.wait_for_timeout(500)
            
            for char in PROMPT_CONTINUE:
                page.keyboard.type(char)
                page.wait_for_timeout(random.randint(20, 60))
            
            current_tile = f"{WORKSPACE_DIR}/tile_{i}.png"
            run_and_wait(page, current_tile)
            stitch_to_master(MASTER_MAP_FILE, current_tile, i)
            
            print(f"   Success. Master Map is now {Image.open(MASTER_MAP_FILE).width}px wide.")
            print("   Cooling down for 20 seconds to dodge rate limits...")
            time.sleep(20) 

    print(f"\nFinished! Result saved to {MASTER_MAP_FILE}")

if __name__ == "__main__":
    main()