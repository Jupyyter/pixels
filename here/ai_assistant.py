import os
import re
import json
import webbrowser
import tkinter as tk
from tkinter import filedialog
from flask import Flask, request, jsonify, render_template_string
import google.generativeai as genai

app = Flask(__name__)

# Prevent browser from caching the old UI
@app.after_request
def add_header(response):
    response.headers['Cache-Control'] = 'no-store'
    return response

EXCLUDE_DIRS = {
    '.git', 'node_modules', 'venv', 'env', '__pycache__', '.idea', 
    '.vscode', '.vs', 'dist', 'build', 'out', 'bin', 'obj', 'coverage'
}

INCLUDE_EXTS = {
    '.py', '.js', '.ts', '.jsx', '.tsx', '.html', '.css', '.scss', 
    '.java', '.c', '.cpp', '.h', '.hpp', '.cs', '.go', '.rs', '.php', 
    '.rb', '.swift', '.kt', '.dart', '.json', '.xml', '.sh', '.bat', 
    '.yml', '.yaml', '.md', '.sql', '.vue', '.svelte', '.ini', '.toml'
}

# --- IN-MEMORY VERSION HISTORY ---
PROJECT_HISTORY = {}

def get_all_files(root_dir):
    file_list = []
    for root, dirs, files in os.walk(root_dir):
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
        for f in files:
            ext = os.path.splitext(f)[1].lower()
            if ext in INCLUDE_EXTS:
                full_path = os.path.relpath(os.path.join(root, f), root_dir)
                if full_path != os.path.basename(__file__): 
                    file_list.append(full_path.replace('\\', '/'))
    return sorted(file_list)

def apply_search_replace(file_content, search_str, replace_str):
    file_content = file_content.replace('\r\n', '\n')
    search_str = search_str.replace('\r\n', '\n')
    replace_str = replace_str.replace('\r\n', '\n')

    def clean_md(text):
        t = text.strip()
        if t.startswith("```"):
            t = re.sub(r'^```[^\n]*\n', '', t)
            t = re.sub(r'\n```$', '', t)
        return t

    search_clean = clean_md(search_str)
    replace_clean = clean_md(replace_str)

    if search_clean in file_content:
        return file_content.replace(search_clean, replace_clean)
        
    if search_clean.strip('\n') in file_content:
        return file_content.replace(search_clean.strip('\n'), replace_clean.strip('\n'))
        
    if search_clean.strip() in file_content:
        return file_content.replace(search_clean.strip(), replace_clean.strip())
        
    return None

@app.route('/api/browse', methods=['GET'])
def browse_folder():
    try:
        root = tk.Tk()
        root.withdraw()
        root.attributes('-topmost', True)
        folder_path = filedialog.askdirectory(title="Select Project Folder")
        root.destroy()
        
        if folder_path:
            return jsonify({"path": folder_path.replace('\\', '/')})
        return jsonify({"path": ""})
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/files', methods=['GET'])
def list_files():
    root_dir = request.args.get('dir', '')
    if not root_dir or not os.path.exists(root_dir):
        return jsonify([])
    return jsonify(get_all_files(root_dir))

@app.route('/api/history', methods=['GET'])
def get_history():
    root_dir = request.args.get('dir', '')
    history = PROJECT_HISTORY.get(root_dir, [])
    summary = [{"id": v["id"], "name": v["name"]} for v in history]
    return jsonify(summary)

