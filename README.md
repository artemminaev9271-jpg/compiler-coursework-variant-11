# Компилятор учебного языка, вариант 11

Курсовой проект: компилятор учебного C-подобного языка в объектный файл для Linux.

## Среда

- Ubuntu / WSL
- C++17
- Flex
- GNU Bison
- LLVM/Clang 14.0.0
- target triple: `x86_64-pc-linux-gnu`

## Возможности языка

Поддерживается:

- тип `int`, который компилируется в LLVM `i64`;
- функции вида `int name(int arg) { ... }`;
- обязательная функция `int compiled_fn(int arg)`;
- арифметика: `+ - * / %`;
- сравнения: `== != < <= > >=`;
- логика: `&& || !` с short-circuit;
- `if / else`;
- `for`;
- `return`;
- тернарный оператор `cond ? a : b`.

Тернарный оператор проверяется семантически:

- `cond` должен иметь тип `bool`;
- ветви `a` и `b` должны иметь одинаковый тип;
- в LLVM IR генерируются basic blocks и `PHI` node.

## Структура проекта

```text
CMakeLists.txt
README.md

src/
  lexer.l
  parser.y
  ast.hpp / ast.cpp
  sema.hpp / sema.cpp
  codegen.hpp / codegen.cpp
  driver.cpp

runtime/
  main.c

tests/
  *.mc

scripts/
  run_tests.sh
```

## Сборка

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

## Запуск одного теста

```bash
cd build
./mini_cc ../tests/test.mc -o generated.o --emit-ir generated.ll
clang ../runtime/main.c generated.o -o app
./app
```

Ожидаемый вывод:

```text
14
```

## Полный прогон тестов

Из корня проекта:

```bash
bash scripts/run_tests.sh
```

Скрипт собирает проект, компилирует положительные тесты, линкует их с `runtime/main.c`, запускает результат и проверяет отрицательные тесты.

## Пример программы

```c
int compiled_fn(int arg) {
    int x = arg;
    int y = x > 10 ? x * 2 : x + 5;

    if (y > 20) {
        return y;
    } else {
        return y - 1;
    }
}
```

## Фазы компилятора

1. `lexer.l` разбивает исходный текст на токены.
2. `parser.y` строит AST. LLVM IR на этом этапе не используется.
3. `sema.cpp` выполняет семантический анализ: таблица символов, области видимости, типы, наличие `return`.
4. `codegen.cpp` переводит AST в LLVM IR.
5. LLVM 14 через `legacy::PassManager` и старый `addPassesToEmitFile(..., CGFT_ObjectFile)` генерирует `.o`.

## Тесты

- `01_return_literal.mc`
- `02_arithmetic.mc`
- `03_if_else.mc`
- `04_for.mc`
- `05_ternary_true.mc`
- `06_ternary_false.mc`
- `07_nested_ternary.mc`
- `08_type_error.mc`
- `09_undeclared_variable.mc`
- `10_short_circuit.mc`
- `11_function_call.mc`
- `12_sum_primes.mc`
- `test.mc`

Ошибочные тесты `08_type_error.mc` и `09_undeclared_variable.mc` должны завершаться `semantic error`.
