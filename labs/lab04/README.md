# Домашнее задание 04: Проектирование и работа с MongoDB

Вариант: `21. Система управления арендой автомобилей`.

## Что добавлено в Lab 04
- второй backend хранения на `MongoDB` без изменения HTTP API;
- runtime-переключение backend'а через `--data-backend postgres|mongo`;
- MongoDB document model для `users`, `cars`, `rentals`, `outbox_events`;
- MongoDB validation через `$jsonSchema`;
- MongoDB seed и набор CRUD/aggregation запросов;
- Docker Compose для запуска `MongoDB + API` в режиме lab04.

## Артефакты лабораторной
- [`../../schema_design.md`](../../schema_design.md) — проектирование документной модели и обоснование `embedded/references`.
- [`../../data.js`](../../data.js) — тестовые данные для MongoDB.
- [`../../queries.js`](../../queries.js) — CRUD-запросы и aggregation pipeline.
- [`../../validation.js`](../../validation.js) — создание коллекций, индексов, `$jsonSchema` и проверка невалидных вставок.
- [`../../Dockerfile`](../../Dockerfile), [`../../docker-compose.yml`](../../docker-compose.yml) — запуск API и MongoDB.

## Backend configuration

### CLI
```bash
./build/car_rental_api \
  --host 127.0.0.1 \
  --port 8080 \
  --data-backend mongo \
  --mongo-url 'mongodb://127.0.0.1:27017/?replicaSet=rs0' \
  --mongo-db car_rental \
  --jwt-secret local-dev-secret
```

### Environment variables
- `DATA_BACKEND`
- `DATABASE_URL`
- `MONGO_URL`
- `MONGO_DB_NAME`

`PostgreSQL` backend сохранён для совместимости с предыдущими лабораторными, но для lab04 основной режим — `mongo`.

## Docker
```bash
docker compose up --build
```

Что делает compose:
- поднимает `mongo` в режиме single-node replica set `rs0`;
- запускает одноразовый `mongo-init`, который выполняет `validation.js` и `data.js`;
- запускает `api` с `DATA_BACKEND=mongo`.

После запуска:
- API: `http://localhost:8080`
- MongoDB: `mongodb://localhost:27017/?replicaSet=rs0`

## Локальный запуск MongoDB-скриптов
После поднятия MongoDB:

```bash
mongosh 'mongodb://127.0.0.1:27017/car_rental?replicaSet=rs0' --file validation.js
mongosh 'mongodb://127.0.0.1:27017/car_rental?replicaSet=rs0' --file data.js
mongosh 'mongodb://127.0.0.1:27017/car_rental?replicaSet=rs0' --file queries.js
```

## Тесты
Основной режим lab04:

```bash
export CAR_RENTAL_TEST_BACKEND=mongo
export CAR_RENTAL_TEST_MONGO_URL='mongodb://127.0.0.1:27017/?replicaSet=rs0'
cmake -S ../.. -B ../../build
cmake --build ../../build
ctest --test-dir ../../build --output-on-failure
```

Регрессия PostgreSQL backend:

```bash
export CAR_RENTAL_TEST_BACKEND=postgres
export CAR_RENTAL_TEST_ADMIN_URL='postgresql://postgres:postgres@127.0.0.1:5432/postgres'
cmake -S ../.. -B ../../build
cmake --build ../../build
ctest --test-dir ../../build --output-on-failure
```
