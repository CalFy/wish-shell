// wish — простий командний інтерпретатор (OSTEP, processes-shell)
// Етап 1: каркас — інтерактивний та пакетний режими, читання рядків.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

// Єдине повідомлення про помилку, яке вимагає завдання.
// Пишемо через write(), щоб не залежати від буферизації stdio.
void print_error() {
    const char msg[] = "An error has occurred\n";
    if (write(STDERR_FILENO, msg, sizeof(msg) - 1) < 0) {
        // якщо навіть stderr недоступний — робити нічого
    }
}

// Обробка одного рядка вводу (поки що нічого не робить).
void process_line(const std::string& line) {
    (void)line;
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
