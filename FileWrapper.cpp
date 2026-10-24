#include "FileWrapper.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <utility>

namespace help {
    void close(int& fd) noexcept {
        if (fd == -1) return;

        int res = ::close(fd);
        fd = -1;
        if (res == -1 && errno != EINTR) {
            std::string error_msg = std::system_category().message(errno);
            std::cerr << "close() failed: " << error_msg << '\n';
        }
    }
}

File File::open(const std::filesystem::path& p, int flags, mode_t mode) {
    int fd;
    do {
        fd = ::open(p.c_str(), flags | O_CLOEXEC, mode);
    } while (fd == -1 && errno == EINTR);

    if (fd == -1) {
        throw std::system_error(errno, std::system_category(),
                                "open: " + p.string());
    }

    return File(fd);
}

std::expected<File, std::error_code> File::try_open(const std::filesystem::path& p, int flags, mode_t mode) noexcept {
    int fd;
    do {
        fd = ::open(p.c_str(), flags | O_CLOEXEC, mode);
    } while (fd == -1 && errno == EINTR);

    if (fd == -1)
        return std::unexpected(std::error_code(errno, std::system_category()));

    return File(fd);
}

File::File(File&& other) noexcept
    : fd_(std::exchange(other.fd_, -1))
{}

File& File::operator=(File&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = std::exchange(other.fd_, -1);
    }

    return *this;
}

File::~File() {
    close();
}

void File::close() noexcept {
    help::close(fd_);
}

void File::swap(File& other) noexcept {
    std::swap(fd_, other.fd_);
}

[[nodiscard]] bool File::is_open() const noexcept {
    return fd_ >= 0;
}

std::size_t File::read(std::span<std::byte> buf) const {
    ssize_t n;
    do {
        n = ::read(fd_, buf.data(), buf.size());
    } while (n == -1 && errno == EINTR);

    if (n == -1)
        throw std::system_error{errno, std::system_category(), "read from file"};

    return static_cast<std::size_t>(n);
}

void File::write_all(std::span<const std::byte> data) const {
    std::size_t written = 0;
    while (written < data.size()) {
        ssize_t n = write(fd_, data.data() + written, data.size() - written);
        if (n == -1) {
            if (errno == EINTR) continue;
            throw std::system_error{errno, std::system_category(), "write to file"};
        }
        written += static_cast<std::size_t>(n);
    }
}

[[nodiscard]] std::string File::read_all() const {
    struct stat st{};
    if (::fstat(fd_, &st) == -1) {
        throw std::system_error{errno, std::system_category(), "read_all: fstat"};
    }

    if (st.st_size < 0) {
        throw std::runtime_error("read_all: invalid file size");
    }
    const auto file_size = static_cast<std::size_t>(st.st_size);

    std::string result;
    result.resize(file_size);

    std::size_t total_read = 0;
    while (total_read < file_size) {
        ssize_t n = pread(fd_, result.data() + total_read, file_size - total_read,
                            static_cast<off_t>(total_read));
        if (n == -1) {
            if (errno == EINTR) continue;
            throw std::system_error{errno, std::system_category(), "read_all: pread"};
        }
        if (n == 0) {
            result.resize(total_read);
            break;
        }
        total_read += static_cast<std::size_t>(n);
    }

    return result;
}

void File::reopen(const std::filesystem::path& p, int flags) {
    int new_fd;
    do {
        new_fd = ::open(p.c_str(), flags | O_CLOEXEC);
    } while (new_fd == -1 && errno == EINTR);

    if (new_fd == -1) {
        throw std::system_error{errno, std::system_category(), "reopen: open"};
    }

    int old_fd = std::exchange(fd_, new_fd);
    if (old_fd >= 0) {
        help::close(old_fd);
    }
}
