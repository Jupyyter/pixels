import sys
import os
import re
from google import genai
from PIL import Image

def get_available_blocks(constants_file_path):
    # Regex to catch the names inside your X(...) macro
    pattern = re.compile(r'X\(\s*([A-Za-z0-9_]+)\s*,')
    blocks = []
    
    try:
        with open(constants_file_path, 'r') as f:
            for line in f:
                match = pattern.search(line)
                if match:
                    blocks.append(match.group(1))
    except FileNotFoundError:
        print(f"Could not find {constants_file_path}. Please check the path.")
        sys.exit(1)
        
    return blocks

def get_prominent_colors(image_path, num_colors=20):
    with Image.open(image_path) as img:
        img = img.convert('RGB')
        # Shrink to speed up processing and cluster colors
        img.thumbnail((256, 256))
        # Quantize reduces the image to the most prominent N colors
        q_img = img.quantize(colors=num_colors)
        palette = q_img.getpalette()
        
        # Extract unique colors actually used
        colors_in_use = [item[1] for item in q_img.getcolors()]
        colors = []
        for idx in colors_in_use:
            r, g, b = palette[idx*3 : idx*3+3]
            if (r, g, b) not in colors:
                colors.append((r, g, b))
                
        return colors, img

def analyze_image_with_gemini(image_path, api_key, constants_path):
    # Initialize the NEW SDK Client
    client = genai.Client(api_key=api_key)
    
    print(f"Extracting colors from {image_path}...")
    colors, original_img = get_prominent_colors(image_path, 24)
    color_list_str = "\n".join([f"- RGB({r}, {g}, {b})" for r, g, b in colors])
    
    print(f"Looking for blocks in: {constants_path}")
    available_blocks = get_available_blocks(constants_path)
    print(f"Found {len(available_blocks)} blocks in Constants.hpp")
    
    prompt = f"""
    Analyze the attached image and the following prominent colors extracted from it:
    {color_list_str}

    I am building a 2D falling sand game. Map each of these colors to the most visually or conceptually similar block from this list:
    {', '.join(available_blocks)}.

    CRITICAL RULES:
    1. First, identify what real-world object or element this color represents in the image (e.g., Sky, Tree, Ground, Wall). This is your 'Initial_Thought' (use underscores instead of spaces, e.g., Blue_Sky).
    2. Backgrounds are drawn separately in my game! If the color represents a BACKGROUND element (like the sky, sun, or distant empty space), it MUST NOT be a physical block. Map it to "None".
    3. If the color represents a foreground object, map it to the closest matching block from the list.
    4. Output EXACTLY one line per color in this strict format: R G B MappedMaterial Initial_Thought
    5. Do not output markdown, headers, or any other text.

    Example Outputs:
    135 206 235 None Blue_Sky
    34 139 34 Wood Tree_Leaves
    120 120 120 Stone Concrete_Wall
    """

    print("Asking Gemini to map the colors...")
    
    response = client.models.generate_content(
        model='gemini-3.5-flash',
        contents=[prompt, original_img]
    )
    
    with open("color_map.txt", "w") as f:
        for line in response.text.strip().split('\n'):
            parts = line.strip().split()
            # As long as there are at least 4 parts, it's a valid line for C++
            if len(parts) >= 4 and parts[0].isdigit():
                f.write(line.strip() + "\n")
                
    print("Success! Wrote color mapping to 'color_map.txt'.")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python ai_map_generator.py <image_path> <gemini_api_key> [path_to_constants.hpp]")
    else:
        # Get the directory where this python script is located
        script_dir = os.path.dirname(os.path.abspath(__file__))
        
        # Default to include/Constants.hpp relative to the script directory
        default_constants_path = os.path.join(script_dir, "include", "Constants.hpp")
        
        # Allow overriding via command line argument, otherwise use the default path
        constants_file = sys.argv[3] if len(sys.argv) > 3 else default_constants_path
        
        analyze_image_with_gemini(sys.argv[1], sys.argv[2], constants_file)