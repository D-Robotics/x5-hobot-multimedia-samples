#!/bin/bash

# 定义函数用于解析指定section的value
parse_section() {
    section=$1
    target_key=$2
    while IFS='=' read -r key value || [ -n "$key" ]
    do
        if [[ $key == '#'* ]]; then
            continue  # 跳过注释行
        elif [ "$key" == "$target_key" ]; then
            echo "[$section] $key = $value"
            # 在这里添加针对指定键值的解析逻辑
        fi
    done
}

# 定义函数用于解析INI文件中的指定section和指定键值
parse_ini_section() {
    local target_section=$1
    local target_key=$2
    local ini_file=$3
    local section_found=0

    while IFS= read -r line
    do
        if [[ $line =~ ^\[$target_section\]$ ]]; then
            section_found=1
        elif [[ $line =~ ^\[.+\]$ && $section_found -eq 1 ]]; then
            break
        elif [ $section_found -eq 1 ]; then
            echo "$line" | parse_section "$target_section" "$target_key"
        fi
    done < "$ini_file"
}

