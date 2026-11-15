// wish — простий командний інтерпретатор (OSTEP, processes-shell)
//
// Можливості:
//   - інтерактивний режим (запрошення "wish> ") та пакетний (./wish file)
//   - вбудовані команди: exit, cd, path
//   - запуск програм з каталогів path через fork/execv/waitpid
//   - перенаправлення виводу: cmd args > file
//   - паралельні команди: cmd1 & cmd2 args & cmd3

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Список каталогів, у яких шукаємо програми. Початково — лише /bin.
std::vector<std::string> search_path = {"/bin"};

// Одна розібрана команда.
struct Command {
    std::vector<std::string> args;  // ім'я програми та аргументи
    std::string out_file;           // файл для '>' (порожній — без перенаправлення)
};

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

// Розбирає текст однієї команди з можливим перенаправленням.
// Повертає false, якщо синтаксис неправильний:
//   - більше одного '>'
//   - немає команди перед '>'
//   - після '>' не рівно один файл
bool parse_command(const std::string& text, Command& cmd) {
    size_t pos = text.find('>');
    if (pos == std::string::npos) {
        cmd.args = split_words(text);
        return true;
    }
    if (text.find('>', pos + 1) != std::string::npos) return false;

    // '>' може стояти без пробілів навколо: "ls>out" — тому ріжемо рядок
    cmd.args = split_words(text.substr(0, pos));
    std::vector<std::string> files = split_words(text.substr(pos + 1));
    if (cmd.args.empty() || files.size() != 1) return false;

    cmd.out_file = files[0];
    return true;
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

// Шукає виконуваний файл у каталогах search_path.
// Повертає повний шлях або порожній рядок, якщо не знайдено.
std::string find_program(const std::string& name) {
    for (const std::string& dir : search_path) {
        std::string full = dir + "/" + name;
        if (access(full.c_str(), X_OK) == 0) return full;
    }
    return "";
}

// Запускає зовнішню програму в дочірньому процесі.
// Повертає pid дитини або -1 у разі помилки.
pid_t launch(const Command& cmd) {
    const std::vector<std::string>& args = cmd.args;
    std::string prog = find_program(args[0]);
    if (prog.empty()) {
        print_error();
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        print_error();
        return -1;
    }
    if (pid == 0) {
        // Дочірній процес. Якщо є '>', підміняємо stdout і stderr файлом
        // (файл створюється або перезаписується).
        if (!cmd.out_file.empty()) {
            int fd = open(cmd.out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                print_error();
                _exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        // Готуємо argv у форматі, який чекає execv()
        std::vector<char*> argv;
        for (const std::string& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);

        execv(prog.c_str(), argv.data());
        // сюди потрапляємо лише якщо execv() не вдався
        print_error();
        _exit(1);
    }
    return pid;  // батьківський процес
}

// Обробка одного рядка вводу.
// Рядок ділимо за '&' на окремі команди. Усі зовнішні команди запускаємо
// одразу (не чекаючи одна одну), а потім чекаємо завершення всіх дітей.
void process_line(const std::string& line) {
    std::vector<pid_t> children;

    size_t start = 0;
    while (true) {
        size_t amp = line.find('&', start);
        std::string part = (amp == std::string::npos)
                               ? line.substr(start)
                               : line.substr(start, amp - start);

        Command cmd;
        if (!parse_command(part, cmd)) {
            print_error();
        } else if (!cmd.args.empty()) {  // порожня частина ("&" в кінці) — пропускаємо
            if (!run_builtin(cmd.args)) {
                pid_t pid = launch(cmd);
                if (pid > 0) children.push_back(pid);
            }
        }

        if (amp == std::string::npos) break;
        start = amp + 1;
    }

    // Чекаємо, поки завершаться всі запущені паралельно процеси
    for (pid_t pid : children) waitpid(pid, nullptr, 0);
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
