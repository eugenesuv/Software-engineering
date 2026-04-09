# Архитектура программных систем

Репозиторий хранит последовательные лабораторные по одному домену: вариант `21. Система управления арендой автомобилей`.

## Навигация по лабораторным
- [Lab 01: Документирование архитектуры в Structurizr](labs/lab01/README.md)
- [Lab 02: REST API на POCO C++ Libraries](labs/lab02/README.md)
- [Lab 03: Проектирование и оптимизация реляционной БД](labs/lab03/README.md)

## Текущее состояние проекта
- Архитектурный контекст и C4-модель: `workspace.dsl`, `docs`
- Реализованный REST API: `src`, `include/car_rental`
- OpenAPI спецификация: `openapi.yaml`
- Интеграционные тесты: `tests`
- SQL-артефакты Lab 03: `schema.sql`, `data.sql`, `queries.sql`, `optimization.md`
- Docker-артефакты: `Dockerfile`, `docker-compose.yaml`

## Быстрый старт

### Локально
```bash
createdb car_rental
psql postgresql://postgres:postgres@127.0.0.1:5432/car_rental -f schema.sql
psql postgresql://postgres:postgres@127.0.0.1:5432/car_rental -f data.sql

cmake -S . -B build
cmake --build build
./build/car_rental_api \
  --host 127.0.0.1 \
  --port 8080 \
  --db-url postgresql://postgres:postgres@127.0.0.1:5432/car_rental \
  --jwt-secret local-dev-secret
```

API также умеет брать строку подключения из `DATABASE_URL`.

Seed-менеджер создаётся при старте приложения, если пользователя `manager` ещё нет:
- login: `manager`
- password: `Manager123!`

### Интеграционные тесты
```bash
export CAR_RENTAL_TEST_ADMIN_URL=postgresql://postgres:postgres@127.0.0.1:5432/postgres
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

`CAR_RENTAL_TEST_ADMIN_URL` указывает на административную PostgreSQL-базу, из которой тесты создают временную БД на каждый сценарий.

### Docker
```bash
docker compose up --build
```

`docker-compose.yaml` поднимает `postgres`, применяет `schema.sql` и `data.sql`, затем запускает `api`, подключённый через `DATABASE_URL`.

После запуска API доступен на `http://localhost:8080`.

## Что уже есть в репозитории
- Lab 01: архитектурная документация и C4-описание системы.
- Lab 02: REST API, JWT, OpenAPI, тесты и Docker.
- Lab 03: PostgreSQL-схема, данные, SQL-запросы, индексы и оптимизация.
