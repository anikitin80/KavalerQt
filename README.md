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

## Настройки

Настройки хранятся в реестре: `HKCU\Software\Anatoli Nikitin\Kavaler`.

| Ключ | Что это |
|------|---------|
| `Database` | путь к базе SQLite |
| `DictionaryRU`, `DictionaryBY` | словари профессий для импорта (кнопка «Словари...» в окне импорта) |
| `YandexApiKey` | API-ключ Яндекс.Переводчика (меню «Правка → API-ключ Яндекса...») |

Ключ Яндекса можно задать и переменной окружения `KAVALER_YANDEX_API_KEY`, она важнее
настройки. В коде ключ не хранится.
