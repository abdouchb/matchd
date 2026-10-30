import socket
import json
import time
import os

HOST = '127.0.0.1'
PORT = 4444

def clear_screen():
    os.system('cls' if os.name == 'nt' else 'clear')

def main():
    print(f"[*] Waiting for ZarqaaOS Telemetry on {HOST}:{PORT}...")
    
    allocations = {}
    network_conns = {}
    
    while True:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((HOST, PORT))
                print("[+] Connected to ZarqaaOS Telemetry Stream!")
                
                buffer = ""
                while True:
                    data = s.recv(1024)
                    if not data:
                        break
                    
                    buffer += data.decode('utf-8', errors='ignore')
                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        line = line.strip()
                        if not line:
                            continue
                            
                        try:
                            event = json.loads(line)
                        except json.JSONDecodeError:
                            continue
                        
                        if event.get('type') == 'heap':
                            addr = event.get('addr')
                            if event.get('action') == 'alloc':
                                allocations[addr] = event.get('size', 0)
                            elif event.get('action') == 'free':
                                if addr in allocations:
                                    del allocations[addr]
                                    
                        elif event.get('type') == 'net':
                            host = event.get('host', 'unknown')
                            action = event.get('action')
                            if action == 'start':
                                network_conns[host] = "Connecting..."
                            elif action == 'connect':
                                network_conns[host] = "Connected (OK)" if event.get('err', 0) == 0 else f"Failed ({event.get('err')})"
                            elif action == 'close':
                                network_conns[host] = f"Closed (Status: {event.get('status', 'unknown')})"
                            elif action == 'error':
                                network_conns[host] = f"TCP Error ({event.get('err')})"
                                
                        # Update dashboard (throttled to avoid flicker on fast allocs)
                        if event.get('type') == 'net' or len(allocations) % 200 == 0:
                            clear_screen()
                            print("="*50)
                            print("         ZarqaaOS Telemetry Dashboard")
                            print("="*50)
                            total_alloc_bytes = sum(allocations.values())
                            print(f"Active Heap Allocations: {len(allocations)} blocks ({total_alloc_bytes} bytes)")
                            print("-"*50)
                            print("Active Network Connections:")
                            if not network_conns:
                                print("  (None)")
                            else:
                                for h, status in network_conns.items():
                                    print(f"  - {h}: {status}")
                            print("="*50)
                            
        except ConnectionRefusedError:
            time.sleep(1)
        except Exception as e:
            print(f"[-] Error: {e}")
            time.sleep(1)

if __name__ == '__main__':
    main()
