# Архитектура программных систем

Репозиторий теперь хранит не одну сдачу, а накапливает результаты лабораторных по одному домену: вариант `21. Система управления арендой автомобилей`.

## Навигация по лабораторным
- [Lab 01: Документирование архитектуры в Structurizr](labs/lab01/README.md)
- [Lab 02: REST API на POCO C++ Libraries](labs/lab02/README.md)

## Текущее состояние проекта
- Архитектурный контекст и C4-модель: [`workspace.dsl`](workspace.dsl), [`docs`](docs)
- Реализованный REST API: [`src`](src), [`include/car_rental`](include/car_rental)
- Интеграционные тесты: [`tests`](tests)
- OpenAPI спецификация: [`openapi.yaml`](openapi.yaml)
- Docker-артефакты: [`Dockerfile`](Dockerfile), [`docker-compose.yaml`](docker-compose.yaml)

## Быстрый старт

### Локально
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/car_rental_api --host 127.0.0.1 --port 8080 --db-path data/car_rental.db --jwt-secret local-dev-secret
```

### Docker
```bash
docker compose up --build
```

После запуска API доступен на `http://localhost:8080`.

## Что уже есть в репозитории
- Lab 01: архитектурная документация и C4-описание системы.
- Lab 02: полностью реализованный REST API с `JWT`, `SQLite`, `POCO`, OpenAPI, тестами и Docker.
