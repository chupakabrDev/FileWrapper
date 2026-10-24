#include "FileWrapper.hpp"

#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace {

std::span<const std::byte> as_bytes(std::string_view s) noexcept {
    return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
}

void hr(const char* title) {
    std::cout << "\n==== " << title << " ====\n";
}

} // namespace

int main() {
    const std::filesystem::path path = "/tmp/filewrapper_demo.txt";
    const std::string payload = "Hello, FileWrapper!\nВторая строка.\n";

    // 1. Создаём/перезаписываем файл и пишем туда строку
    hr("1. open(O_WRONLY|O_CREAT|O_TRUNC) + write_all");
    try {
        File f = File::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        std::cout << "is_open: " << std::boolalpha << f.is_open() << '\n';
        f.write_all(as_bytes(payload));
        std::cout << "записано " << payload.size() << " байт\n";
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
    // деструктор f закрывает fd

    // 2. Переоткрываем read-only и читаем целиком через read_all (pread)
    hr("2. reopen(O_RDONLY) + read_all");
    try {
        File f = File::open(path, O_RDONLY);
        std::string all = f.read_all();
        std::cout << "read_all вернул " << all.size() << " байт:\n---\n"
                  << all << "---\n";
        std::cout << "payload совпадает: "
                  << std::boolalpha << (all == payload) << '\n';
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }

    // 3. Читаем первые N байт через read(span) — позиция сдвигается
    hr("3. read(span) — последовательное чтение");
    try {
        File f = File::open(path, O_RDONLY);
        std::vector<std::byte> buf(10);
        std::size_t n = f.read(std::span<std::byte>(buf));
        std::string chunk(reinterpret_cast<char*>(buf.data()), n);
        std::cout << "прочитано " << n << " байт: \"" << chunk << "\"\n";

        n = f.read(std::span<std::byte>(buf));
        chunk.assign(reinterpret_cast<char*>(buf.data()), n);
        std::cout << "следующие " << n << " байт: \"" << chunk << "\"\n";
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }

    // 4. try_open на несуществующем файле — без исключений
    hr("4. try_open на отсутствующем файле");
    auto res = File::try_open("/tmp/filewrapper_does_not_exist_xyz", O_RDONLY);
    if (!res) {
        std::cout << "ожидаемая ошибка: " << res.error().message()
                  << " (errno=" << res.error().value() << ")\n";
    } else {
        std::cerr << "неожиданно открылся файл!\n";
        return 1;
    }

    // 5. try_open на существующем — success-ветка
    hr("5. try_open на существующем файле");
    if (auto ok = File::try_open(path, O_RDONLY)) {
        std::cout << "успешно, is_open=" << ok->is_open()
                  << ", size=" << ok->read_all().size() << '\n';
    } else {
        std::cerr << "ERROR: " << ok.error().message() << '\n';
        return 1;
    }

    // 6. Move-семантика: владение переходит, источник пуст
    hr("6. move-конструктор и move-assignment");
    File a = File::open(path, O_RDONLY);
    File b = std::move(a);
    std::cout << "после move-ctor: a.is_open=" << a.is_open()
              << ", b.is_open=" << b.is_open() << '\n';

    File c = File::open(path, O_RDONLY);
    c = std::move(b);
    std::cout << "после move-assign: b.is_open=" << b.is_open()
              << ", c.is_open=" << c.is_open() << '\n';

    // 7. reopen: старый fd должен закрыться, новый — открыться
    hr("7. reopen с другими флагами");
    try {
        File f = File::open(path, O_RDONLY);
        f.reopen(path, O_RDWR | O_APPEND);
        f.write_all(as_bytes("APPENDED"));
        std::cout << "дописали через reopen(O_RDWR)\n";
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }

    // 8. Читаем то, что получилось после reopen+write
    hr("8. финальное содержимое файла");
    {
        File f = File::open(path, O_RDONLY);
        std::cout << f.read_all() << '\n';
    }

    // 9. Обработка ошибок open() через исключение
    hr("9. open() на отсутствующем файле → исключение");
    try {
        File f = File::open("/tmp/nope_no_such_file_zzz", O_RDONLY);
        (void)f;
        std::cerr << "не должно было сюда попасть\n";
    } catch (const std::system_error& e) {
        std::cout << "поймано system_error: " << e.what()
                  << " (code=" << e.code().value() << ")\n";
    }

    // Убираем за собой
    std::filesystem::remove(path);
    std::cout << "\nфайл " << path << " удалён\n";
    return 0;
}