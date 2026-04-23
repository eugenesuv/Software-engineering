# Архитектура программных систем

Репозиторий хранит последовательные лабораторные по одному домену: вариант `21. Система управления арендой автомобилей`.

## Навигация по лабораторным
- [Lab 01: Документирование архитектуры в Structurizr](labs/lab01/README.md)
- [Lab 02: REST API на POCO C++ Libraries](labs/lab02/README.md)
- [Lab 03: Проектирование и оптимизация реляционной БД](labs/lab03/README.md)
- [Lab 04: Проектирование и работа с MongoDB](labs/lab04/README.md)

## Текущее состояние проекта
- Архитектурный контекст и C4-модель: `workspace.dsl`, `docs`
- Реализованный REST API: `src`, `include/car_rental`
- OpenAPI спецификация: `openapi.yaml`
- Интеграционные тесты: `tests`
- SQL-артефакты Lab 03: `schema.sql`, `data.sql`, `queries.sql`, `optimization.md`
- MongoDB-артефакты Lab 04: `schema_design.md`, `data.js`, `queries.js`, `validation.js`
- Docker-артефакты: `Dockerfile`, `docker-compose.yml`
- API умеет работать с backend'ами `postgres` и `mongo`

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

### MongoDB backend
```bash
./build/car_rental_api \
  --host 127.0.0.1 \
  --port 8080 \
  --data-backend mongo \
  --mongo-url 'mongodb://127.0.0.1:27017/?replicaSet=rs0' \
  --mongo-db car_rental \
  --jwt-secret local-dev-secret
```

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

Для MongoDB-режима:
```bash
export CAR_RENTAL_TEST_BACKEND=mongo
export CAR_RENTAL_TEST_MONGO_URL='mongodb://127.0.0.1:27017/?replicaSet=rs0'
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Docker
```bash
docker compose up --build
```

`docker-compose.yml` поднимает MongoDB replica set `rs0`, применяет `validation.js` и `data.js`, затем запускает `api` в режиме `DATA_BACKEND=mongo`.

После запуска API доступен на `http://localhost:8080`.

## Что уже есть в репозитории
- Lab 01: архитектурная документация и C4-описание системы.
- Lab 02: REST API, JWT, OpenAPI, тесты и Docker.
- Lab 03: PostgreSQL-схема, данные, SQL-запросы, индексы и оптимизация.
- Lab 04: MongoDB document model, validation, seed-данные, dual-backend API и MongoDB Docker stack.
