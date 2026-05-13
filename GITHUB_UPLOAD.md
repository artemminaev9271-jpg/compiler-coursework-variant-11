# Загрузка проекта на GitHub

Команды выполнять из WSL Ubuntu.

## 1. Перейти в папку проекта

```bash
cd "/mnt/c/Users/minae/Documents/New project"
```

## 2. Инициализировать Git

```bash
git init
git add .
git status
git commit -m "Initial compiler project"
```

## 3. Создать пустой репозиторий на GitHub

На сайте GitHub создать новый репозиторий без README, без `.gitignore` и без license, потому что эти файлы уже есть в проекте.

Пример имени:

```text
compiler-coursework-variant-11
```

## 4. Подключить удалённый репозиторий

Заменить `YOUR_LOGIN` на свой логин GitHub:

```bash
git branch -M main
git remote add origin https://github.com/YOUR_LOGIN/compiler-coursework-variant-11.git
git push -u origin main
```

## 5. Проверить на GitHub

После push на странице репозитория должны быть видны:

- `README.md`
- `CMakeLists.txt`
- `src/`
- `runtime/`
- `tests/`
- `scripts/run_tests.sh`
- `Отчёт_компилятор_вариант_11_проверенный.docx`

Папки `build/`, `.vs/`, `out/` и сгенерированные `.o`, `.ll`, `.app` не должны попасть в репозиторий, потому что они закрыты в `.gitignore`.
