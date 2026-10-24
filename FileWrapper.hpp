#pragma once
#include <expected>
#include <filesystem>

class File {
public:
    static File open(const std::filesystem::path& p, int flags, mode_t mode = 0600);

    static std::expected<File, std::error_code> try_open(const std::filesystem::path& p,
                                                         int flags, mode_t mode = 0600) noexcept;

    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&&) noexcept;
    File& operator=(File&&) noexcept;
    ~File();

    void swap(File& other) noexcept;
    [[nodiscard]] bool is_open() const noexcept;

    std::size_t read(std::span<std::byte> buf) const;
    void write_all(std::span<const std::byte> data) const;
    [[nodiscard]] std::string read_all() const;
    void reopen(const std::filesystem::path& p, int flags);
    void close() noexcept;

private:
    int fd_ = -1;

    explicit File(int fd) : fd_(fd) {
    }
};
