import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import concurrent.futures
import threading
import queue
import os

# expected header names we support (normalized -> canonical)
CANONICAL_ORDER = ["BAV","FPY","FPYGLOBAL","WIFI","date","ICT","BNFT"]

# mapping of acceptable input header spellings to canonical keys (lowercase)
HEADER_ALIASES = {
    "date":"date",
    "fpy":"FPY",
    "fpYglobal".lower():"FPYGLOBAL",
    "fpyglobal":"FPYGLOBAL",
    "fp yglobal".replace(" ","").lower():"FPYGLOBAL",
    "bav":"BAV",
    "bnft":"BNFT",
    "wifi":"WIFI",
    "wifi":"WIFI",
    "ict":"ICT",
    "fypglobal":"FPYGLOBAL",  # common typos handled
    "fyp":"FPY"
}
# Ensure lowercase keys map properly
HEADER_ALIASES = {k.lower(): v for k,v in HEADER_ALIASES.items()}

def normalize_token(tok: str) -> str:
    return tok.strip()

def map_index_to_field(tokens):
    """
    Given a line split into tokens, try to map indices to canonical fields.
    This function assumes input lines are in a *fixed order*, but we handle common
    misspellings from your sample by mapping token index -> canonical field by position.
    If file has a header row, we detect it and build mapping from header strings.
    """
    # fallback positional mapping based on sample you provided:
    # sample seemed like: date FPY BAV BNFT ICT WIFI FPYGLOBAL
    # So default positional mapping:
    pos_map = ["date","FPY","BAV","BNFT","ICT","WIFI","FPYGLOBAL"]
    res = {}
    for i, key in enumerate(pos_map):
        if i < len(tokens):
            res[key] = tokens[i]
        else:
            res[key] = ""
    return res

def parse_line_into_dict(line: str):
    tokens = line.strip().split()
    if not tokens:
        return None
    # If the first token looks like a header (non-date words), we will skip it
    # We will attempt to parse every line with default positional mapping.
    data = map_index_to_field(tokens)
    # make sure keys are canonical (uppercase for most)
    canonical = {
        "date": data.get("date",""),
        "FPY": data.get("FPY",""),
        "BAV": data.get("BAV",""),
        "BNFT": data.get("BNFT",""),
        "ICT": data.get("ICT",""),
        "WIFI": data.get("WIFI",""),
        "FPYGLOBAL": data.get("FPYGLOBAL","")
    }
    return canonical

class FileProcessor:
    def __init__(self, filepath, ui_queue):
        self.filepath = filepath
        self.ui_queue = ui_queue
        self.lock = threading.Lock()
        self.rows = []  # list of dict rows

    def process(self):
        # read file lines and parse them in a thread pool
        with open(self.filepath, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()

        # remove empty lines, optionally the header line (we'll ignore header if non-data)
        lines = [ln for ln in lines if ln.strip()]
        # If first line contains a non-date word (e.g., starts with letters and not a date),
        # we treat it as header and skip it.
        if lines:
            first_tokens = lines[0].split()
            # crude check: if first token contains letters and is not like yyyy-mm or dd/mm
            if any(c.isalpha() for c in first_tokens[0]) and not any(ch.isdigit() for ch in first_tokens[0]):
                # assume header -> skip
                lines = lines[1:]

        # Use ThreadPoolExecutor to parse lines
        with concurrent.futures.ThreadPoolExecutor(max_workers=6) as exe:
            futures = [exe.submit(parse_line_into_dict, ln) for ln in lines]
            for f in concurrent.futures.as_completed(futures):
                row = f.result()
                if row:
                    with self.lock:
                        self.rows.append(row)
                    # send to UI queue for incremental display
                    self.ui_queue.put(row)
        # signal finished
        self.ui_queue.put(None)

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("File Parser GUI")
        self.geometry("900x450")

        # UI queue to receive parsed rows from worker
        self.ui_queue = queue.Queue()

        control_frame = ttk.Frame(self)
        control_frame.pack(fill="x", padx=6, pady=6)

        self.lbl_path = ttk.Label(control_frame, text="No file selected")
        self.lbl_path.pack(side="left", padx=6)

        btn_open = ttk.Button(control_frame, text="Open file", command=self.open_file)
        btn_open.pack(side="left", padx=6)
        btn_save = ttk.Button(control_frame, text="Save processed", command=self.save_processed)
        btn_save.pack(side="left", padx=6)

        # Treeview columns: use canonical order plus 'raw' sentinel
        cols = CANONICAL_ORDER.copy()
        cols.insert(0, "row")  # optional row number
        self.tree = ttk.Treeview(self, columns=cols, show="headings")
        for c in cols:
            self.tree.heading(c, text=c)
            self.tree.column(c, width=110, anchor="center")
        self.tree.pack(expand=True, fill="both", padx=6, pady=6)

        self.processor = None
        self.rows = []
        self.after(100, self.poll_queue)

    def open_file(self):
        path = filedialog.askopenfilename(title="Select data file", filetypes=[("Text files","*.txt"),("All files","*.*")])
        if not path:
            return
        self.lbl_path.config(text=path)
        # clear tree
        for item in self.tree.get_children():
            self.tree.delete(item)
        self.rows.clear()
        self.processor = FileProcessor(path, self.ui_queue)
        # start parsing in a separate thread
        t = threading.Thread(target=self.processor.process, daemon=True)
        t.start()

    def poll_queue(self):
        """Called regularly on the UI thread to pull parsed rows from queue"""
        try:
            while True:
                item = self.ui_queue.get_nowait()
                if item is None:
                    # finished signal
                    # we could show a message or update UI
                    if self.processor:
                        # stash rows
                        self.rows = self.processor.rows.copy()
                    # optional: notify
                    # messagebox.showinfo("Done", "Finished parsing file")
                    continue
                # item is a dict with keys canonical
                self.add_row_to_tree(item)
        except queue.Empty:
            pass
        self.after(100, self.poll_queue)

    def add_row_to_tree(self, d):
        index = len(self.tree.get_children()) + 1
        display = [index] + [d.get(k,"") for k in CANONICAL_ORDER]
        self.tree.insert("", "end", values=display)

    def save_processed(self):
        if not (self.processor and self.processor.rows):
            messagebox.showwarning("No data", "No parsed data to save.")
            return
        save_path = filedialog.asksaveasfilename(defaultextension=".txt", filetypes=[("Text files","*.txt")])
        if not save_path:
            return
        # Write header in requested order: BAV FPY FPYGLOBAL WIFI date ICT BNFT
        with open(save_path, "w", encoding="utf-8") as f:
            f.write("\t".join(CANONICAL_ORDER) + "\n")
            # rows are dicts; write values in canonical order
            for row in self.processor.rows:
                vals = [row.get(k,"") for k in CANONICAL_ORDER]
                f.write("\t".join(vals) + "\n")
        messagebox.showinfo("Saved", f"Saved {len(self.processor.rows)} rows to {save_path}")

if __name__ == "__main__":
    print("app start")
    app = App()
    app.mainloop()
