#include <filesystem>
#include <fstream>
#include <regex>
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

std::string read_file(const std::string& filename)
{
    auto input_file = std::ifstream(filename);
    if (!input_file) throw std::runtime_error(fmt::format("couldn't open file: {}", filename));

    return std::string(std::istreambuf_iterator<char>(input_file), std::istreambuf_iterator<char>());
}

const char* compile_rule     = "compile";
const char* link_rule        = "link";
const char* glslc_rule       = "glc";
const char* spirv_embed_rule = "embed_spirv";
const char* stlib_rule       = "static_lib";

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
            std::string spirv_file = glsl_file;

            for (char& c : spirv_file)
                c = c == '/' ? '_' : c;

            std::string embed_name = spirv_file;

            spirv_file = spirv_dir + spirv_file;

            std::regex re("\\[variant\\[(\\w+)\\]\\]");
            auto file_data = read_file(glsl_file);
            for (auto it = std::sregex_iterator(file_data.begin(), file_data.end(), re); it != std::sregex_iterator(); ++it)
            {
                auto match  = *it;
                auto define = match[1].str();
                fmt::print("found variant {}\n", define);
                auto spirv_variant = fmt::format("{}.{}.sp", spirv_file, define);
                m_file.print("build {}:{} {}\n  glflags= {}\n", spirv_variant, glslc_rule, glsl_file, fmt::format("{} -D{}", glslc_flags, define));

                m_glsl_files.push_back(CompiledGlsl{
                    .spirv_file = spirv_variant,
                    .embed_name = fmt::format("{}.D{}", glsl_file, define),
                });
            }

            m_file.print("build {}:{} {}\n  glflags= {}\n", spirv_file + ".sp", glslc_rule, glsl_file, glslc_flags);

            m_glsl_files.push_back(CompiledGlsl{
                .spirv_file = spirv_file + ".sp",
                .embed_name = glsl_file,
            });
        }

        auto embed(const std::string& embeded_cpp_file)
        {
            m_file.print("build {}:{} ", embeded_cpp_file, spirv_embed_rule);

            auto embeds = fmt::output_file(embeded_cpp_file + ".embeds");

            for (auto& glsl_file : m_glsl_files)
            {
                m_file.print("{} ", glsl_file.spirv_file);
                embeds.print("{}\n", glsl_file.embed_name);
            }

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
        m_file.print("rule {}\n  command= {} -o $out $in\n", spirv_embed_rule, spirv_embeder);
        m_file.print("rule {}\n  command= ar rcs $out $in\n", stlib_rule);
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

    void build_static_lib(const std::string& exec_name, const std::string& obj_files)
    {
        m_file.print("build {}:{} {}\n", exec_name, stlib_rule, obj_files);
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

auto find_glsl_files(const std::string& directory)
{
    std::vector<std::string> glsl_files;

    for (const auto& extension : {".comp", ".vert", ".frag"})
        find_files_in_dir_append(glsl_files, directory, extension, true);

    return glsl_files;
}

int main()
{
    auto builder          = NinjaBuilder("build.ninja");
    builder.compile_flags = "-std=c++20 -fcolor-diagnostics -isystem libs -Ivendor_git/glm -g -O0 -DLANG_CPP -DGLM_FORCE_RADIANS -DGLM_FORCE_DEPTH_ZERO_TO_ONE";
    builder.link_flags    = "-lSDL2 -ldl -lvulkan -lpthread -lfmt";

    const char* lib_vke = "bin/vke.a";

    auto compile_cpp_files = [&](const auto& cpp_files) {
        return map_vec(cpp_files, [&](const std::string& cpp_file) {
            return builder.compile_cpp_file(cpp_file);
        });
    };

    {
        std::vector<std::string> cpp_files;
        for (const auto& src_dir : {"src/", "libs/"})
            find_files_in_dir_append(cpp_files, src_dir, ".cpp", true);

        auto obj_files = compile_cpp_files(cpp_files);

        builder.build_static_lib(lib_vke, fmt::format("{}", fmt::join(obj_files, " ")));
    }

    builder.compile_flags += " -Isrc/";

    auto compile_sub_project = [&](const std::string& dir, const std::string& exec_name) {
        std::vector<std::string> cpp_files;
        find_files_in_dir_append(cpp_files, dir, ".cpp", true);

        std::string exec_name_raw = exec_name.substr(0, exec_name.rfind("."));

        if (auto index = exec_name_raw.rfind("/"); index != std::string::npos)
        {
            exec_name_raw.erase(0, index + 1);
        }

        auto glsl_files = find_glsl_files(dir);

        auto glsl_compiler = builder.glsl_compiler();

        for (const auto& glsl_file : glsl_files)
            glsl_compiler.compile_glsl(glsl_file);

        cpp_files.push_back(glsl_compiler.embed(fmt::format(".obj_files/{}.cpp", exec_name_raw)));

        auto obj_files = compile_cpp_files(cpp_files);
        obj_files.push_back(lib_vke);

        builder.build_executable(exec_name, fmt::format("{}", fmt::join(obj_files, " ")));
    };

    // compile_sub_project("demos/plane_and_cam/", "bin/1.out");
    // compile_sub_project("demos/portals/", "bin/portals.out");
    compile_sub_project("demos/minecraft_clone/", "bin/mc.out");
}