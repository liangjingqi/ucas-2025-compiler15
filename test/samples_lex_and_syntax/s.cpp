#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <regex>
#include <algorithm>

struct CommandEntry {
    int number;
    std::string command;
};

// 提取文件名前的数字（如 21 来自 21_xxx.cact 或 27.cact）
int extractNumber(const std::string& line) {
    std::regex re(R"((\d+))");
    std::smatch match;
    if (std::regex_search(line, match, re)) {
        return std::stoi(match[1]);
    }
    return -1; // 提取失败
}

int main() {
    std::ifstream input("a.txt");
    if (!input.is_open()) {
        std::cerr << "无法打开 a.txt 文件！" << std::endl;
        return 1;
    }

    std::vector<CommandEntry> entries;
    std::string line;
    while (std::getline(input, line)) {
        int number = extractNumber(line);
        if (number != -1) {
            entries.push_back({number, line});
        }
    }
    input.close();

    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.number < b.number;
    });

    std::ofstream output("a.txt");
    if (!output.is_open()) {
        std::cerr << "无法写入 a.txt 文件！" << std::endl;
        return 1;
    }

    for (const auto& entry : entries) {
        output << entry.command << "\n";
    }

    std::cout << "✅ 排序完成，结果已写入 a.txt。" << std::endl;
    return 0;
}
