#include <filesystem>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <fmt/os.h>
#include <fmt/ranges.h>

void find_files_in_dir_append(std::vector<std::string>& append_str, std::string dir_name, std::string post_fix, bool recursive)
{

    if (recursive)
    {
        for (auto const& dir_entry : std::filesystem::recursive_directory_iterator(dir_name))
        {
            if (dir_entry.is_directory()) continue;

            if (auto ext = dir_entry.path().extension().string(); ext == post_fix)
            {
                append_str.push_back(dir_entry.path().string());
            }
        }
    }
    else
    {
        for (auto const& dir_entry : std::filesystem::directory_iterator(dir_name))
        {
            if (dir_entry.is_directory()) continue;

            if (auto ext = dir_entry.path().extension().string(); ext == post_fix)
            {
                append_str.push_back(dir_entry.path().string());
            }
        }
    }
}

auto map_vec(auto&& vec, auto&& func)
{
    std::vector<decltype(func(vec.back()))> ret_vec;
    ret_vec.reserve(vec.size());
    for (auto&& e : vec)
    {
        ret_vec.push_back(func(e));
    }
    return ret_vec;
}

const char* compile_rule     = "compile";
const char* link_rule        = "link";
const char* glslc_rule       = "glc";
const char* spirv_embed_rule = "embed_spirv";

const char* glslc_compiler = "glslc";
const char* spirv_embeder  = "util/spirv_embeder.out";

class NinjaBuilder
{
public:
    class GlslCompiler
    {
    public:
        GlslCompiler(fmt::ostream& file) : m_file(file)
        {
        }

        void compile_glsl(const std::string& glsl_file, const std::string& spirv_dir = ".obj_files/spirv_files/")
        {
            std::string spirv_file = glsl_file.substr(0, glsl_file.rfind("."));

            for (char& c : spirv_file)
                c = c == '/' ? '_' : c;

            std::string embed_name = spirv_file;

            spirv_file = spirv_dir + spirv_file + ".sp";

            m_file.print("build {}:{} {}\n  glflags= {}\n", spirv_file, glslc_rule, glsl_file, glslc_flags);

            m_glsl_files.push_back(CompiledGlsl{
                .spirv_file = spirv_file,
                .embed_name = glsl_file});
        }

        auto embed(const std::string& embeded_cpp_file)
        {
            m_file.print("build {}:{} ", embeded_cpp_file, spirv_embed_rule);

            for (auto& glsl_file : m_glsl_files)
                m_file.print("{} {} ", glsl_file.spirv_file, glsl_file.embed_name);

            m_file.print("\n");

            return embeded_cpp_file;
        }

    public:
        std::string glslc_flags;

    private:
        struct CompiledGlsl
        {
            std::string spirv_file;
            std::string embed_name;
        };

        std::vector<CompiledGlsl> m_glsl_files;

        fmt::ostream& m_file;
    };

    NinjaBuilder(const std::string& ninja_file)
        : m_file(fmt::output_file(ninja_file))
    {
        m_file.print("compiler= clang++\n");
        m_file.print("rule {}\n  deps= gcc\n  depfile= $out.d\n  command= $compiler $cflags -MMD -MF $out.d -c $in -o $out\n", compile_rule);
        m_file.print("rule {}\n  command= $compiler $cflags $ldflags $in -o $out\n", link_rule);
        m_file.print("rule {}\n  deps= gcc\n  depfile= $out.d\n  command= {} -MD -MF $out.d $glflags $in -o $out && spirv-opt -Os $out -o $out\n", glslc_rule, glslc_compiler);
        m_file.print("rule {}\n  command= {} $args -o $out\n", spirv_embed_rule, spirv_embeder);
    }

    auto glsl_compiler()
    {
        return GlslCompiler(m_file);
    }

    std::string compile_cpp_file(const std::string& cpp_file, const std::string& obj_file_dir = ".obj_files/")
    {
        std::string obj_file = cpp_file.substr(0, cpp_file.rfind(".")) + ".o";

        for (char& c : obj_file)
            c = c == '/' ? '_' : c;

        obj_file = obj_file_dir + obj_file;

        m_file.print("build {}:{} {}\n  cflags= {}\n", obj_file, compile_rule, cpp_file, compile_flags);

        return obj_file;
    }

    void build_executable(const std::string& exec_name, const std::string& obj_files)
    {
        m_file.print("build {}:{} {}\n  cflags= {}\n  ldflags= {}\n", exec_name, link_rule, obj_files, compile_flags, link_flags);
    }

public:
    std::string compile_flags, link_flags;

private:
    fmt::ostream m_file;
};

int main()
{
    auto builder          = NinjaBuilder("build.ninja");
    builder.compile_flags = "-std=c++20 -Ilibs -g -O0";
    builder.link_flags    = "-lSDL2 -ldl -lvulkan -lpthread -lfmt";

    std::vector<std::string> glsl_files;
    find_files_in_dir_append(glsl_files, "src/", ".comp", true);
    find_files_in_dir_append(glsl_files, "src/", ".vert", true);
    find_files_in_dir_append(glsl_files, "src/", ".frag", true);

    auto glsl_compiler = builder.glsl_compiler();

    for (const auto& glsl_file : glsl_files)
        glsl_compiler.compile_glsl(glsl_file);

    std::vector<std::string> src_dirs = {"src/", "libs/"};

    std::vector<std::string> cpp_files;
    for (const auto& src_dir : src_dirs)
        find_files_in_dir_append(cpp_files, src_dir, ".cpp", true);

    cpp_files.push_back(glsl_compiler.embed(".obj_files/embeded_spirv.cpp"));

    auto obj_files = map_vec(cpp_files, [&](const std::string& cpp_file) {
        return builder.compile_cpp_file(cpp_file);
    });

    builder.build_executable("out.out", fmt::format("{}", fmt::join(obj_files, " ")));
}