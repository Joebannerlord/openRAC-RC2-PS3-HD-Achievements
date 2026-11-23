#include "toast_font.hpp"

#include <windows.h>
#include <dwrite_3.h>

#include <cstdint>
#include <fstream>
#include <vector>

namespace achievements::runtime {
namespace {

constexpr std::uintmax_t kMaximumFontBytes = 2U * 1024U * 1024U;

bool read_file(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size == 0 || size > kMaximumFontBytes) return false;

    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    bytes.resize(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return input && input.gcount() == static_cast<std::streamsize>(bytes.size());
}

} // namespace

struct ToastFont::State {
    IDWriteFactory* factory = nullptr;
    IDWriteInMemoryFontFileLoader* loader = nullptr;
    IDWriteFontCollection* collection = nullptr;
    bool loader_registered = false;

    ~State() {
        // The collection may retain references through the loader, so release it first.
        if (collection != nullptr) collection->Release();
        if (loader_registered && factory != nullptr && loader != nullptr) {
            factory->UnregisterFontFileLoader(loader);
        }
        if (loader != nullptr) loader->Release();
        if (factory != nullptr) factory->Release();
    }
};

ToastFont::ToastFont(const std::filesystem::path& file) : state_(std::make_unique<State>()) {
    std::vector<std::uint8_t> packed;
    if (!read_file(file, packed)) return;

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(&state_->factory)))) {
        state_->factory = nullptr;
        return;
    }

    IDWriteFactory5* factory5 = nullptr;
    if (FAILED(state_->factory->QueryInterface(IID_PPV_ARGS(&factory5)))) return;

    IDWriteFontFileStream* unpacked = nullptr;
    IDWriteFontFile* font_file = nullptr;
    IDWriteFontFaceReference* face_reference = nullptr;
    IDWriteFontSetBuilder* builder = nullptr;
    IDWriteFontSet* font_set = nullptr;
    IDWriteFontCollection1* collection = nullptr;

    const DWRITE_CONTAINER_TYPE container = factory5->AnalyzeContainerType(packed.data(),
                                                                             static_cast<UINT32>(packed.size()));
    HRESULT result = container == DWRITE_CONTAINER_TYPE_WOFF2
        ? factory5->UnpackFontFile(container, packed.data(), static_cast<UINT32>(packed.size()), &unpacked)
        : E_INVALIDARG;
    if (SUCCEEDED(result)) result = factory5->CreateInMemoryFontFileLoader(&state_->loader);
    if (SUCCEEDED(result)) result = state_->factory->RegisterFontFileLoader(state_->loader);
    if (SUCCEEDED(result)) state_->loader_registered = true;

    UINT64 unpacked_size = 0;
    const void* fragment = nullptr;
    void* fragment_context = nullptr;
    if (SUCCEEDED(result)) result = unpacked->GetFileSize(&unpacked_size);
    if (SUCCEEDED(result) && (unpacked_size == 0 || unpacked_size > kMaximumFontBytes)) result = E_FAIL;
    if (SUCCEEDED(result)) result = unpacked->ReadFileFragment(&fragment, 0, unpacked_size, &fragment_context);
    if (SUCCEEDED(result)) {
        // A null owner asks DirectWrite to copy this transient unpacked fragment.
        result = state_->loader->CreateInMemoryFontFileReference(
            state_->factory, fragment, static_cast<UINT32>(unpacked_size), nullptr, &font_file);
        unpacked->ReleaseFileFragment(fragment_context);
    }
    if (SUCCEEDED(result)) {
        result = factory5->CreateFontFaceReference(font_file, 0, DWRITE_FONT_SIMULATIONS_NONE,
                                                   &face_reference);
    }
    if (SUCCEEDED(result)) result = factory5->CreateFontSetBuilder(&builder);
    if (SUCCEEDED(result)) result = builder->AddFontFaceReference(face_reference);
    if (SUCCEEDED(result)) result = builder->CreateFontSet(&font_set);
    if (SUCCEEDED(result)) result = factory5->CreateFontCollectionFromFontSet(font_set, &collection);
    if (SUCCEEDED(result)) {
        state_->collection = collection;
        collection = nullptr;
    }

    if (collection != nullptr) collection->Release();
    if (font_set != nullptr) font_set->Release();
    if (builder != nullptr) builder->Release();
    if (face_reference != nullptr) face_reference->Release();
    if (font_file != nullptr) font_file->Release();
    if (unpacked != nullptr) unpacked->Release();
    factory5->Release();
}

ToastFont::~ToastFont() = default;

IDWriteFactory* ToastFont::factory() const {
    return state_->factory;
}

IDWriteFontCollection* ToastFont::collection() const {
    return state_->collection;
}

} // namespace achievements::runtime
