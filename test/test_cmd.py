CASES = r'''
$ bin/client.exe set test_key hello_world
(nil)
$ bin/client.exe get test_key
(str) hello_world
$ bin/client.exe get non_existent_key
(nil)
$ bin/client.exe del test_key
(int) 1
$ bin/client.exe get test_key
(nil)
$ bin/client.exe set key1 value1
(nil)
$ bin/client.exe set key2 value2
(nil)
$ bin/client.exe set key3 value3
(nil)
$ bin/client.exe keys
(arr) len=3(str) key3
(str) key2
(str) key1
(arr) end
$ bin/client.exe del key1
(int) 1
$ bin/client.exe del key2
(int) 1
$ bin/client.exe del key3
(int) 1
$ bin/client.exe zscore asdf n1
(nil)
$ bin/client.exe zquery xxx 1 asdf 1 10
(arr) len=0(arr) end
$ bin/client.exe zadd zset 1 n1
(int) 1
$ bin/client.exe zadd zset 2 n2
(int) 1
$ bin/client.exe zadd zset 1.1 n1
(int) 0
$ bin/client.exe zscore zset n1
(dbl) 1.1
$ bin/client.exe zquery zset 1 "" 0 10
(arr) len=4(str) n1
(dbl) 1.1
(str) n2
(dbl) 2
(arr) end
$ bin/client.exe zquery zset 1.1 "" 1 10
(arr) len=2(str) n2
(dbl) 2
(arr) end
$ bin/client.exe zquery zset 1.1 "" 2 10
(arr) len=0(arr) end
$ bin/client.exe zrem zset adsf
(int) 0
$ bin/client.exe zrem zset n1
(int) 1
$ bin/client.exe zquery zset 1 "" 0 10
(arr) len=2(str) n2
(dbl) 2
(arr) end
'''


import shlex
import subprocess
import os
import sys
import time
import signal
import threading

# 确定客户端路径
def get_client_path():
    if os.name == 'nt':  # Windows
        return 'bin/client.exe'
    else:  # Linux/Mac
        return 'bin/client'

def get_server_path():
    if os.name == 'nt':  # Windows
        return 'bin/server.exe'
    else:  # Linux/Mac
        return 'bin/server'

def is_windows():
    return os.name == 'nt'

