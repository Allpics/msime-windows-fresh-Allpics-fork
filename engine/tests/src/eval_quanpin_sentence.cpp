// 全拼整句级切分评测：逐条敲入 tests/data/quanpin_sentence_cases.tsv 的输入，看首选、第 2 位和
// 预编辑，并统计每键耗时。
//
// 不是测试：不断言，也不进 ctest。改切分或整句打分前后各跑一遍对比，例如
//   set METASEQUOIA_IME_DATA_DIR=<数据目录的副本>
//   eval_quanpin_sentence engine\tests\data\quanpin_sentence_cases.tsv [repeats] [lattice]
// 只敲键和重置，从不选词，不写用户日志；仍请指向副本，别用正在用的数据目录。
#include "../../core/data_path.h"
#include "../../core/input_session.h"
#include "../../core/sentence_association_options.h"
#include "../../quanpin/quanpin_utils.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct Case
{
    std::string input;
    std::string expected;
    std::string category;
};

std::vector<Case> load_cases(const char *path)
{
    std::vector<Case> cases;
    std::ifstream file(path, std::ios::binary);
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line.front() == '#')
            continue;
        std::istringstream fields(line);
        Case item;
        std::getline(fields, item.input, '\t');
        std::getline(fields, item.expected, '\t');
        std::getline(fields, item.category, '\t');
        if (!item.input.empty() && !item.expected.empty())
            cases.push_back(std::move(item));
    }
    return cases;
}

double percentile(std::vector<double> values, double p)
{
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const size_t index = (std::min)(values.size() - 1, static_cast<size_t>(p * (values.size() - 1) + 0.5));
    return values[index];
}

struct Setting
{
    const char *name;
    unsigned autocorrect_types;
};
} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: eval_quanpin_sentence <cases.tsv> [repeats] [lattice]\n");
        return 2;
    }
    const auto cases = load_cases(argv[1]);
    const int repeats = argc > 2 ? (std::max)(1, std::atoi(argv[2])) : 3;

    // 与出厂默认一致：词格整句 + 神经速度档 + 万象搭配加分（模型在数据目录里时）。
    // 第 3 个参数写 lattice 时只开词格，用来把神经重排的耗时单独拆出来。
    const bool lattice_only = argc > 3 && std::string(argv[3]) == "lattice";
    SentenceAssociationOptions options;
    options.word_lattice = true;
    options.neural_keyboard = !lattice_only;
    const auto collocation = metasequoia::data_directory() / "models" / "zh-moqi" / "zh-moqi.gram";
    if (std::filesystem::exists(collocation))
        options.collocation_model = metasequoia::path_to_utf8(collocation);

    const unsigned all_types = quanpin::kAutocorrectTransposition | quanpin::kAutocorrectNeighbor |
                               quanpin::kAutocorrectDeletion | quanpin::kAutocorrectInsertion;
    const Setting settings[] = {{"autocorrect off", 0}, {"autocorrect on", all_types}};

    for (const Setting &setting : settings)
    {
        metasequoia::InputSession session(SchemeType::Quanpin, setting.autocorrect_types);
        session.set_sentence_association(options);
        std::printf("== %s ==\n", setting.name);
        std::map<std::string, std::pair<int, int>> by_category;
        std::vector<double> per_key;
        for (const Case &item : cases)
        {
            std::string head;
            std::string second;
            std::string display;
            // 第一轮预热，只记后面几轮的耗时；结果每轮都一样，取最后一轮。
            for (int round = -1; round < repeats; ++round)
            {
                session.reset_state();
                session.reset_cache();
                for (const char character : item.input)
                {
                    const auto start = std::chrono::steady_clock::now();
                    session.handle_character(character);
                    const double ms =
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                    if (round >= 0)
                        per_key.push_back(ms);
                }
                const auto &candidates = session.candidates();
                head = candidates.empty() ? std::string{} : candidates[0].word;
                second = candidates.size() < 2 ? std::string{} : candidates[1].word;
                display = session.get_pinyin_segmentation_with_cases();
            }
            const bool pass = head == item.expected;
            auto &counts = by_category[item.category];
            counts.first += pass ? 1 : 0;
            counts.second += 1;
            std::printf("%s  %-30s %-10s 1:%s  2:%s  [%s]\n", pass ? "PASS" : "FAIL", item.input.c_str(),
                        item.category.c_str(), head.c_str(), second.c_str(), display.c_str());
        }
        for (const auto &[category, counts] : by_category)
            std::printf("  %-10s %d/%d\n", category.c_str(), counts.first, counts.second);
        std::printf("  ms per key: median %.2f  p90 %.2f  max %.2f\n\n", percentile(per_key, 0.5),
                    percentile(per_key, 0.9), percentile(per_key, 1.0));
        session.reset_state();
    }
    return 0;
}
