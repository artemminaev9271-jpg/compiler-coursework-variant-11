from pathlib import Path

from docx import Document
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.shared import Cm, Pt


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "Отчёт_мини_компилятор_вариант_11.docx"
OUT = ROOT / "Отчёт_мини_компилятор_вариант_11_с_правками.docx"


def format_paragraph(paragraph) -> None:
    paragraph.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
    paragraph.paragraph_format.first_line_indent = Cm(1.25)
    paragraph.paragraph_format.space_after = Pt(6)
    for run in paragraph.runs:
        run.font.name = "Times New Roman"
        run.font.size = Pt(12)


def insert_after(paragraph, text: str):
    new_p = paragraph.insert_paragraph_before("")
    paragraph._p.addnext(new_p._p)
    new_p.add_run(text)
    format_paragraph(new_p)
    return new_p


def find_heading(doc: Document, title: str):
    for paragraph in doc.paragraphs:
        if paragraph.text.strip() == title:
            return paragraph
    raise RuntimeError(f"Heading not found: {title}")


def find_heading_occurrence(doc: Document, title: str, occurrence: int):
    seen = 0
    for paragraph in doc.paragraphs:
        if paragraph.text.strip() == title:
            seen += 1
            if seen == occurrence:
                return paragraph
    raise RuntimeError(f"Heading occurrence not found: {title} #{occurrence}")


def remove_paragraph(paragraph) -> None:
    element = paragraph._element
    element.getparent().remove(element)
    paragraph._p = paragraph._element = None


def already_contains(doc: Document, marker: str) -> bool:
    text = "\n".join(paragraph.text for paragraph in doc.paragraphs)
    return marker in text


def insert_table_after(paragraph, title: str, command: str, expected: str):
    doc = paragraph._parent
    table = doc.add_table(rows=3, cols=1, width=Cm(16))
    table.style = "Table Grid"
    table.cell(0, 0).text = f"МЕСТО ДЛЯ СКРИНШОТА: {title}"
    table.cell(1, 0).text = f"Что должно быть видно: {expected}"
    table.cell(2, 0).text = f"Команда/действие: {command}"
    paragraph._p.addnext(table._tbl)
    return table


def add_sum_primes_to_report(doc: Document) -> None:
    full_text = "\n".join(paragraph.text for paragraph in doc.paragraphs)
    full_text += "\n".join(cell.text for table in doc.tables for row in table.rows for cell in row.cells)
    if "12_sum_primes.mc" in full_text and "Сумма простых чисел" in full_text:
        return

    tests_heading = find_heading_occurrence(doc, "9. Тестовые наборы программ", 2)
    insert_after(tests_heading, "Дополнительно добавлен тест 12_sum_primes.mc. Он содержит функцию is_prime для проверки простоты числа и функцию compiled_fn, которая суммирует все простые числа от 2 до переданного аргумента.")

    results_heading = find_heading_occurrence(doc, "11. Результаты тестирования", 2)
    p = insert_after(results_heading, "Для проверки более содержательной программы добавлен тест суммы простых чисел. При стандартном запуске runtime вызывает compiled_fn(10), поэтому ожидаемый результат равен 17: 2 + 3 + 5 + 7.")
    insert_table_after(
        p,
        "Сумма простых чисел",
        "./mini_cc ../tests/12_sum_primes.mc -o primes.o --emit-ir primes.ll\nclang ../runtime/main.c primes.o -o primes_app\n./primes_app",
        "Вывод 17 для суммы простых чисел 2 + 3 + 5 + 7.",
    )

    for paragraph in doc.paragraphs:
        if "11_function_call: 15" in paragraph.text and "12_sum_primes: 17" not in paragraph.text:
            paragraph.text = paragraph.text.replace("11_function_call: 15\ntest: 14", "11_function_call: 15\n12_sum_primes: 17\ntest: 14")
            for run in paragraph.runs:
                run.font.name = "Courier New"
                run.font.size = Pt(8)


def patch_title(doc: Document) -> None:
    for paragraph in doc.paragraphs:
        text = paragraph.text
        if "Выполнила" in text or "Выполнил" in text:
            if "студент" in text and ("Минаева" in text or "Минаев" in text or "Матвеев" in text):
                paragraph.text = "Выполнил\nстудент 3-го курса\nгр. 0483-04\nМинаев А.Н."
                paragraph.alignment = WD_ALIGN_PARAGRAPH.RIGHT
                for run in paragraph.runs:
                    run.font.name = "Times New Roman"
                    run.font.size = Pt(12)
                return
    raise RuntimeError("Title author block not found")