def start_server(server_path):
    """启动服务器进程"""
    try:
        # 检查服务器是否已经在运行
        if is_windows():
            # 在Windows上检查端口6379是否被占用
            result = subprocess.run(['netstat', '-an'], capture_output=True, text=True)
            if ':6379' in result.stdout:
                print("Server already running on port 6379")
                return None
        else:
            # 在Linux/Mac上检查端口
            result = subprocess.run(['lsof', '-i', ':6379'], capture_output=True, text=True)
            if result.returncode == 0:
                print("Server already running on port 6379")
                return None
        
        # 启动服务器
        server_process = subprocess.Popen(
            [server_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        # 等待服务器启动
        time.sleep(2)
        return server_process
    except Exception as e:
        print(f"Failed to start server: {e}")
        return None

def stop_server(server_process):
    """停止服务器进程"""
    if server_process:
        try:
            if is_windows():
                server_process.terminate()
            else:
                server_process.send_signal(signal.SIGTERM)
            server_process.wait(timeout=5)
        except:
            server_process.kill()

def run_test_case(cmd, expect):
    """运行单个测试用例"""
    try:
        result = subprocess.run(
            shlex.split(cmd), 
            capture_output=True,
            text=True,
            timeout=10
        )
        
        # 处理客户端输出：显示所有接收到的输出
        output_lines = result.stdout.split('\n')
        actual_output = []
        
        for line in output_lines:
            line = line.strip()
            if not line:
                continue
            actual_output.append(line)
        
        actual_out = '\n'.join(actual_output) + '\n' if actual_output else ''
        
        # 提取Redis响应部分用于验证（不包含连接信息）
        redis_response = []
        for line in output_lines:
            line = line.strip()
            if not line:
                continue
            # 忽略连接信息
            if line.startswith('Connecting to server') or line.startswith('Connected to server'):
                continue
            redis_response.append(line)
        
        redis_out = '\n'.join(redis_response) + '\n' if redis_response else ''
        
        # 显示所有接收到的输出
        print(f"All output for {cmd}:")
        print(repr(actual_out))
        
        if redis_out == expect:
            print(f"✓ PASS: {cmd}")
            return True
        else:
            print(f"✗ FAIL: {cmd}")
            print(f"  Expected: {repr(expect)}")
            print(f"  Got (Redis response only): {repr(redis_out)}")
            return False
            
    except subprocess.CalledProcessError as e:
        # 即使命令失败，也可能有正确的输出
        output_lines = e.stdout.split('\n')
        actual_output = []
        
        for line in output_lines:
            line = line.strip()
            if not line:
                continue
            actual_output.append(line)
        
        actual_out = '\n'.join(actual_output) + '\n' if actual_output else ''
        
        # 提取Redis响应部分用于验证（不包含连接信息）
        redis_response = []
        for line in output_lines:
            line = line.strip()
            if not line:
                continue
            # 忽略连接信息
            if line.startswith('Connecting to server') or line.startswith('Connected to server'):
                continue
            redis_response.append(line)
        
        redis_out = '\n'.join(redis_response) + '\n' if redis_response else ''
        
        # 显示所有接收到的输出
        print(f"All output for {cmd} (with error exit):")
        print(repr(actual_out))
        
        if redis_out == expect:
            print(f"✓ PASS (with error exit): {cmd}")
            return True
        else:
            print(f"✗ FAIL (with error exit): {cmd}")
            print(f"  Expected: {repr(expect)}")
            print(f"  Got (Redis response only): {repr(redis_out)}")
            print(f"  Exit code: {e.returncode}")
            return False
            
    except subprocess.TimeoutExpired:
        print(f"✗ TIMEOUT: {cmd}")
        return False
    except FileNotFoundError:
        print(f"✗ FILE NOT FOUND: {cmd}")
        print("  Make sure to build the project first with: cmake --build .")
        return False

def main():
    CLIENT_PATH = get_client_path()
    SERVER_PATH = get_server_path()
    
    # 检查客户端是否存在
    if not os.path.exists(CLIENT_PATH):
        print(f"Client executable not found at {CLIENT_PATH}")
        print("Please build the project first with: cmake --build .")
        sys.exit(1)
    
    # 启动服务器
    server_process = start_server(SERVER_PATH)
    if server_process is None and not is_windows():
        # 在非Windows系统上，如果服务器没启动就退出
        print("Server is not running and could not be started")
        sys.exit(1)
    
    try:
        # 解析测试用例并根据当前系统调整路径
        cmds = []
        outputs = []
        lines = CASES.splitlines()
        for x in lines:
            x = x.strip()
            if not x:
                continue
            if x.startswith('$ '):
                # 根据当前系统替换命令路径
                cmd_line = x[2:]
                if is_windows():
                    cmd_line = cmd_line.replace('bin/client.exe', CLIENT_PATH)
                else:
                    cmd_line = cmd_line.replace('bin/client.exe', CLIENT_PATH)
                cmds.append(cmd_line)
                outputs.append('')
            elif cmds:  # 只有在已经有命令的情况下才添加输出
                outputs[-1] = outputs[-1] + x + '\n'
        
        assert len(cmds) == len(outputs), "Mismatched commands and outputs"
        
        # 运行所有测试用例
        passed = 0
        total = len(cmds)
        
        print(f"Running {total} test cases...")
        print("=" * 50)
        
        for i, (cmd, expect) in enumerate(zip(cmds, outputs), 1):
            print(f"[{i}/{total}] ", end="")
            if run_test_case(cmd, expect):
                passed += 1
        
        print("=" * 50)
        print(f"Results: {passed}/{total} tests passed")
        
        if passed == total:
            print("All tests passed! 🎉")
            return 0
        else:
            print("Some tests failed! ❌")
            return 1
            
    finally:
        # 清理：停止服务器
        if server_process:
            stop_server(server_process)

if __name__ == "__main__":
    sys.exit(main())
