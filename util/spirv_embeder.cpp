#include <fstream>
#include <string>
#include <tuple>
#include <vector>

#include <fmt/format.h>
#include <fmt/os.h>
#include <fmt/ranges.h>

std::vector<uint32_t> read_file(const char* file_path)
{
    auto file = std::ifstream(file_path, std::ios::binary | std::ios::ate);

    if (!file.is_open()) throw std::runtime_error(std::string("file: ") + file_path + " doesn't exists");

    size_t file_size = file.tellg();
    file.seekg(0);

    if (file_size % 4 != 0) throw std::runtime_error("not a spirv file");

    auto file_data = std::vector<uint32_t>(file_size / sizeof(uint32_t));

    file.read((char*)file_data.data(), file_size);

    return file_data;
}

std::string string_replace(const std::string& input_string, std::string find, std::string replace)
{
    size_t old_find_end = 0;

    size_t find_pos = input_string.find(find, old_find_end);

    std::string ret_string;

    ret_string.reserve(input_string.size() * 2);

    while (find_pos != input_string.npos)
    {
        ret_string += input_string.substr(old_find_end, find_pos - old_find_end);
        ret_string += replace;

        old_find_end = find_pos + find.size();

        if (old_find_end >= input_string.size()) break;

        find_pos = input_string.find(find, old_find_end);
    }

    ret_string += input_string.substr(old_find_end, input_string.size() - old_find_end);

    return ret_string;
}

int main(int argc, const char** argv)
{
    std::vector<std::pair<std::string, std::string>> files_to_embed;

    std::string out_file_name;

    for (int i = 1; i < argc; ++i)
    {
        if (i + 1 < argc && std::string("-o") == argv[i])
        {
            i++;
            out_file_name = argv[i];
            if (size_t pos = out_file_name.rfind(".cpp"); pos != out_file_name.npos)
            {
                out_file_name.resize(pos);
            }
        }
        else
        {
            std::string sprv_file = argv[i];
            ++i;
            std::string glsl_file = argv[i];

            files_to_embed.emplace_back(sprv_file, glsl_file);
        }
    }

    fmt::print("embdeded files {}\n",files_to_embed);

    if (out_file_name.size() == 0)
    {
        fmt::print(stderr, "no output file name is defined exiting!\n");
        return -1;
    }

    auto out_cpp = fmt::output_file(out_file_name + ".cpp");

    out_cpp.print("#include <inttypes.h>\n");
    out_cpp.print("#include <unordered_map>\n");
    out_cpp.print("#include <string>\n");

    out_cpp.print("\n\n");

    std::string sprv_map;
    auto sm_inserter = std::back_inserter(sprv_map);

    fmt::format_to(sm_inserter, "extern const std::unordered_map<std::string,std::pair<const uint32_t*,uint32_t>> embeded_sprvs;\n");
    fmt::format_to(sm_inserter, "const std::unordered_map<std::string,std::pair<const uint32_t*,uint32_t>> embeded_sprvs = {{\n");

    size_t last_slash = out_file_name.rfind("/");

    for (const auto& [sprv_filename, shader_filename] : files_to_embed)
    {
        auto array_name = string_replace(sprv_filename, "/", "_");
        array_name.resize(array_name.rfind(".sp"));
        array_name = string_replace(array_name, ".", "_");

        auto out_cpp_binary = read_file(sprv_filename.c_str());

        out_cpp.print("const uint32_t {}[] = {{ {} }};\n", array_name, fmt::join(out_cpp_binary, ","));

        fmt::format_to(sm_inserter, "\t{{ \"{}\", {{ {},{} }} }},\n", shader_filename, array_name, out_cpp_binary.size());
    }

    fmt::format_to(sm_inserter, "}};\n");

    out_cpp.print("\n{}", sprv_map);
}