@app.route('/api/models', methods=['POST'])
def list_models():
    """Fetches available Gemini models dynamically from the Google API."""
    data = request.json
    api_key = data.get('api_key', '')
    
    if not api_key:
        return jsonify({"error": "No API Key provided"}), 400
        
    try:
        genai.configure(api_key=api_key)
        # Fetch all models and filter for those that support text generation
        valid_models = []
        for m in genai.list_models():
            if 'generateContent' in m.supported_generation_methods:
                # Strip the "models/" prefix to make the UI cleaner
                clean_name = m.name.replace('models/', '')
                valid_models.append(clean_name)
        
        # Sort so the newest/flash models appear near the top generally
        valid_models.sort(reverse=True)
        return jsonify({"models": valid_models})
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/generate', methods=['POST'])
def generate_prompt():
    data = request.json
    root_dir = data.get('dir', '')
    prompt = data.get('prompt', '')
    files = data.get('files', [])
    
    combined = prompt + "\n\n"
    for fp in files:
        full_path = os.path.join(root_dir, fp)
        try:
            with open(full_path, 'r', encoding='utf-8') as f:
                combined += f"--- FILE: {fp} ---\n{f.read()}\n\n"
        except Exception as e:
            combined += f"--- FILE: {fp} ---\n[Error reading file: {e}]\n\n"
            
    return jsonify({"result": combined})

