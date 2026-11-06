# Кавалер (KavalerQt)

Каталог государственных наград Беларуси: база награждений в SQLite, интерфейс на Qt 6.
Импорт указов из Word (docx) и PDF, перевод с белорусского через Яндекс.Переводчик.

## Сборка

Нужны:

- Visual Studio 2022 (MSVC, x64);
- Qt 6.8 (`msvc2022_64`) с модулями Widgets, Sql, Network, PrintSupport;
- CMake 3.16+;
- PDFTron PDFNet SDK. Он не хранится в репозитории: положите его в папку `PDFLib`
  рядом с `CMakeLists.txt`:

  ```
  PDFLib/
    Headers/        заголовки SDK (Common/, PDF/, SDF/, ...)
    Lib/x64/        PDFNetC.lib, PDFNetC.dll
  ```

```bash
cmake -S . -B build -DQt6_DIR=C:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6
cmake --build build --config Release
```

`PDFNetC.dll` копируется рядом с `Kavaler.exe` после сборки. Для запуска на другой
машине Qt-библиотеки разворачиваются через `windeployqt`.

## Тесты

Тесты парсера указов (Qt Test) собираются вместе с проектом. Отключить их можно опцией
`-DKAVALER_BUILD_TESTS=OFF`. Запуск:

```bash
ctest --test-dir build -C Release --output-on-failure
```

| Тест | Что проверяет |
|------|---------------|
| `tst_textutil` | даты в заголовках указов, строковые утилиты |
| `tst_nameconverter` | ФИО из дательного/винительного падежа в именительный (рус./бел.) |
| `tst_professionconverter` | должности по словарям профессий, новые слова |
| `tst_documentparser` | награды, звания, «посмертно», форматы списков, сведения о группе |
| `tst_decrees` | реальные указы из `tests/data/decrees` против записей каталога |
| `tst_docxtext` | чтение docx (нужен `tar.exe` из Windows 10+) |

В `tests/data/decrees` лежат тексты указов (`<id>.txt`, `\v` означает разрыв строки
Word) и ожидаемые записи (`<id>.expected.tsv`) в том виде, в котором они хранятся в
каталоге. Если в колонке `known_issue` записано описание ошибки, тест ждёт
расхождения (`XFAIL`). Когда парсер исправят, тест сообщит `XPASS`, и пометку нужно
будет убрать. Словари профессий и таблица наград для тестов скопированы в `tests/data`,
настройки пользователя тесты не читают.

## Настройки

Настройки хранятся в реестре: `HKCU\Software\Anatoli Nikitin\Kavaler`.

| Ключ | Что это |
|------|---------|
| `Database` | путь к базе SQLite |
| `DictionaryRU`, `DictionaryBY` | словари профессий для импорта (кнопка «Словари...» в окне импорта) |
| `YandexApiKey` | API-ключ Яндекс.Переводчика (меню «Правка → API-ключ Яндекса...») |

Ключ Яндекса можно задать и переменной окружения `KAVALER_YANDEX_API_KEY`, она важнее
настройки. В коде ключ не хранится.
