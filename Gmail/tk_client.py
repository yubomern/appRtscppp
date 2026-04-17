# tk_client.py
import tkinter as tk
from tkinter.scrolledtext import ScrolledText
import socket
import threading

HOST = '127.0.0.1'
PORT = 9000

class App:
    def __init__(self, master):
        self.master = master
        master.title("TCP Command Client")
        self.text = ScrolledText(master, height=15)
        self.text.pack(fill='both', expand=True)
        self.entry = tk.Entry(master)
        self.entry.pack(fill='x')
        self.entry.bind('<Return>', self.send_cmd)
        btn_frame = tk.Frame(master)
        btn_frame.pack(fill='x')
        tk.Button(btn_frame, text='Start all', command=lambda: self.send_line('start_all')).pack(side='left')
        tk.Button(btn_frame, text='Stop all', command=lambda: self.send_line('stop_all')).pack(side='left')
        tk.Button(btn_frame, text='Run python', command=self.run_python).pack(side='left')
        self.sock = None
        self.connect()

    def connect(self):
        try:
            self.sock = socket.create_connection((HOST, PORT))
            threading.Thread(target=self.recv_loop, daemon=True).start()
            self.text.insert('end', 'Connected to server\n')
        except Exception as e:
            self.text.insert('end', f'Connect error: {e}\n')

    def recv_loop(self):
        try:
            while True:
                data = self.sock.recv(4096)
                if not data: break
                self.text.insert('end', data.decode())
                self.text.see('end')
        except Exception as e:
            self.text.insert('end', f'Recv error: {e}\n')

    def send_line(self, line):
        if not self.sock: return
        try:
            self.sock.sendall((line + '\n').encode())
        except Exception as e:
            self.text.insert('end', f'Send error: {e}\n')

    def send_cmd(self, event=None):
        line = self.entry.get().strip()
        if not line: return
        self.send_line(line)
        self.entry.delete(0, 'end')

    def run_python(self):
        # sample command to run via server: call python script on server side
        cmd = 'run_python python -u python_script_example.py'
        self.send_line(cmd)

if __name__ == '__main__':
    root = tk.Tk()
    app = App(root)
    root.mainloop()
