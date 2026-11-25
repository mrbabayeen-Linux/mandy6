#pragma once

// A native pipeline description and catalog for the vertex and mesh pixel readback tests. The source
// programs are built by the tests. The extra row names a container whose actual HASH does not match.
#include "d3d12_test_shaders.h"
#include "../src/native_pso.h"

namespace d3d12test {
namespace np = leonida::native_pso;
inline constexpr np::Digest producer_digest{1}, ps_digest{2}, stale_digest{3};

// Synthetic kind-6 hash carriers. Preserve the M1.5 pixel shader's data table at byte 0x100.
inline std::vector<std::uint8_t> native_code(std::vector<std::uint8_t> payload, std::uint32_t marker, bool short_header) {
    const std::uint32_t words[] = {0xd5800000, 0x03fe0100, marker, 0xd5800000, 0x03fe0100, 0,
                                   short_header ? 0xbf810000u : 0xd5800000u, 0x03fe0100, 0, 0xbf810000};
    if (payload.size() < sizeof(words)) payload.resize(sizeof(words));
    std::memcpy(payload.data(), words, sizeof(words));
    return payload;
}

inline bool write_native_code_index(const std::filesystem::path& library, const char* producer_stage,
                                    const std::filesystem::path& producer_module, const std::vector<std::uint8_t>& producer,
                                    const std::filesystem::path& pixel_module, const std::vector<std::uint8_t>& pixel,
                                    bool pixel_reads_code) {
    const auto gfx = library / "fspx_gfx";
    std::filesystem::create_directories(gfx);
    std::filesystem::copy_file(producer_module, gfx / "producer.spv", std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(pixel_module, gfx / "pixel.spv", std::filesystem::copy_options::overwrite_existing);
    std::ofstream index(gfx / "index.json", std::ios::binary);
    index << R"({"producer":{"stage":")" << producer_stage << R"(","fspx_code_sha256":")" << leonida::sha256_hex(producer)
          << R"(","push_constant_bytes":144,"outputs":[0])";
    if (std::string_view(producer_stage) == "ms")
        index << R"(,"root_window":{"sgpr":16,"dword":8},"local_size":[32,1,1],"max_vertices":4,"max_primitives":2)";
    index << R"(},"pixel":{"stage":"ps","fspx_code_sha256":")" << leonida::sha256_hex(pixel)
          << R"(","push_constant_bytes":144,"inputs":[0],"outputs":[0],"uses_code_base":)"
          << (pixel_reads_code ? "true" : "false") << "}}\n";
    return index.good();
}

inline bool write_native_catalog(const std::filesystem::path& dest, const char* producer_stage,
                                 const std::vector<std::uint8_t>& producer, const std::vector<std::uint8_t>& ps) {
    std::error_code ec;
    if (!std::filesystem::create_directory(dest / "dxbc", ec)) return false;
    std::ofstream index(dest / "spirv" / "native_hashes.txt");
    index << np::HashMap::kHeader << '\n';
    const auto add = [&](const char* stage, const np::Digest& digest, const std::vector<std::uint8_t>& program) {
        const std::string name = std::string(stage) + ".dxbc";
        const auto container = shader_container(program, &digest);
        std::ofstream file(dest / "dxbc" / name, std::ios::binary);
        file.write(reinterpret_cast<const char*>(container.data()), container.size());
        index << np::hex(digest) << '\t' << stage << "\t../dxbc/" << name << '\n';
        return file.good() && index.good();
    };
    if (!add(producer_stage, producer_digest, producer) || !add("ps", ps_digest, ps)) return false;
    index << np::hex(stale_digest) << '\t' << producer_stage << "\t../dxbc/" << producer_stage << ".dxbc\n";
    return index.good();
}

inline HRESULT create_native_pso(ID3D12Device* device, ID3D12RootSignature* root, DXGI_FORMAT target,
                                 const np::Digest& producer, ID3D12PipelineState** out,
                                 std::span<const std::uint8_t> producer_code = {}, std::span<const std::uint8_t> pixel_code = {}) {
    std::array<std::uint8_t, np::kStateBHeader> state{};
    state[0x10] = static_cast<std::uint8_t>(target);
    std::memset(state.data() + 0x1c, 0xff, sizeof(UINT));
    state[0x25] = 1;
    state[0x26] = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    state[0x28] = 1;
    const auto record = [](const np::Digest& d) {
        std::array<std::uint8_t, 20> bytes{0x43};
        std::memcpy(bytes.data() + 4, d.data(), d.size());
        return bytes;
    };
    const auto v = record(producer), p = record(ps_digest);
    np::GraphicsDesc desc{};
    desc.root_signature = root;
    desc.state_b = {state.data(), state.size()};
    if (producer_code.empty() || producer != np::Digest{}) desc.stages[5].records = {v.data(), v.size()};
    if (!producer_code.empty()) desc.stages[5].code = {producer_code.data(), producer_code.size()};
    if (pixel_code.empty()) desc.stages[2].records = {p.data(), p.size()};
    else desc.stages[2].code = {pixel_code.data(), pixel_code.size()};
    using Create = HRESULT(WINAPI*)(void*, const np::GraphicsDesc*, const IID*, void**);
    return reinterpret_cast<Create>(slot(device, 57))(device, &desc, &__uuidof(ID3D12PipelineState),
                                                      reinterpret_cast<void**>(out));
}

}  // namespace d3d12test
