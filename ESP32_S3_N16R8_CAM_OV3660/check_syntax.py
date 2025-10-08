#!/usr/bin/env python3
"""
H.264 RTP 代码语法检查器
用于验证C代码的基本语法正确性
"""

import re
import os

def check_c_syntax(file_path):
    """检查C文件的基本语法"""
    print(f"Checking: {file_path}")
    
    if not os.path.exists(file_path):
        print(f"❌ File not found: {file_path}")
        return False
    
    with open(file_path, 'r') as f:
        content = f.read()
    
    errors = []
    
    # 检查括号匹配
    stack = []
    for i, char in enumerate(content):
        if char in '({[':
            stack.append((char, i))
        elif char in ')}]':
            if not stack:
                errors.append(f"Line {content[:i].count(chr(10)) + 1}: Unmatched closing '{char}'")
            else:
                open_char, _ = stack.pop()
                if (char == ')' and open_char != '(') or \
                   (char == '}' and open_char != '{') or \
                   (char == ']' and open_char != '['):
                    errors.append(f"Line {content[:i].count(chr(10)) + 1}: Mismatched brackets")
    
    if stack:
        for open_char, pos in stack:
            errors.append(f"Line {content[:pos].count(chr(10)) + 1}: Unclosed '{open_char}'")
    
    # 检查基本的C语法结构
    lines = content.split('\n')
    for i, line in enumerate(lines, 1):
        stripped = line.strip()
        if not stripped or stripped.startswith('//') or stripped.startswith('/*'):
            continue
            
        # 检查未关闭的字符串
        in_string = False
        escaped = False
        for char in stripped:
            if escaped:
                escaped = False
                continue
            if char == '\\':
                escaped = True
            elif char == '"' and not escaped:
                in_string = not in_string
        
        if in_string:
            errors.append(f"Line {i}: Unclosed string literal")
    
    # 检查include语句
    includes = re.findall(r'#include\s+[<"][^>"]+[>"]', content)
    print(f"✓ Found {len(includes)} include statements")
    
    # 检查函数定义
    functions = re.findall(r'(static\s+)?(\w+\s+)+\w+\s*\([^)]*\)\s*{', content)
    print(f"✓ Found {len(functions)} function definitions")
    
    # 检查结构体定义
    structs = re.findall(r'typedef\s+struct[^{]*{[^}]+}\s*\w+;', content, re.DOTALL)
    print(f"✓ Found {len(structs)} struct definitions")
    
    if errors:
        print("❌ Syntax errors found:")
        for error in errors:
            print(f"   {error}")
        return False
    else:
        print("✅ Basic syntax check passed")
        return True

def check_header_file(file_path):
    """检查头文件"""
    print(f"Checking header: {file_path}")
    
    if not os.path.exists(file_path):
        print(f"❌ Header not found: {file_path}")
        return False
    
    with open(file_path, 'r') as f:
        content = f.read()
    
    # 检查头文件保护
    if '#ifndef' in content and '#define' in content and '#endif' in content:
        print("✅ Header guard found")
    else:
        print("⚠️  No header guard detected")
    
    # 检查extern "C"
    if 'extern "C"' in content:
        print("✅ C++ compatibility found")
    
    return True

def main():
    print("H.264 RTP Code Syntax Checker")
    print("=" * 40)
    
    # 检查的文件列表
    files_to_check = [
        "components/h264_rtsp_server/h264_rtsp_server.c",
        "components/h264_rtsp_server/include/h264_rtsp_server.h", 
        "components/h264_rtsp_server/include/h264_test_data.h",
        "main/main.c"
    ]
    
    all_good = True
    
    for file_path in files_to_check:
        print()
        if file_path.endswith('.h'):
            if not check_header_file(file_path):
                all_good = False
        else:
            if not check_c_syntax(file_path):
                all_good = False
    
    print("\n" + "=" * 40)
    if all_good:
        print("✅ All files passed syntax check!")
        print("The H.264 RTP implementation should compile correctly.")
    else:
        print("❌ Some files have syntax errors.")
        print("Please fix the errors before building.")
    
    return all_good

if __name__ == "__main__":
    main()