@app.route('/api/apply', methods=['POST'])
def apply_code():
    data = request.json
    root_dir = data.get('dir', '')
    original_prompt = data.get('original_prompt', '')
    ai_response = data.get('ai_response', '')
    files = data.get('files', [])
    
    api_key = data.get('api_key', '')
    model_name = data.get('model', 'gemini-1.5-flash')
    
    if not api_key:
        return jsonify({"status": "error", "message": "API Key not provided. Please enter it in the UI."}), 400
        
    try:
        genai.configure(api_key=api_key)
        model = genai.GenerativeModel(model_name, generation_config={"max_output_tokens": 8192})
    except Exception as e:
        return jsonify({"status": "error", "message": f"Failed to initialize Gemini API: {str(e)}"}), 500
    
    files_context = []
    valid_files = []
    for fp in files:
        full_path = os.path.join(root_dir, fp)
        try:
            with open(full_path, 'r', encoding='utf-8') as f:
                content = f.read()
            files_context.append(f"--- FILE: {fp} ---\n{content}\n")
            valid_files.append(fp)
        except Exception:
            pass
            
    context_str = "\n".join(files_context)
    
    prompt = f"""You are a specialized code merging assistant.
I will provide you with the full story: My original request, the AI's response, and the CURRENT state of my files.

ORIGINAL PROMPT (My Question):
{original_prompt}

AI INSTRUCTIONS / SNIPPETS (The Answer):
{ai_response}

CURRENT FILES:
{context_str}

AVAILABLE FILES TO MODIFY:
{', '.join(valid_files)}

YOUR TASK:
Read the AI instructions carefully, figure out how they apply to the CURRENT FILES, and output the necessary modifications using strictly `<search>` and `<replace>` blocks.

OUTPUT FORMAT:
You MUST use the exact XML format below. Do not output anything else. No conversational text.

<version>A short 3 to 5 word description of the fix</version>

<file path="path/to/file.cpp">
<search>
[Exact code snippet from the ORIGINAL file to replace. Include a few lines of context above and below so it's a unique match.]
</search>
<replace>
[The new code that will replace the search block.]
</replace>
</file>

CRITICAL RULES:
1. NEVER output the entire file. You MUST use <search> and <replace> blocks. This prevents token limits on large files.
2. The <search> block MUST be an exact character-for-character substring of the CURRENT FILE, including spaces and indentation. 
3. Your 'path' attribute MUST match one of the AVAILABLE FILES TO MODIFY.
4. You can put multiple <search> and <replace> blocks inside a single <file> tag.
"""
    try:
        response = model.generate_content(prompt)
        raw_text = response.text.strip()
        
        updates = {}
        failed_blocks = []
        
        vname_match = re.search(r"<version>(.*?)</version>", raw_text, re.IGNORECASE | re.DOTALL)
        version_name = vname_match.group(1).strip() if vname_match else "AI Code Update"
        
        file_matches = re.finditer(r'<file path=["\']?(.*?)["\']?>(.*?)(?:</file>|\Z)', raw_text, re.IGNORECASE | re.DOTALL)
        
        found_any_files = False
        for match in file_matches:
            found_any_files = True
            ai_fp = match.group(1).strip()
            modifications = match.group(2)
            
            matched_fp = None
            for v_file in valid_files:
                if v_file == ai_fp or v_file.endswith('/' + ai_fp) or v_file.endswith('\\' + ai_fp):
                    matched_fp = v_file
                    break
                    
            if not matched_fp:
                failed_blocks.append((ai_fp, "FILE_NOT_SELECTED_IN_UI"))
                continue
                
            full_path = os.path.join(root_dir, matched_fp)
            with open(full_path, 'r', encoding='utf-8') as f:
                file_content = f.read()
                
            blocks = re.finditer(r'<search>(.*?)</search>\s*<replace>(.*?)</replace>', modifications, re.IGNORECASE | re.DOTALL)
            
            changes_made = 0
            for block in blocks:
                search_str = block.group(1)
                replace_str = block.group(2)
                
                new_content = apply_search_replace(file_content, search_str, replace_str)
                if new_content:
                    file_content = new_content
                    changes_made += 1
                else:
                    failed_blocks.append((matched_fp, "SEARCH_TEXT_NOT_FOUND"))
                    
            if changes_made > 0:
                updates[matched_fp] = file_content
            elif changes_made == 0 and '<search>' not in modifications:
                if "</file>" not in match.group(0).lower():
                    failed_blocks.append((matched_fp, "ENTIRE_FILE_TRUNCATED"))
                else:
                    updates[matched_fp] = apply_search_replace(file_content, file_content, modifications) or modifications.strip()
                    
        if not found_any_files:
            snippet = raw_text[:800] + ("\n...[truncated]" if len(raw_text) > 800 else "")
            return jsonify({
                "status": "error", 
                "message": f"Gemini failed to output the XML correctly. It generated:\n\n{snippet}"
            }), 400

        if not updates and failed_blocks:
            err_details = "\n".join([f"- {fp}: {reason}" for fp, reason in failed_blocks])
            return jsonify({
                "status": "error", 
                "message": f"Failed to apply any changes. Details:\n{err_details}"
            }), 400

        history = PROJECT_HISTORY.setdefault(root_dir, [])
        current_state = {}
        for fp in valid_files:
            full_path = os.path.join(root_dir, fp)
            with open(full_path, 'r', encoding='utf-8') as f:
                current_state[fp] = f.read()
                
        if len(history) == 0:
            history.append({"id": 0, "name": "Initial Base State", "files": dict(current_state)})
            
        new_state = dict(current_state)
        updated_list = []
        for fp, new_content in updates.items():
            full_path = os.path.join(root_dir, fp)
            with open(full_path, 'w', encoding='utf-8') as f:
                f.write(new_content)
            new_state[fp] = new_content
            updated_list.append(fp)
                
        if updated_list:
            history.append({"id": len(history), "name": version_name, "files": new_state})
            
        warning_msg = ""
        if failed_blocks:
            warning_msg = "⚠️ Some blocks failed to apply:\n" + "\n".join([f"- {fp} ({reason})" for fp, reason in failed_blocks])
            
        return jsonify({
            "status": "success", 
            "updated_files": updated_list, 
            "version_name": version_name,
            "warnings": warning_msg
        })
    except Exception as e:
        err_str = str(e)
        if "API key not valid" in err_str or "API_KEY_INVALID" in err_str:
            err_str = "Invalid API Key. Please check your key and try again."
        return jsonify({"status": "error", "message": err_str}), 500

@app.route('/api/history/revert', methods=['POST'])
def revert_history():
    data = request.json
    root_dir = data.get('dir', '')
    version_id = data.get('version_id')
    
    history = PROJECT_HISTORY.get(root_dir, [])
    target = next((v for v in history if v["id"] == version_id), None)
    
    if not target:
        return jsonify({"status": "error", "message": "Version not found in session memory."}), 404
        
    try:
        for fp, content in target["files"].items():
            full_path = os.path.join(root_dir, fp)
            with open(full_path, 'w', encoding='utf-8') as f:
                f.write(content)
                
        return jsonify({"status": "success"})
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