def add_theory(doc: Document) -> None:
    blocks = {
        "2. Лексическая спецификация": [
            "Лексический анализ — это первая фаза компиляции, на которой непрерывный текст программы разбивается на отдельные смысловые единицы: ключевые слова, идентификаторы, литералы, операторы и разделители. На вход этой фазы поступает исходный файл .mc, а на выходе получается поток токенов для синтаксического анализатора.",
            "На практике лексический анализ реализуют конечным автоматом или генератором лексеров. В данном проекте используется Flex — генератор лексических анализаторов, который по регулярным выражениям создаёт код сканера.",
            "В данной работе лексер находится в src/lexer.l. Он распознаёт токены учебного языка, пропускает комментарии и пробелы, а также передаёт координаты строки и столбца для последующей диагностики ошибок.",
        ],
        "3. Диаграмма и описание правил грамматики": [
            "Синтаксический анализ — это фаза, которая проверяет, соответствует ли поток токенов правилам грамматики языка. На вход поступают токены от лексера, а на выходе формируется абстрактное синтаксическое дерево (AST), то есть древовидное представление программы.",
            "На практике синтаксический анализ может быть написан вручную или сгенерирован специальной программой. В проекте используется GNU Bison — генератор синтаксических анализаторов, который по файлу parser.y создаёт parser.",
            "Важное архитектурное решение состоит в том, что parser.y не генерирует LLVM IR. LLVM IR, то есть промежуточное представление LLVM, строится только на этапе генерации кода после семантической проверки.",
        ],
        "4. Листинг кода лексического и синтаксического анализаторов": [
            "Передняя часть компилятора, или front-end, отвечает за чтение исходного текста и построение AST. Входом является текст программы, а выходом — дерево функций, операторов и выражений.",
            "В проекте lexer.l задаёт правила распознавания токенов, а parser.y задаёт правила грамматики и действия по созданию узлов AST. Такой подход отделяет описание языка от генерации машинного кода.",
        ],
        "5. Описание семантических правил языка": [
            "Семантический анализ проверяет смысловую корректность программы после синтаксического разбора. Синтаксически правильная программа может быть некорректной по смыслу: например, использовать необъявленную переменную или вернуть значение неправильного типа.",
            "На вход семантического анализатора поступает AST. На выходе получается либо подтверждение корректности программы, либо список диагностических сообщений. Кроме того, выражения получают вычисленные типы, которые затем используются генератором кода.",
            "В проекте семантическая фаза реализована через таблицу функций и стек таблиц символов для локальных областей видимости. Это позволяет проверять параметры функций, локальные переменные и вложенные блоки.",
        ],
        "6. Листинг кода семантического анализатора": [
            "Код семантического анализатора выполняет обход AST. Объявление переменной добавляет имя в текущую область видимости, обращение к переменной ищет имя в стеке областей, а выражения проверяются по правилам типизации.",
            "Если на этом этапе найдена ошибка, дальнейшая генерация LLVM IR не выполняется. Это защищает генератор кода от некорректного дерева и делает сообщения об ошибках более понятными.",
        ],
        "7. Перевод AST в LLVM IR и объектный файл": [
            "Генерация кода преобразует проверенное AST в более низкоуровневое представление. В работе сначала строится LLVM IR, а затем LLVM переводит его в объектный файл .o для архитектуры x86_64 Linux.",
            "На вход генератора кода поступает AST после успешной семантической проверки. На выходе создаются файл .ll с промежуточным представлением и объектный файл .o, который затем связывается с runtime/main.c.",
            "Runtime, или исполняющая обвязка, — это небольшой C-файл, который вызывает функцию compiled_fn. В проекте runtime/main.c нужен для запуска скомпилированной функции как обычной программы.",
        ],
        "8. Листинг кода генератора кода": [
            "Генератор кода реализует обход AST и создание инструкций LLVM. Литералы превращаются в константы, переменные работают через alloca, load и store, а арифметические операции превращаются в соответствующие инструкции LLVM IR.",
            "Логические операции реализованы с коротким замыканием. Короткое замыкание означает, что правая часть выражения && или || вычисляется только тогда, когда её значение может повлиять на результат.",
            "Тернарный оператор варианта 11 реализован через basic blocks, то есть базовые блоки, и PHI node, то есть PHI-узел выбора значения в зависимости от выполненной ветви.",
        ],
        "9. Тестовые наборы программ": [
            "Тестирование компилятора проверяет весь конвейер: исходный файл .mc, лексический анализ, синтаксический анализ, семантическую проверку, генерацию LLVM IR, создание объектного файла и запуск программы.",
            "Положительные тесты должны успешно компилироваться и выдавать ожидаемое число. Отрицательные тесты должны завершаться semantic error, то есть ошибкой семантического анализа.",
        ],
        "10. Описание процесса использования компилятора": [
            "Использование компилятора состоит из трёх этапов: сборка mini_cc, компиляция учебной программы в объектный файл и связывание объектного файла с runtime.",
            "CMake — это система сборки, которая находит Flex, Bison и LLVM 14, после чего создаёт исполняемый файл mini_cc. В проекте используется стандартный сценарий mkdir build, cmake .. и cmake --build .",
        ],
        "11. Результаты тестирования": [
            "Результаты тестирования показывают, что отдельные фазы работают согласованно. Положительные тесты проходят полный путь до запуска исполняемого файла, а отрицательные останавливаются на семантической фазе.",
            "Особое внимание уделено тернарному оператору и логике с коротким замыканием, потому что именно эти элементы демонстрируют корректную работу basic blocks и PHI node в LLVM IR.",
        ],
        "12. Заключение": [
            "Теоретически компилятор является последовательностью преобразований: текст программы превращается в токены, затем в AST, затем в проверенное дерево, промежуточное представление и объектный код.",
            "Такое разделение фаз упрощает проверку и отладку, потому что каждая часть имеет понятные входные и выходные данные.",
        ],
    }

    theory_texts = {text for paragraphs in blocks.values() for text in paragraphs}

    for paragraph in list(doc.paragraphs):
        if paragraph.text.strip() in theory_texts:
            remove_paragraph(paragraph)

    for title, paragraphs in blocks.items():
        # The first occurrence is in the table of contents; the second one is the actual chapter.
        heading = find_heading_occurrence(doc, title, 2)
        cursor = heading
        for text in paragraphs:
            cursor = insert_after(cursor, text)


def main() -> None:
    doc = Document(SRC)
    before_shapes = len(doc.inline_shapes)
    patch_title(doc)
    add_theory(doc)
    add_sum_primes_to_report(doc)
    doc.save(OUT)
    check = Document(OUT)
    print(OUT)
    print(f"inline_shapes_before={before_shapes}")
    print(f"inline_shapes_after={len(check.inline_shapes)}")


if __name__ == "__main__":
    main()
