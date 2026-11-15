// wish — простий командний інтерпретатор (OSTEP, processes-shell)
// Етап 2: розбиття рядка на слова, вбудовані команди exit, cd, path.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

// Список каталогів, у яких шукаємо програми. Початково — лише /bin.
std::vector<std::string> search_path = {"/bin"};

// Єдине повідомлення про помилку, яке вимагає завдання.
// Пишемо через write(), щоб не залежати від буферизації stdio.
void print_error() {
    const char msg[] = "An error has occurred\n";
    if (write(STDERR_FILENO, msg, sizeof(msg) - 1) < 0) {
        // якщо навіть stderr недоступний — робити нічого
    }
}

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Розбиває рядок на слова за пробільними символами
// (кількість пробілів між словами може бути довільною).
std::vector<std::string> split_words(const std::string& s) {
    std::vector<std::string> words;
    std::string cur;
    for (char c : s) {
        if (is_space(c)) {
            if (!cur.empty()) {
                words.push_back(cur);
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) words.push_back(cur);
    return words;
}

// Виконує вбудовану команду. Повертає true, якщо команда була вбудованою.
bool run_builtin(const std::vector<std::string>& args) {
    const std::string& cmd = args[0];

    if (cmd == "exit") {
        // exit не приймає аргументів
        if (args.size() != 1) print_error();
        else exit(0);
        return true;
    }
    if (cmd == "cd") {
        // cd приймає рівно один аргумент; помилка chdir() теж помилка
        if (args.size() != 2 || chdir(args[1].c_str()) != 0) print_error();
        return true;
    }
    if (cmd == "path") {
        // path повністю замінює список каталогів (може бути і порожнім)
        search_path.assign(args.begin() + 1, args.end());
        return true;
    }
    return false;
}

// Обробка одного рядка вводу.
void process_line(const std::string& line) {
    std::vector<std::string> args = split_words(line);
    if (args.empty()) return;  // порожній рядок — нічого не робимо
    if (run_builtin(args)) return;
    // зовнішні програми — на наступному етапі
}

int main(int argc, char* argv[]) {
    FILE* in = stdin;
    bool interactive = true;

    // Більше одного файлу — помилка.
    if (argc > 2) {
        print_error();
        exit(1);
    }
    // Пакетний режим: команди читаються з файлу, запрошення не друкуємо.
    if (argc == 2) {
        in = fopen(argv[1], "r");
        if (in == nullptr) {
            print_error();
            exit(1);
        }
        interactive = false;
    }

    char* buf = nullptr;
    size_t cap = 0;
    while (true) {
        if (interactive) {
            printf("wish> ");
            fflush(stdout);
        }
        // getline() сам виділяє пам'ять під рядок будь-якої довжини.
        ssize_t n = getline(&buf, &cap, in);
        if (n == -1) break;  // EOF — виходимо з кодом 0
        process_line(buf);
    }

    free(buf);
    if (in != stdin) fclose(in);
    return 0;
}
