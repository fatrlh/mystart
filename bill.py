import re

def preprocess_file(input_file, intermediate_1):
    """
    预处理阶段：去除无效内容，生成第一个中间结果文件。
    """
    with open(input_file, 'r', encoding='utf-8') as infile, open(intermediate_1, 'w', encoding='utf-8') as outfile:
        skip = False  # 是否跳过无效内容
        for line in infile:
            line = line.strip()
            if line.startswith("中国银行交易流水明细清单"):
                skip = True  # 开始跳过无效内容
            elif re.match(r'^\d{4}-\d{2}-\d{2}', line):  # 以日期开头的行
                skip = False  # 结束跳过，开始记录有效内容
            elif line.startswith("温馨提示:"):
                skip = True  # 开始跳过无效内容
            elif line.startswith("----END----"):
                skip = False  # 结束跳过
            if not skip and line:  # 如果不在跳过状态且行不为空
                outfile.write(line + "\n")

def merge_lines(intermediate_1, intermediate_2):
    """
    数据行合并阶段：将换行数据合并为完整的一行，生成第二个中间结果文件。
    """
    with open(intermediate_1, 'r', encoding='utf-8') as infile, open(intermediate_2, 'w', encoding='utf-8') as outfile:
        buffer = []  # 用于存储当前数据行的内容
        for line in infile:
            line = line.strip()
            if re.match(r'^\d{4}-\d{2}-\d{2}', line):  # 以日期开头的行
                if buffer:  # 如果buffer不为空，表示上一个数据行结束
                    outfile.write(" ".join(buffer) + "\n")  # 写入完整数据行
                    buffer = []  # 清空buffer
                buffer.append(line)  # 开始新的数据行
            else:
                buffer.append(line)  # 继续拼接当前数据行
        if buffer:  # 处理最后一行数据
            outfile.write(" ".join(buffer) + "\n")

def process_data(intermediate_2, output_file):
    """
    数据处理阶段：提取目标数据并生成输出文件。
    """
    total_gas = 0.0
    total_water = 0.0
    total_electricity = 0.0
    target_transactions = []

    with open(intermediate_2, 'r', encoding='utf-8') as infile, open(output_file, 'w', encoding='utf-8') as outfile:
        for line in infile:
            line = line.strip()
            print(f"Processing line: {line}")  # 调试信息：打印当前处理的行
            # 定位 "人民币"
            rmb_index = line.find('人民币')
            if rmb_index != -1:
                # 提取金额字段
                amount_start = rmb_index + len('人民币')
                amount_end = line.find(' ', amount_start+1)
                if amount_end == -1:
                    amount_end = len(line)
                amount_str = line[amount_start:amount_end].strip()
                print(f"Extracted amount string: '{amount_str}'")  # 调试信息：打印提取的金额字段
                # 清理金额字符串，去除逗号和其他非数字字符（除了小数点）
                cleaned_amount_str = ''.join(char for char in amount_str if char.isdigit() or char == '.')
                print(f"Cleaned amount string: '{cleaned_amount_str}'")  # 调试信息：打印清理后的金额字段
                # 转换为数字
                try:
                    amount = float(cleaned_amount_str)
                    print(f"Amount converted to float: {amount}")  # 调试信息：打印转换后的金额
                    # 根据关键词分类
                    if '燃气' in line:
                        print("Matched '燃气'")  # 调试信息：打印匹配的关键字
                        total_gas += abs(amount)
                        target_transactions.append(line)
                    elif '水务' in line:
                        print("Matched '水务'")  # 调试信息：打印匹配的关键字
                        total_water += abs(amount)
                        target_transactions.append(line)
                    elif '供电' in line:
                        print("Matched '供电'")  # 调试信息：打印匹配的关键字
                        total_electricity += abs(amount)
                        target_transactions.append(line)
                except ValueError:
                    print(f"Failed to convert amount: '{cleaned_amount_str}'")  # 调试信息：打印转换失败的金额
            else:
                print("No '人民币' found")  # 调试信息：打印未找到 "人民币"

        # 写入匹配到的数据行
        outfile.write("匹配到的数据行：\n")
        outfile.write("=" * 50 + "\n")
        for transaction in target_transactions:
            outfile.write(transaction + "\n")
        outfile.write("=" * 50 + "\n\n")

        # 写入汇总数据
        outfile.write("汇总数据：\n")
        outfile.write("=" * 50 + "\n")
        outfile.write(f"煤气总支出: {total_gas:.2f} 元\n")
        outfile.write(f"水务总支出: {total_water:.2f} 元\n")
        outfile.write(f"电总支出: {total_electricity:.2f} 元\n")
        outfile.write(f"总支出: {total_gas + total_water + total_electricity:.2f} 元\n")


def main():
    input_file = '2025.txt'  # 输入文件路径
    intermediate_1 = 'intermediate_1.txt'  # 第一个中间结果文件
    intermediate_2 = 'intermediate_2.txt'  # 第二个中间结果文件
    output_file = 'output.txt'  # 输出文件路径

    # 预处理阶段
    preprocess_file(input_file, intermediate_1)
    print(f"预处理完成，生成第一个中间结果文件: {intermediate_1}")

    # 数据行合并阶段
    merge_lines(intermediate_1, intermediate_2)
    print(f"数据行合并完成，生成第二个中间结果文件: {intermediate_2}")

    # 数据处理阶段
    process_data(intermediate_2, output_file)
    print(f"数据处理完成，生成输出文件: {output_file}")

if __name__ == "__main__":
    main()