# ==================== HTML / UI ====================
HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Efficient AI Coding UI</title>
    <style>
        body { margin: 0; background-color: #1e1e1e; color: #cccccc; font-family: 'Segoe UI', Tahoma, sans-serif; display: flex; height: 100vh; overflow: hidden; }
        .panel { flex: 1; display: flex; flex-direction: column; padding: 20px; border-right: 1px solid #333; }
        .panel:last-child { border-right: none; }
        h2 { margin-top: 0; font-size: 1.2rem; color: #ffffff; }
        
        .file-list { 
            overflow-y: auto; overflow-x: hidden;
            background-color: #1e1e1e; 
            border: 1px solid #3c3c3c; 
            border-radius: 4px; 
            margin-bottom: 15px;
            padding: 5px 0; 
        }
        .tree-item { 
            display: flex; align-items: center; 
            padding: 4px 8px; 
            cursor: pointer; 
            font-size: 0.9rem;
            user-select: none;
            color: #cccccc;
        }
        .tree-item:hover { background-color: #2a2d2e; }
        .tree-item.selected { background-color: #37373d; }
        .tree-children { display: none; }
        .tree-children.expanded { display: block; }
        .chevron { display: inline-block; width: 18px; text-align: center; color: #888; font-size: 0.7rem; margin-right: 2px;}
        .icon { margin-right: 6px; font-size: 1rem; }
        .label { white-space: nowrap; }

        .history-item { margin-bottom: 0; font-size: 0.9rem; display: flex; align-items: center; justify-content: space-between; padding: 6px 12px; cursor: default; }
        .history-item:hover { background-color: #2a2d2e; }
        .history-btn { background-color: transparent; padding: 4px 8px; font-size: 0.8rem; margin: 0; border: 1px solid #555; border-radius: 4px; color: #ccc; cursor: pointer; font-weight: bold; }
        .history-btn:hover { background-color: #555; color: white;}
        
        input[type="text"], input[type="password"], select { 
            background-color: #252526; color: #ffffff; border: 1px solid #3c3c3c; border-radius: 4px; padding: 8px; font-family: monospace; 
        }
        textarea { flex: 1; background-color: #252526; color: #d4d4d4; border: 1px solid #3c3c3c; border-radius: 4px; padding: 10px; font-family: monospace; resize: none; margin-bottom: 15px; font-size: 0.95rem; white-space: pre-wrap; }
        
        button { background-color: #0e639c; color: #ffffff; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; font-size: 1rem; margin-bottom: 10px; font-weight: 600;}
        button:hover { background-color: #1177bb; }
        button:disabled { background-color: #555; cursor: not-allowed; }
        .secondary-btn { background-color: #4CAF50; }
        .secondary-btn:hover { background-color: #45a049; }
        
        .info { font-size: 0.9rem; margin-bottom: 5px; color: #aaaaaa; }
        .status { font-weight: bold; margin-top: 5px; white-space: pre-wrap; font-family: monospace; font-size: 0.85rem;}
        .error { color: #f44336; }
        .folder-select { display: flex; gap: 10px; margin-bottom: 15px; }
        .settings-row { display: flex; gap: 10px; margin-bottom: 15px; align-items: center; }
    </style>
</head>
<body>
    <div class="panel">
        <h2>1. Context & History</h2>
        
        <div class="info">Project Folder:</div>
        <div class="folder-select">
            <input type="text" id="project-dir" value="" placeholder="Click Browse to select folder..." style="flex: 1;" readonly>
            <button onclick="browseFolder()" class="secondary-btn" style="margin: 0;">Browse</button>
        </div>

        <div class="info">Files to include as context:</div>
        <div class="file-list" id="file-list" style="flex: 1.5;"></div>

        <div class="info">Session History (Click to switch state):</div>
        <div class="file-list" id="history-list" style="flex: 1; margin-bottom: 0;">
            <div style="color: #888; font-size: 0.9rem; padding: 10px;">No changes made yet.</div>
        </div>
    </div>

    <div class="panel">
        <h2>2. Generate Prompt</h2>
        <div class="info">Write what you want the AI to do.</div>
        <textarea id="prompt-text" placeholder="E.g., Refactor this class to use async/await..."></textarea>
        <button id="btn-copy" onclick="generateAndCopy()">Copy Prompt to Clipboard</button>
        <div class="status" id="copy-status" style="color: #4CAF50;"></div>
    </div>

    <div class="panel">
        <h2>3. Apply Changes</h2>
        
        <div class="info">API Settings (Saved locally in browser):</div>
        <div class="settings-row">
            <input type="password" id="api-key" placeholder="Enter Gemini API Key..." style="flex: 1;" onchange="saveKeyPref()">
            <button class="secondary-btn" onclick="fetchModels()" style="margin: 0; padding: 8px 12px; font-size: 0.9rem;" title="Fetch available models">🔄 Load</button>
            <select id="model-select" onchange="saveModelPref()" style="min-width: 180px;">
                <option value="gemini-1.5-flash">gemini-1.5-flash</option>
            </select>
        </div>

        <div class="info">Paste the response from ChatGPT/Claude/Gemini here:</div>
        <textarea id="ai-response" placeholder="Paste the AI response here..."></textarea>
        <button id="btn-apply" onclick="applyChanges()">Update Files via Gemini</button>
        <div class="status" id="apply-status"></div>
    </div>

    <script>
        // --- On Load: Restore settings from Local Storage ---
        document.addEventListener("DOMContentLoaded", () => {
            const savedKey = localStorage.getItem('gemini_api_key');
            if (savedKey) {
                document.getElementById('api-key').value = savedKey;
                fetchModels(); // Automatically fetch available models if key is present
            }
        });

        function saveKeyPref() {
            localStorage.setItem('gemini_api_key', document.getElementById('api-key').value.trim());
        }

        function saveModelPref() {
            localStorage.setItem('gemini_model', document.getElementById('model-select').value);
        }

        async function fetchModels() {
            const apiKey = document.getElementById('api-key').value.trim();
            if (!apiKey) return;
            
            saveKeyPref(); // Save it before fetching
            
            const select = document.getElementById('model-select');
            const currentModel = localStorage.getItem('gemini_model') || select.value;
            
            select.innerHTML = '<option value="">Fetching live models...</option>';
            select.disabled = true;

            try {
                const res = await fetch('/api/models', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({api_key: apiKey})
                });
                const data = await res.json();
                
                if (data.models && data.models.length > 0) {
                    select.innerHTML = '';
                    data.models.forEach(m => {
                        const opt = document.createElement('option');
                        opt.value = m;
                        opt.textContent = m;
                        select.appendChild(opt);
                    });
                    
                    // Attempt to restore their previously selected model
                    if (data.models.includes(currentModel)) {
                        select.value = currentModel;
                    } else if (data.models.includes('gemini-1.5-flash')) {
                        select.value = 'gemini-1.5-flash';
                    }
                    saveModelPref();
                } else {
                    select.innerHTML = `<option value="${currentModel}">${currentModel}</option>`;
                    if (data.error) alert("Error fetching models: " + data.error);
                }
            } catch (err) {
                select.innerHTML = `<option value="${currentModel}">${currentModel}</option>`;
                console.error(err);
            }
            select.disabled = false;
        }

        function getDir() {
            return document.getElementById('project-dir').value;
        }

        async function browseFolder() {
            const container = document.getElementById('file-list');
            container.innerHTML = '<div style="color: #888; padding: 10px;">Opening folder picker... Check your taskbar.</div>';
            
            try {
                const res = await fetch('/api/browse');
                const data = await res.json();
                if (data.path) {
                    document.getElementById('project-dir').value = data.path;
                    refreshFiles();
                    refreshHistory();
                } else {
                    refreshFiles(); 
                }
            } catch (err) {
                container.innerHTML = `<div class="error" style="padding: 10px;">Error opening folder picker.</div>`;
            }
        }

        function buildTree(paths) {
            const root = {};
            paths.forEach(path => {
                const parts = path.split('/');
                let current = root;
                for (let i = 0; i < parts.length; i++) {
                    const part = parts[i];
                    if (i === parts.length - 1) {
                        current[part] = path;
                    } else {
                        current[part] = current[part] || {};
                        current = current[part];
                    }
                }
            });
            return root;
        }

        function renderTree(node, container, level = 0) {
            const keys = Object.keys(node).sort((a, b) => {
                const isFolderA = typeof node[a] === 'object' && node[a] !== null;
                const isFolderB = typeof node[b] === 'object' && node[b] !== null;
                if (isFolderA && !isFolderB) return -1;
                if (!isFolderA && isFolderB) return 1;
                return a.localeCompare(b);
            });

            keys.forEach(key => {
                const val = node[key];
                const isFolder = typeof val === 'object' && val !== null;
                const itemDiv = document.createElement('div');
                
                const rowDiv = document.createElement('div');
                rowDiv.className = 'tree-item';
                rowDiv.style.paddingLeft = `${level * 16 + 4}px`; 
                
                if (isFolder) {
                    rowDiv.innerHTML = `<span class="chevron">▶</span> <span class="icon">📁</span> <span class="label">${key}</span>`;
                    
                    const childrenDiv = document.createElement('div');
                    childrenDiv.className = 'tree-children';
                    renderTree(val, childrenDiv, level + 1);
                    
                    rowDiv.onclick = () => {
                        const isExpanded = childrenDiv.classList.toggle('expanded');
                        rowDiv.querySelector('.chevron').innerText = isExpanded ? '▼' : '▶';
                    };
                    
                    itemDiv.appendChild(rowDiv);
                    itemDiv.appendChild(childrenDiv);
                } else {
                    rowDiv.innerHTML = `
                        <span class="chevron"></span> 
                        <input type="checkbox" class="file-checkbox" value="${val}" style="margin:0 8px 0 0; pointer-events:none;"> 
                        <span class="icon">📄</span> <span class="label">${key}</span>
                    `;
                    rowDiv.onclick = () => {
                        const cb = rowDiv.querySelector('input[type="checkbox"]');
                        cb.checked = !cb.checked;
                        if (cb.checked) {
                            rowDiv.classList.add('selected');
                        } else {
                            rowDiv.classList.remove('selected');
                        }
                    };
                    itemDiv.appendChild(rowDiv);
                }
                container.appendChild(itemDiv);
            });
        }

        async function refreshFiles() {
            const dir = getDir();
            const container = document.getElementById('file-list');
            if (!dir) {
                container.innerHTML = '<div style="color: #888; padding: 10px;">No folder selected.</div>';
                return;
            }
            container.innerHTML = '<div style="padding: 10px;">Loading code files...</div>';

            try {
                const res = await fetch('/api/files?dir=' + encodeURIComponent(dir));
                const files = await res.json();
                
                container.innerHTML = '';
                if(files.length === 0) {
                    container.innerHTML = '<div style="color: #888; padding: 10px;">No code files found in this directory.</div>';
                    return;
                }

                const tree = buildTree(files);
                renderTree(tree, container, 0);

            } catch (err) {
                container.innerHTML = `<div class="error" style="padding: 10px;">Network error loading files.</div>`;
            }
        }

        async function refreshHistory() {
            const dir = getDir();
            const container = document.getElementById('history-list');
            if (!dir) return;

            try {
                const res = await fetch('/api/history?dir=' + encodeURIComponent(dir));
                const history = await res.json();
                
                if(history.length === 0) {
                    container.innerHTML = '<div style="color: #888; font-size: 0.9rem; padding: 10px;">No changes made yet.</div>';
                    return;
                }

                container.innerHTML = '';
                [...history].reverse().forEach(v => {
                    const div = document.createElement('div');
                    div.className = 'history-item';
                    div.innerHTML = `
                        <span><strong>v${v.id}</strong>: ${v.name}</span>
                        <button class="history-btn" onclick="revertTo(${v.id})">Switch to</button>
                    `;
                    container.appendChild(div);
                });
            } catch (err) {
                container.innerHTML = `<div class="error" style="padding: 10px;">Error loading history.</div>`;
            }
        }

        function getSelectedFiles() {
            return Array.from(document.querySelectorAll('.file-checkbox:checked')).map(cb => cb.value);
        }

        async function generateAndCopy() {
            const dir = getDir();
            if (!dir) return alert("Select a project folder first.");

            const files = getSelectedFiles();
            const prompt = document.getElementById('prompt-text').value;
            const btn = document.getElementById('btn-copy');
            const status = document.getElementById('copy-status');
            
            btn.disabled = true;
            status.innerText = 'Compiling...';

            const res = await fetch('/api/generate', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({dir, prompt, files})
            });
            const data = await res.json();
            
            await navigator.clipboard.writeText(data.result);
            status.innerText = '✓ Copied to clipboard! (Ready to paste into your AI)';
            btn.disabled = false;
        }

        async function applyChanges() {
            const dir = getDir();
            if (!dir) return alert("Select a project folder first.");

            const files = getSelectedFiles();
            const original_prompt = document.getElementById('prompt-text').value;
            const ai_response = document.getElementById('ai-response').value;
            
            const api_key = document.getElementById('api-key').value.trim();
            const model = document.getElementById('model-select').value;
            
            const btn = document.getElementById('btn-apply');
            const status = document.getElementById('apply-status');

            if (!api_key) {
                status.style.color = '#f44336';
                status.innerText = 'Error: Please enter your Gemini API Key.';
                return;
            }
            if (files.length === 0) {
                status.style.color = '#f44336';
                status.innerText = 'Error: Select files on the left first.';
                return;
            }
            if (!ai_response.trim()) {
                status.style.color = '#f44336';
                status.innerText = 'Error: Paste an AI response first.';
                return;
            }

            // Save key/model immediately to storage
            saveKeyPref();
            saveModelPref();

            btn.disabled = true;
            status.style.color = '#4CAF50';
            status.innerText = 'Gemini is processing diffs... Please wait.';

            try {
                const res = await fetch('/api/apply', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({
                        dir: dir, 
                        original_prompt: original_prompt, 
                        ai_response: ai_response, 
                        files: files,
                        api_key: api_key,
                        model: model
                    })
                });
                const data = await res.json();

                if (data.status === 'success') {
                    let msg = `✓ Updated [${data.version_name}]\\n` + (data.updated_files.join(', ') || 'No changes made.');
                    if (data.warnings) {
                        msg += '\\n\\n' + data.warnings;
                        status.style.color = '#ff9800'; // Orange
                    } else {
                        status.style.color = '#4CAF50';
                    }
                    status.innerText = msg;
                    refreshHistory(); 
                } else {
                    status.style.color = '#f44336'; // Red
                    status.innerText = data.message;
                }
            } catch (err) {
                status.style.color = '#f44336';
                status.innerText = "Network Error: Could not reach the Python backend.";
            }
            
            btn.disabled = false;
        }

        async function revertTo(version_id) {
            const dir = getDir();
            if (confirm(`Are you sure you want to rewind your code state to Version ${version_id}?`)) {
                const res = await fetch('/api/history/revert', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({dir, version_id})
                });
                const data = await res.json();
                
                if (data.status === 'success') {
                    alert(`Successfully switched files to Version ${version_id}.`);
                } else {
                    alert('Error reverting: ' + data.message);
                }
            }
        }

        refreshFiles();
        refreshHistory();
    </script>
</body>
</html>
"""

@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE)

if __name__ == '__main__':
    print("Starting AI Coding Assistant. Opening browser...")
    webbrowser.open("http://127.0.0.1:5000")
    app.run(port=5000, debug=False)