#!/usr/bin/env python3
import sys
import socket
import threading

def read_from_socket_write_to_stdout(sock):
    buffer = ""
    while True:
        try:
            data = sock.recv(4096)
            if not data:
                break
            buffer += data.decode('utf-8')
            while '\n' in buffer:
                line, buffer = buffer.split('\n', 1)
                sys.stdout.write(line + '\n')
                sys.stdout.flush()
        except Exception:
            break

def read_from_stdin_write_to_socket(sock):
    while True:
        try:
            line = sys.stdin.readline()
            if not line:
                break
            sock.sendall(line.encode('utf-8'))
        except Exception:
            break

def main():
    host = '127.0.0.1'
    port = 8080

    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            pass

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((host, port))
    except Exception as e:
        sys.stderr.write(f"Failed to connect to vAmigaImgui MCP server on {host}:{port}: {e}\n")
        sys.exit(1)

    t1 = threading.Thread(target=read_from_socket_write_to_stdout, args=(sock,), daemon=True)
    t1.start()

    read_from_stdin_write_to_socket(sock)
    sock.close()

if __name__ == "__main__":
    main()
